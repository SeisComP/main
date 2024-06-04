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

#include <seiscomp/plugins/qc/qcbuffer.h>
#include <seiscomp/qc/qcprocessor_latency.h>
#include "qcplugin_latency.h"


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
namespace Seiscomp {
namespace Applications {
namespace Qc {
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
using namespace std;
using namespace Seiscomp::Processing;


#define REGISTERED_NAME "QcLatency"

IMPLEMENT_SC_CLASS_DERIVED(QcPluginLatency, QcPlugin, "QcPluginLatency");
ADD_SC_PLUGIN("Qc Parameter Latency", "GFZ Potsdam <seiscomp-devel@gfz-potsdam.de>", 0, 1, 0)
REGISTER_QCPLUGIN(QcPluginLatency, REGISTERED_NAME);
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
QcPluginLatency::QcPluginLatency(): QcPlugin() {
	_qcProcessor = new QcProcessorLatency();
	_qcProcessor->subscribe(this);

	_lastArrivalTime = Core::Time::GMT();

	_name = REGISTERED_NAME;
	_parameterNames.push_back("latency");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void QcPluginLatency::timeoutTask() {
	if ( _qcBuffer->empty() ) {
		SEISCOMP_DEBUG("qcLatency: Waveform buffer is empty");
		return;
	}

	auto qcp = new QcParameter();
	qcp->recordSamplingFrequency = -1;
	qcp->recordEndTime = Core::Time::GMT();

	// origin of previous buffer item was a real record
	if ( _qcBuffer->back()->recordSamplingFrequency != -1 ) {
		_lastArrivalTime = _qcBuffer->back()->recordEndTime;
	}

	qcp->recordStartTime = _lastArrivalTime;
	qcp->parameter = static_cast<double>(qcp->recordEndTime - qcp->recordStartTime);
	_qcBuffer->push_back(&_streamID, qcp);

	sendMessages(Core::Time());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
}
}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
