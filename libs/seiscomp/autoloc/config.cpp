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

#include <seiscomp/autoloc/autoloc.h>
#include <seiscomp/logging/log.h>


namespace Seiscomp {

namespace Autoloc {


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Config::Config()
{
	maxAge = 6*3600;
	goodRMS = 1.5;
	maxRMS  = 3.5;
	dynamicPickThresholdInterval = 3600;
	maxResidualUse = 7.0;
	maxResidualKeep = 3*maxResidualUse;
	maxAziGapSecondary = 360; // 360 means no SGAP restriction
	maxStaDist = 180;
	defaultMaxNucDist = 180;
	minPhaseCount = 6;
	minScore = 8;
	defaultDepth = 10;
	defaultDepthStickiness = 0.5;
	tryDefaultDepth = true;
	adoptManualDepth = false;
	minimumDepth = 5;
	maxDepth = 1000.0; // so effectively by default there is no maximum depth
	minPickSNR = 3;
	minPickAffinity = 0.05;
	minStaCountIgnorePKP = 15;
	minScoreBypassNucleator = 40;
	maxAllowedFakeProbability = 0.2; // TODO make this configurable
	distSlope = 1.0; // TODO: Make this configurable after testing
	test = false;
	offline = false;
	playback = false;
	useManualPicks = false;
	cleanupInterval = 3600;
	aggressivePKP = true;
	useManualOrigins = false;
	useImportedOrigins = false;
	// If true, more exotic phases like PcP, SKKP etc. will be reported.
	// By default, only P/PKP will be reported. Internally, of course, the
	// other phases are also associated to avoid fakes. They also show up
	// in the log files
	// FIXME: Note that this is a temporary HACK
	reportAllPhases = false;

	maxRadiusFactor = 1;
	networkType = Autoloc::GlobalNetwork;

	publicationIntervalTimeSlope = 0.5;
	publicationIntervalTimeIntercept = 0;
	publicationIntervalPickCount = 20;

	xxlEnabled = false;
	xxlMinSNR = 20.;
	xxlMinAmplitude = 5000.;
	xxlMinPhaseCount = 4;
	xxlMaxStaDist = 15;
	xxlMaxDepth = 100;
	xxlDeadTime = 120.;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Config::dump() const
{
	SEISCOMP_INFO("Configuration:");
	SEISCOMP_INFO("  agencyID                    %8s",         agencyID.c_str());
	SEISCOMP_INFO("  amplTypeAbs                 %8s",         amplTypeAbs.c_str());
	SEISCOMP_INFO("  amplTypeSNR                 %8s",         amplTypeSNR.c_str());
	SEISCOMP_INFO("  defaultDepth                     %g",     defaultDepth);
	SEISCOMP_INFO("  defaultDepthStickiness           %g",     defaultDepthStickiness);
	SEISCOMP_INFO("  tryDefaultDepth                  %s",     tryDefaultDepth ? "true":"false");
	SEISCOMP_INFO("  adoptManualDepth                 %s",     adoptManualDepth ? "true":"false");
	SEISCOMP_INFO("  minimumDepth                     %g",     minimumDepth);
	SEISCOMP_INFO("  minPhaseCount                    %d",     minPhaseCount);
	SEISCOMP_INFO("  minScore                         %.1f",   minScore);
	SEISCOMP_INFO("  minPickSNR                       %.1f",   minPickSNR);
	SEISCOMP_INFO("  maxResidual                      %.1f s", maxResidualUse);
	SEISCOMP_INFO("  goodRMS                          %.1f s", goodRMS);
	SEISCOMP_INFO("  maxRMS                           %.1f s", maxRMS);
	SEISCOMP_INFO("  maxDepth                         %.1f km", maxDepth);
	SEISCOMP_INFO("  minStaCountIgnorePKP             %d",     minStaCountIgnorePKP);
	SEISCOMP_INFO("  pickKeep                         %.0f s", maxAge);
	SEISCOMP_INFO("  publicationIntervalTimeSlope     %.2f",   publicationIntervalTimeSlope);
	SEISCOMP_INFO("  publicationIntervalTimeIntercept %.1f",   publicationIntervalTimeIntercept);
	SEISCOMP_INFO("  publicationIntervalPickCount     %d",     publicationIntervalPickCount);
	SEISCOMP_INFO("  reportAllPhases                  %s",     reportAllPhases ? "true":"false");
//	SEISCOMP_INFO("  pickLogFile                      %s",     pickLogFile.size() ? pickLogFile.c_str() : "pick logging is disabled");
	SEISCOMP_INFO("  dynamicPickThresholdInterval     %g",     dynamicPickThresholdInterval);
	SEISCOMP_INFO("  offline                          %s",     offline ? "true":"false");
	SEISCOMP_INFO("  test                             %s",     test ? "true":"false");
	SEISCOMP_INFO("  playback                         %s",     playback ? "true":"false");
	SEISCOMP_INFO("  useManualPicks                   %s",     useManualPicks ? "true":"false");
	SEISCOMP_INFO("  useManualOrigins                 %s",     useManualOrigins ? "true":"false");
// This isn't used still so we don't want to confuse the user....
//	SEISCOMP_INFO("useImportedOrigins               %s",     useImportedOrigins ? "true":"false");
	SEISCOMP_INFO("  locatorProfile                   %s",     locatorProfile.c_str());

	if ( ! xxlEnabled) {
		SEISCOMP_INFO("  XXL feature is not enabled");
		return;
	}
	SEISCOMP_INFO("  XXL feature is enabled");
	SEISCOMP_INFO("  xxl.minPhaseCount                 %d",     xxlMinPhaseCount);
	SEISCOMP_INFO("  xxl.minAmplitude                  %g",     xxlMinAmplitude);
	SEISCOMP_INFO("  xxl.minSNR                        %g",     xxlMinSNR);
	SEISCOMP_INFO("  xxl.maxStationDistance           %.1f deg", xxlMaxStaDist);
	SEISCOMP_INFO("  xxl.maxDepth                      %g km",  xxlMaxDepth);
	SEISCOMP_INFO("  xxl.deadTime                      %g s",  xxlDeadTime);
//	SEISCOMP_INFO("maxRadiusFactor                  %g", 	 maxRadiusFactor);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


}  // namespace Autoloc

}  // namespace Seiscomp
