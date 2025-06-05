/***************************************************************************
 * Copyright (C) GFZ Potsdam                                               *
 * All rights reserved.                                                    *
 *                                                                         *
 * GNU Affero General Public License Usage                                 *
 * This file may be used under the terms of the GNU Affero                 *
 * Public License version 3.0 as published by the Free Software Foundation *
 * and appearing in the file LICENSE included in the packaging of this     *
 * file. Please review the following information to ensure the GNU Affero  *
 * Public License version 3.0 requirements will be met:                    *
 * https://www.gnu.org/licenses/agpl-3.0.html.                             *
 ***************************************************************************/


#define SEISCOMP_COMPONENT QL2SC

#include "quakelink.h"
#include "app.h"

#include <seiscomp/logging/log.h>
#include <seiscomp/core/version.h>


using namespace std;

namespace Seiscomp {
namespace QL2SC {
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
QLClient::QLClient(int notificationID, const HostConfig *config, size_t backLog)
: IO::QuakeLink::Connection(), _notificationID(notificationID), _config(config)
, _backLog(backLog) {
	setLogPrefix("[QL " + _config->host + "] ");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
QLClient::~QLClient() {
	SEISCOMP_INFO("%sterminated, messages/bytes received: %lu/%lu",
	              _logPrefix.c_str(), _stats.messages, _stats.payloadBytes);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void QLClient::run() {
	SEISCOMP_INFO("%sconnecting to URL '%s'", _logPrefix.c_str(),
	              _config->url.c_str());
	_thread = thread(bind(&QLClient::listen, this));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void QLClient::join() {
	if ( _thread.joinable() ) {
		SEISCOMP_DEBUG("%swaiting for thread to terminate", _logPrefix.c_str());
		_thread.join();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
OPT(Core::Time) QLClient::lastUpdate() const {
	lock_guard l(_mutex);
	return _lastUpdate;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void QLClient::setLastUpdate(const Core::Time &time) {
	lock_guard l(_mutex);
	_lastUpdate = time;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void QLClient::processResponse(IO::QuakeLink::Response *response) {
	SEISCOMP_INFO("%sreceived message, size: %lu)", _logPrefix.c_str(),
	              (unsigned long)response->length);
	((App*)SCCoreApp)->feed(this, response);
	++_stats.messages;
	_stats.payloadBytes += response->length;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void QLClient::listen() {
	IO::QuakeLink::RequestFormat rf = _config->gzip ?
	                                 (_config->native ? IO::QuakeLink::rfGZNative : IO::QuakeLink::rfGZXML) :
	                                 (_config->native ? IO::QuakeLink::rfNative : IO::QuakeLink::rfXML);
	// activate socket timeout if keepAlive was requested
	if ( _config->options & IO::QuakeLink::opKeepAlive ) {
		if ( _sock ) {
			_sock->setTimeout(60);
		}
		else {
			SEISCOMP_ERROR("%sinstance not initialized", _logPrefix.c_str());
			return;
		}
	}

	while ( !interrupted() ) {
		// determine start time of request
		OPT(Core::Time) from;
		string filter = _config->filter;
		if ( _backLog > 0 ) {
			Core::Time minTime = Core::Time::UTC() - Core::TimeSpan(_backLog, 0);
			from = lastUpdate();
			if ( !from || *from < minTime ) {
				from = minTime;
			}
			if ( !filter.empty() ) {
				filter += " AND ";
			}
			filter += "UPDATED > " + from->toString(IO::QuakeLink::RequestTimeFormat);
		}

		// start request
		try {
			select(from.has_value(), Core::Time(), Core::Time(), rf, filter);
		}
		catch ( Core::GeneralException &e ) {
			if ( interrupted() ) {
				break;
			}
			SEISCOMP_DEBUG("%sselect exception: %s", _logPrefix.c_str(), e.what());
		}

		_sock->close(); // clears interrupt flag
		SEISCOMP_WARNING("%sQuakeLink connection closed, trying to reconnect "
		                 "in 5s", _logPrefix.c_str());

		for ( int i = 0; i < 50; ++i ) {
			usleep(100000); // 100ms

			if ( interrupted() ) {
				break;
			}
		}
	}

	if ( interrupted() ) {
		SEISCOMP_INFO("%sQuakeLink connection interrupted", _logPrefix.c_str());
	}

	_sock->close();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
} // ns QL2SC
} // ns Seiscomp
