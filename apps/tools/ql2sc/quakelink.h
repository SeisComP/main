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


#ifndef SEISCOMP_QL2SC_QUAKELINK_H__
#define SEISCOMP_QL2SC_QUAKELINK_H__


#include "config.h"

#include <seiscomp/core/datetime.h>
#include <seiscomp/io/quakelink/connection.h>

#include <boost/thread.hpp>

#include <string>
#include <vector>


namespace Seiscomp {
namespace QL2SC {


class QLClient : public IO::QuakeLink::Connection {

	public:
		QLClient(int notificationID, const HostConfig *config, size_t backLog = 0);
		virtual ~QLClient();

		void run();
		void join(const Seiscomp::Core::Time &until);

		const HostConfig* config() const { return _config; }

		Seiscomp::Core::Time lastUpdate() const;
		void setLastUpdate(const Seiscomp::Core::Time &time);

		int notificationID() const { return _notificationID; }

	protected:
		void processResponse(IO::QuakeLink::Response *response);

	private:
		void listen();

	private:
		struct Statistics {
			Statistics() : messages(0), payloadBytes(0) {};
			size_t messages;
			size_t payloadBytes;
		};

		int                         _notificationID;
		const HostConfig           *_config;
		size_t                      _backLog;
		boost::thread              *_thread;

		Seiscomp::Core::Time        _lastUpdate;
		mutable boost::mutex        _mutex;
		Statistics                  _stats;
};


} // ns QL2SC
} // ns Seiscomp


#endif // SEISCOMP_QL2SC_QUAKELINK_H__
