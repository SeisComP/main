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


#define SEISCOMP_COMPONENT SCQC
#include <seiscomp/logging/log.h>

#include <seiscomp/core/system.h>
#include <seiscomp/messaging/connection.h>
#include <seiscomp/utils/timer.h>
#include <seiscomp/datamodel/outage.h>

#include <boost/visit_each.hpp>
#include <functional>

#include "qcmessenger.h"

#ifndef WIN32
#include <unistd.h>
#endif


namespace Seiscomp {
namespace Applications {
namespace Qc {


using namespace std;
using namespace Seiscomp;
using namespace Seiscomp::Core;
using namespace Seiscomp::Client;
using namespace Seiscomp::DataModel;
using namespace Seiscomp::IO;

const char *const myGroup = "Qc";
const char *const myPackage = "QualityControl";


namespace {

//! converts object to QcIndex
// poorly conceived, this ... looking for a more general solution.
QcIndex toIndex(const DataModel::Object *obj) {
	QcIndex index;
	if ( obj == NULL ) return index;

	const DataModel::WaveformQuality *wfq = DataModel::WaveformQuality::ConstCast(obj);
	if ( wfq ) {
		const WaveformQualityIndex &idx = wfq->index();
		return QcIndex(idx.waveformID.networkCode() + "." +
		               idx.waveformID.stationCode() + "." +
		               idx.waveformID.locationCode() +"." +
		               idx.waveformID.channelCode() + "-" +
		               idx.type + "-" +
		               idx.parameter,
		               wfq->start());
	}

	const DataModel::Outage *outage = DataModel::Outage::ConstCast(obj);
	if (outage) {
		const OutageIndex& idx = outage->index();
		return QcIndex(idx.waveformID.networkCode() + "." +
		               idx.waveformID.stationCode() + "." +
		               idx.waveformID.locationCode() + "." +
		               idx.waveformID.channelCode(),
		               outage->start());
	}

	return index;
}


}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// Synchronize with scmaster every 100 messages
#define SYNC_COUNT 100
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
QcMessenger::QcMessenger(QcApp *app) : _app(app) {
	QcApp::TimerSignal::slot_type slot = bind(&QcMessenger::scheduler, this);
	_app->addTimeout(slot);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void QcMessenger::setTestMode(bool enable) {
	_testMode = enable;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool QcMessenger::attachObject(DataModel::Object *obj, bool notifier, Operation operation) {
	// send notifier msg
	if ( notifier ) {
		if ( operation == OP_UNDEFINED ) {
			QcIndex idx = toIndex(obj);
			if ( _qcIndex.find(idx) ) {
				//cerr << _qcIndex.size() << "   found QcIndex: " << idx.key << "-" << idx.startTime.iso() << endl; //! DEBUG
				operation = OP_UPDATE;
			}
			else {
				//cerr << _qcIndex.size() << "   did not find QcIndex: " << idx.key << "-" << idx.startTime.iso() << endl; //! DEBUG
				operation = OP_ADD;
				_qcIndex.insert(idx);
			}
		}

		if ( !_notifierMsg ) _notifierMsg = new NotifierMessage;
		NotifierPtr notifier = new Notifier(myPackage, operation, obj);
		_notifierMsg->attach(notifier);

	}
	// send data msg
	else {
		if ( !_dataMsg ) {
			_dataMsg = new DataMessage;
		}
		_dataMsg->attach(obj);
	}

	// let scheduler decide, when to send the message
	scheduler();

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void QcMessenger::scheduler() {
	bool msgSent = false;

	if ( _notifierMsg ) {
		try {
			if ( ((_timer.elapsed() > _sendInterval) && (_notifierMsg->size() > 0))
			  || (_notifierMsg->size() >=_maxSize) ) {
// 				SEISCOMP_DEBUG("sending Qc NOTIFIER message with %d attachments", _notifierMsg->size());
				sendMessage(_notifierMsg.get());
				msgSent = true;
			}
		}
		catch ( ... ) { //FIXME error handling
			if ( _notifierMsg->size() > 2000 ) {
				_notifierMsg->clear();
				SEISCOMP_ERROR("Notifier message buffer overflow! Buffer cleared!");
			}
		}
	}

	if ( _dataMsg ) {
		try {
			if ( ((_timer.elapsed() > _sendInterval) && (_dataMsg->size() > 0)) || (_dataMsg->size() >=_maxSize) ) {
// 				SEISCOMP_DEBUG("sending Qc DATA message with %d attachments", _dataMsg->size());
				sendMessage(_dataMsg.get());
				msgSent = true;
			}
		}
		catch (...){ //FIXME error handling
			if ( _dataMsg->size() > 2000 ) {
				_dataMsg->clear();
				SEISCOMP_ERROR("Data message buffer overflow! Buffer cleared!");
			}
		}
	}

	if ( msgSent ) {
		_timer.restart();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void QcMessenger::flushMessages(){
	Core::TimeSpan tmp = _sendInterval;
	_sendInterval = -1.0;
	scheduler();
	_sendInterval = tmp;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool QcMessenger::sendMessage(Message *msg) {
	Client::Connection *con = _app->connection();
	if ( msg && msg->size() > 0 ) {
		if ( !_testMode && !con->send(msg) ) {
			throw ConnectionException("Could not send QC message");
		}

		msg->clear();

		return true;
	}

	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
}
}
}

