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


#define SEISCOMP_COMPONENT Autoloc

#include <seiscomp/logging/log.h>
#include "datamodel.h"
#include "util.h"
#include "picklog.h"


namespace Seiscomp {

namespace AutolocInternal {


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void PickLog::setPrefix(const std::string &p) {
	prefix = p;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void PickLog::setEnabled(bool e) {
	enabled = e;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void PickLog::setFileName(const std::string &f)
{
	if ( file.is_open() ) {
		if ( f == filename ) {
			return;
		}

		file.close();
	}

	if ( f.empty() ) {
		return;
	}

	filename = f;

	file.open(filename.c_str(), std::ios_base::app);
	if ( !file.is_open() ) {
		SEISCOMP_ERROR("Failed to open pick log file %s", f);
		return;
	}

	SEISCOMP_INFO("Logging picks to file %s", f);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void PickLog::log(const Pick *pick) {

	if ( !enabled ) {
		return;
	}

	const DataModel::WaveformStreamID &wfid = pick->scpick->waveformID();
	char line[200];
	const std::string &net = wfid.networkCode();
	const std::string &sta = wfid.stationCode();
	const std::string  loc = wfid.locationCode() == "" ? "__" : wfid.locationCode();
	const std::string &cha = wfid.channelCode();
	const std::string tstr = pick->scpick->time().value().toString("%F %T.%f000000").substr(0, 21);
	snprintf(line, 200, "%s %-2s %-6s %-3s %-2s %6.1f %10.3f %4.1f %c %s",
	      tstr.c_str(), net.c_str(), sta.c_str(), cha.c_str(), loc.c_str(),
	      pick->snr, pick->amp, pick->per, modeFlag(pick),
	      pick->label.c_str());

	if ( prefix.empty() ) {
		SEISCOMP_WARNING("Pick log enabled but no file name prefix specified - disabling pick log");
		enabled = false;
		return;
	}
	else {
		Core::Time now = Core::Time::UTC();
		setFileName(prefix + "." + now.toString("%F"));
	}

	if ( file.good() ) {
		file << line << std::endl;
	}

	SEISCOMP_INFO("%s", line);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


}  // namespace AutolocInternal

}  // namespace Seiscomp
