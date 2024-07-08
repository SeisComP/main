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

#include <seiscomp/datamodel/outage.h>
#include <seiscomp/plugins/qc/qcconfig.h>
#include <seiscomp/plugins/qc/qcmessenger.h>
#include <seiscomp/qc/qcprocessor_outage.h>
#include "qcplugin_outage.h"


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
namespace Seiscomp {
namespace Applications {
namespace Qc {


using namespace std;
using namespace Seiscomp::Processing;
using namespace Seiscomp::DataModel;


#define REGISTERED_NAME "QcOutage"

IMPLEMENT_SC_CLASS_DERIVED(QcPluginOutage, QcPlugin, "QcPluginOutage");
ADD_SC_PLUGIN("Qc Outage", "GFZ Potsdam <seiscomp-devel@gfz-potsdam.de>", 0, 1, 0)
REGISTER_QCPLUGIN(QcPluginOutage, REGISTERED_NAME);
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
QcPluginOutage::QcPluginOutage(): QcPlugin() {
	_qcProcessor = new QcProcessorOutage();
	_qcProcessor->subscribe(this);

	_name = REGISTERED_NAME;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool QcPluginOutage::init(QcApp* app, QcConfig *cfg, string streamID) {
	if ( !QcPlugin::init(app,cfg,streamID) ) {
		return false;
	}

	string value = _qcConfig->readConfig(_name,"notifyDB","1800");
	QcProcessorOutage::Cast(_qcProcessor.get())->setThreshold(boost::lexical_cast<int>(value));

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void QcPluginOutage::update() {
	QcParameterPtr qcp = _qcProcessor->getState();

	if ( _qcProcessor->isSet() ) {
		fillUp(qcp.get());
	}

	if ( _qcProcessor->isValid() ) {
		Core::Time end = qcp->recordStartTime;
		Core::Time start = (Core::Time)(end - Core::Time(QcProcessorOutage::Cast(_qcProcessor.get())->getOutage()));

		SEISCOMP_DEBUG("got outage: %s %s-%s",_streamID.c_str(),start.iso().c_str(),end.iso().c_str());

		Outage *obj = new Outage();
		obj->setWaveformID(getWaveformID(_streamID));
		obj->setCreatorID(_app->creatorID());
		obj->setCreated(Time::GMT());
		obj->setStart(start);
		obj->setEnd(end);
		_qcMessenger->attachObject(obj,true,OP_ADD);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool QcPluginOutage::fillUp(const QcParameter *qcp) {
	bool result = false;
	Core::Time recStart = qcp->recordStartTime;

	if ( _recent[_streamID] == Core::Time() || _recent[_streamID] > recStart ) {
		Core::Time recEnd = qcp->recordEndTime;
		DatabaseIterator dbiter = _app->query()->getOutage(getWaveformID(_streamID),recStart,recEnd);
		if ( *dbiter ) {
			OutagePtr pout = Outage::Cast(*dbiter);
			if ( !pout ) {
				SEISCOMP_ERROR("Got unexpected object from getOutage()");
			}
			else {
				Core::Time outStart = pout->start();
				Core::Time outEnd = pout->end();

				Outage *obj1 = new Outage();
				obj1->setWaveformID(getWaveformID(_streamID));
				obj1->setCreatorID(_app->creatorID());
				obj1->setCreated(Time::GMT());

				Outage *obj2 = new Outage();
				obj2->setWaveformID(getWaveformID(_streamID));
				obj2->setCreatorID(_app->creatorID());
				obj2->setCreated(Time::GMT());

				if ( outStart != recStart ) {
					obj1->setStart(outStart);
					obj1->setEnd(recStart);
					_qcMessenger->attachObject(obj1,true,OP_UPDATE);

					obj2->setStart(recEnd);
					obj2->setEnd(outEnd);
					_qcMessenger->attachObject(obj2,true,OP_ADD);
				}
				else {
					obj1->setStart(outStart);
					obj1->setEnd(outEnd);
					_qcMessenger->attachObject(obj1,true,OP_REMOVE);

					obj2->setStart(recEnd);
					obj2->setEnd(outEnd);
					_qcMessenger->attachObject(obj2,true,OP_ADD);
				}
				result = true;
			}
		}
		dbiter.close();
	}

	if ( _recent[_streamID] < recStart ) {
		_recent[_streamID] = recStart;
	}

	return result;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
}
}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
