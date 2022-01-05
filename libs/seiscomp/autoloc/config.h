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


#ifndef SEISCOMP_LIBAUTOLOC_CONFIG_H_INCLUDED
#define SEISCOMP_LIBAUTOLOC_CONFIG_H_INCLUDED

#include <vector>
#include <string>

namespace Seiscomp {

namespace Autoloc {


typedef std::vector<std::string> StringList;


typedef enum { GlobalNetwork, RegionalNetwork, LocalNetwork } NetworkType;


class Config {

	public:
		void dump() const;

		// Checks the config for completeness/consistency
		// of those config options that usually need to
		// be configured explicitly.
		//
		// Use this to enforce proper configuration rather
		// than rely on defaults like "TEST".
		//
		// Returns true if complete, false otherwise
		bool check() const;

	public:
		// The ID of our agency. E.g. "GFZ", "NEIC" etc.
		// Used for the filtering of objects meant to be
		// processed by us. If left blank then no filtering
		// based on agency is performed.
		//
		// This is a required parameter.
		std::string agencyID{"TEST"};

		// The author ID that will be set in all generated
		// objects.
		//
		// This is a required parameter.
		std::string author{"AUTOLOC"};

		// Amplitude types for absolute amplitudes and SNR
		// amplitudes. These are used to improve the
		// processing. They can in principle be any type but
		// by default they are "mb" and "snr", respectively.
		// There is normally no need to change this.
		std::string amplTypeAbs{"mb"};
		std::string amplTypeSNR{"snr"};

		// White list of allowed pick authors.
		// Also defines priority in descending order.
		//
		// If not configured, then all picks from the specified
		// agency are used.
		StringList pickAuthors;

		// During cleanup() all objects older than maxAge
		// (in hours) are removed.
		// If this parameter is <= 0, cleanup() is disabled.
		double maxAge{6*3600};

		// Time span within which we search for picks which
		// may indicate extraordinary activity.
		//
		// TODO: review required
		double dynamicPickThresholdInterval{3600};

		// Typically good RMS in our network in seconds
		double goodRMS{1.5};

		// Maximum RMS of all used picks in seconds
		double maxRMS{3.5};

		// Maximum residual of any pick to be used for location
		// in seconds
		double maxResidualUse{2*maxRMS};

		// Maximum residual of any pick to be kept associated
		// in seconds
		double maxResidualKeep{3*maxResidualUse};

		// NOTE: maxRMS < maxResidualUse < maxResidualKeep
		// typically:
		//    maxResidualKeep = 3*maxResidualUse
		//    maxResidualUse  = 2*maxRMS

		// Use this depth if there is no depth resolution
		double defaultDepth{10.};
		double defaultDepthStickiness{0.5}; // 0...1

		// Try to relocate an origin using the configured
		// default depth. If the resulting origin is "not much
		// worse" than the free-depth origin, fix the depth at
		// the default depth.
		bool tryDefaultDepth{true};

		// If a matching manual origin exists, subsequent automatic
		// origins with adopth the depth of the manual origin.
		bool adoptManualDepth{false};

		// Minimum depth in case there is depth resolution
		double minimumDepth{5.};

		// Maximum depth of origin, checked before sending,
		// default is 1000 km
		double maxDepth{1000.};

		// Max. secondary azimuthal gap
		double maxAziGapSecondary{360.};

		// Maximum distance of stations used in computing
		// the azimuthal gap
		//double maxAziGapDist{};

		// Max. distance of stations to be used
		double maxStaDist{180.};

		// Default max. distance of stations to be used in
		// the nucleator
		// May be overridden by individual values per station.
		double defaultMaxNucDist{180.};

		// Minimum signal-to-noise ratio for a pick to
		// be processed
		double minPickSNR{3};

		// Unless we have a clear definition of "affinity",
		// do not change this!
		double minPickAffinity{0.05};

		// Minimum number of picks for a normal origin
		// to be reported
		int minPhaseCount{6};

		// Minimum score for an origin to be reported;
		// this seems to be a more reliable criterion
		// than number of phases.
		double minScore{8.};

		// Minimum station count for which we ignore PKP phases
		// XXX not yet used
		int minStaCountIgnorePKP{15};

		// if a pick can be associated to an origin with at
		// least this high a score, bypass the nucleator,
		// which will improve speed
		double minScoreBypassNucleator{40};

		// Maximum allowes "probability" for an origin to be a
		// fake event. Value between 0 and 1
		double maxAllowedFakeProbability{0.2};

		// EXPERIMENTAL!!!
		double distSlope{1.};

		// EXPERIMENTAL!!!
		//double maxRadiusFactor;

		// EXPERIMENTAL!!!
		//NetworkType networkType;

		double cleanupInterval{3600};

		double publicationIntervalTimeSlope{0.5};
		double publicationIntervalTimeIntercept{0.};
		int    publicationIntervalPickCount{20};

		// If true, offline mode is selected. In offline mode,
		// no database is accessed, and station locations are
		// read from a plain text file.
		bool offline{false};

		// If true, test mode is selected. In test mode, no
		// origins are sent. This is not the same as offline
		// mode. Test mode is like normal online mode except
		// no origins are sent (only logging output is
		// produced).
		bool test{false};

		// If true, playback mode is selected. In playback mode,
		// origins are sent immediately without delay.
		bool playback{false};

		// If true then manual picks are used, too
		bool useManualPicks{false};

		// locator profile, e.g. "iasp91", "tab" etc.
		std::string locatorProfile{"iasp91"};

		// The station configuration file
		std::string stationConfig;

		// misc. experimental options
		bool aggressivePKP{true};
		bool reportAllPhases{false};
		bool useManualOrigins{false};
//		bool adoptManualOriginsFixedDepth{true};
		bool useImportedOrigins{false};

		// enable the XXL feature
		bool xxlEnabled{false};

		// minimum absolute amplitude to flag a pick as XXL
		double xxlMinAmplitude{5000};

		// minimum SNR to flag a pick as XXL
		double xxlMinSNR{20};

		// ignore all picks within this time window
		// length (in sec) following an XXL pick
		double xxlDeadTime{120};

		// Minimum number of picks for an XXL origin
		// to be reported
		unsigned int xxlMinPhaseCount{4};
		double xxlMaxStaDist{15};
		double xxlMaxDepth{100};
};


}  // namespace Autoloc

}  // namespace Seiscomp

#endif
