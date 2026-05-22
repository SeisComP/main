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
void Autoloc3::Config::dump() const {
	SEISCOMP_INFO("Configuration:");
	SEISCOMP_INFO("  locator");
	SEISCOMP_INFO("    profile                          %s",     locatorProfile);
	SEISCOMP_INFO("    default depth                    %g km",  defaultDepth);
	SEISCOMP_INFO("    minimum depth                    %g km",  minimumDepth);
	SEISCOMP_INFO("  buffer");
	SEISCOMP_INFO("    picks kept in buffer             %.0f s", maxAge);
	SEISCOMP_INFO("    origins kept in buffer           %.0f s", originKeep);
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
	SEISCOMP_INFO("    use manual picks                 %s",     useManualPicks ? "true":"false");
	SEISCOMP_INFO("    use manual origins               %s",     useManualOrigins ? "true":"false");
	SEISCOMP_INFO("    useImportedOrigins               %s",     useImportedOrigins ? "true":"false");
	SEISCOMP_INFO("    publicationIntervalTimeSlope     %.2f",   publicationIntervalTimeSlope);
	SEISCOMP_INFO("    publicationIntervalTimeIntercept %.1f",   publicationIntervalTimeIntercept);
	SEISCOMP_INFO("    publicationIntervalPickCount     %d",     publicationIntervalPickCount);
	SEISCOMP_INFO("    reportAllPhases                  %s",     reportAllPhases ? "true":"false");
	SEISCOMP_INFO("    pickLogFile                      %s",     pickLogFile.size() ? pickLogFile : "pick logging is disabled");
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
