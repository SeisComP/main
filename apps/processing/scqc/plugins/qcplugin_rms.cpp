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
#include <seiscomp/plugins/qc/qcconfig.h>
#include <seiscomp/qc/qcprocessor_rms.h>
#include "qcplugin_rms.h"


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
namespace Seiscomp {
namespace Applications {
namespace Qc {


using namespace std;
using namespace Seiscomp::Processing;


#define REGISTERED_NAME "QcRms"

IMPLEMENT_SC_CLASS_DERIVED(QcPluginRms, QcPlugin, "QcPluginRms");
ADD_SC_PLUGIN("Qc Parameter Rms", "GFZ Potsdam <seiscomp-devel@gfz-potsdam.de>", 0, 1, 0)
REGISTER_QCPLUGIN(QcPluginRms, REGISTERED_NAME);
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
QcPluginRms::QcPluginRms() {
	_qcProcessor = new QcProcessorRms();
	_qcProcessor->subscribe(this);

	_name = REGISTERED_NAME;
	_parameterNames.push_back("rms");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool QcPluginRms::init(QcApp *app, QcConfig *cfg, string streamID) {
	string value = cfg->readConfig(_name, "filter", "");
	if ( ! value.empty() ) {
		auto f = Processing::WaveformProcessor::Filter::Create(value);
		if ( f ) {
			_qcProcessor.get()->setFilter(f);
		}
		else {
			SEISCOMP_ERROR("Could not create waveform filter from string: %s",
			               value.c_str());
			return false;
		}
	}

	return QcPlugin::init(app,cfg,streamID);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
}
}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
