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

#include "autoloc.h"

using namespace std;

namespace Autoloc {


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
StationConfig::StationConfig() {
	string defaultkey = "* *";
	_entry[defaultkey] = Entry();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool StationConfig::read(const std::string &fname) {
	string line;
	string defaultkey = "* *";
	_entry[defaultkey] = Entry();

	ifstream ifile(fname.c_str());
	if ( !ifile.good() ) {
		SEISCOMP_ERROR_S("Failed to open station config file "+fname);
		return false;
	}

	Entry entry;
	while( !ifile.eof() ) {
		getline(ifile, line);
		line.erase(0, line.find_first_not_of(" \n\r\t"));
		if ( line[0] == '#' ) {
			continue;
		}
		char net[10], sta[10];
		int n = sscanf(line.c_str(), "%8s %8s %d %f", net, sta, &entry.usage, &entry.maxNucDist);
		if ( n != 4 ) {
			break;
		}
		string key = string(net) + string(" ") + string(sta);
		_entry[key] = entry;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const StationConfig::Entry&
StationConfig::get(const string &net, const string &sta) const {
	vector<string> patterns;
	patterns.push_back(net + " " + sta);
	patterns.push_back(net + " *");
	patterns.push_back("* " + sta);
	patterns.push_back("* *");

	for ( const string& pattern : patterns ) {
		auto mit = _entry.find(pattern);
		if ( mit == _entry.end() ) {
			continue;
		}

		const Entry &e = (*mit).second;
		SEISCOMP_DEBUG("Station %s %s  pattern %-8s config: usage=%d maxnucdist=%g",
		               net.c_str(), sta.c_str(), pattern.c_str(), e.usage, e.maxNucDist);

		return e;
	}

	return _entry.begin()->second;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Autoloc3::Config::dump() const {
	SEISCOMP_INFO("Configuration:");
	SEISCOMP_INFO("  locator");
	SEISCOMP_INFO("    profile                          %s",     locatorProfile.c_str());
	SEISCOMP_INFO("    defaultDepth                     %g km",  defaultDepth);
	SEISCOMP_INFO("    minimumDepth                     %g km",  minimumDepth);
	SEISCOMP_INFO("  buffer");
	SEISCOMP_INFO("    pickKeep                         %.0f s", maxAge);
	SEISCOMP_INFO("  autoloc");
	SEISCOMP_INFO("    maxRMS                           %.1f s", maxRMS);
	SEISCOMP_INFO("    maxResidual                      %.1f s", maxResidualUse);
	SEISCOMP_INFO("    maxResidual for keeping picks    %.1f s", maxResidualKeep);
	SEISCOMP_INFO("    minPhaseCount                    %d",     minPhaseCount);
	SEISCOMP_INFO("    maxDepth                         %.1f km", maxDepth);
	SEISCOMP_INFO("    minStaCountIgnorePKP             %d",     minStaCountIgnorePKP);
	SEISCOMP_INFO("    defaultDepthStickiness           %g",     defaultDepthStickiness);
	SEISCOMP_INFO("    tryDefaultDepth                  %s",     tryDefaultDepth ? "true":"false");
	SEISCOMP_INFO("    adoptManualDepth                 %s",     adoptManualDepth ? "true":"false");
	SEISCOMP_INFO("    minScore                         %.1f",   minScore);
	SEISCOMP_INFO("    minPickSNR                       %.1f",   minPickSNR);
	SEISCOMP_INFO("    goodRMS                          %.1f s", goodRMS);
	SEISCOMP_INFO("    useManualPicks                   %s",     useManualPicks ? "true":"false");
	SEISCOMP_INFO("    useManualOrigins                 %s",     useManualOrigins ? "true":"false");
	SEISCOMP_INFO("    useImportedOrigins               %s",     useImportedOrigins ? "true":"false");
	SEISCOMP_INFO("    publicationIntervalTimeSlope     %.2f",   publicationIntervalTimeSlope);
	SEISCOMP_INFO("    publicationIntervalTimeIntercept %.1f",   publicationIntervalTimeIntercept);
	SEISCOMP_INFO("    publicationIntervalPickCount     %d",     publicationIntervalPickCount);
	SEISCOMP_INFO("    reportAllPhases                  %s",     reportAllPhases ? "true":"false");
	SEISCOMP_INFO("    pickLogFile                      %s",     pickLogFile.size() ? pickLogFile.c_str() : "pick logging is disabled");
	SEISCOMP_INFO("    dynamicPickThresholdInterval     %g",     dynamicPickThresholdInterval);
	SEISCOMP_INFO("  offline                            %s",     offline ? "true":"false");
	SEISCOMP_INFO("  test                               %s",     test ? "true":"false");
	SEISCOMP_INFO("  playback                           %s",     playback ? "true":"false");
// This isn't used still so we don't want to confuse the user....
//	SEISCOMP_INFO("useImportedOrigins               %s",     useImportedOrigins ? "true":"false");

	if ( ! xxlEnabled) {
		SEISCOMP_INFO("  XXL feature is not enabled");
		return;
	}
	SEISCOMP_INFO("  XXL feature is enabled");
	SEISCOMP_INFO("  xxl.minPhaseCount                 %d",     xxlMinPhaseCount);
	SEISCOMP_INFO("  xxl.minAmplitude                  %g",     xxlMinAmplitude);
	SEISCOMP_INFO("  xxl.maxStationDistance           %.1f deg", xxlMaxStaDist);
	SEISCOMP_INFO("  xxl.maxDepth                      %g km",  xxlMaxDepth);
	SEISCOMP_INFO("  xxl.deadTime                      %g s",  xxlDeadTime);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


}  // namespace Autoloc
