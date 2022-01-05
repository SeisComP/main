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

#include <seiscomp/autoloc/config.h>
#include <seiscomp/logging/log.h>


namespace Seiscomp {

namespace Autoloc {


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Config::dump() const
{
	SEISCOMP_INFO("Configuration:");
	SEISCOMP_INFO("  agencyID                    %8s",         agencyID.c_str());
	SEISCOMP_INFO("  author                      %8s",         author.c_str());
	SEISCOMP_INFO("  pickAuthors with priority");
	for(size_t i=0; i<pickAuthors.size(); i++)
		SEISCOMP_INFO("                  %2ld  %s",
			      pickAuthors.size()-i, pickAuthors[i].c_str());
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
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Config::check() const
{
	Config defaultConfig;

	std::vector<std::string> unconfigured;

	if (author == defaultConfig.author)
		unconfigured.push_back("author");
	if (agencyID == defaultConfig.agencyID)
		unconfigured.push_back("agencyID");
	if (stationConfig.empty())
		unconfigured.push_back("stationConfig");

	if (unconfigured.empty())
		return true;

	SEISCOMP_WARNING("Autoloc::Config detected default settings for");
	for (const std::string &s: unconfigured)
		SEISCOMP_WARNING_S("  "+s);
	SEISCOMP_WARNING("It is unlikely that these shall be left unconfigured");

	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


}  // namespace Autoloc

}  // namespace Seiscomp
