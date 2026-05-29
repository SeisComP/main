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


#ifndef SEISCOMP_AUTOLOC_CONFIG_H
#define SEISCOMP_AUTOLOC_CONFIG_H


#include <seiscomp/config/config.h>

#include <string>
#include <vector>
#include <map>
#include <set>

namespace Seiscomp {

namespace Autoloc {

typedef enum { GlobalNetwork, RegionalNetwork, LocalNetwork } NetworkType;

struct AutolocConfig {
	public:
		AutolocConfig() = default;

		// White list of allowed pick authors.
		// Also defines priority in descending order.
		std::vector<std::string> authors;

		// Our agency ID.
		//
		// Used to distinguish between "own" and "imported" objects
		// and to populate the creationInfo of newly created objects.
		std::string agencyID;

		// Our author ID.
		//
		// Used to distinguish between "own" and "imported" objects
		// and to populate the creationInfo of newly created objects.
		std::string author;

		// The name of the grid file
		//
		// This is a required parameter.
		std::string gridConfigFile{"@DATADIR@/scautoloc/grid.conf"};

		// The name of a station locations file
		//
		// This is an optional parameter; the global inventory is used by default.
		std::string stationLocationFile;

		// During cleanup() all pick objects older than maxAge
		// (in hours) are removed.
		// If this parameter is <= 0, cleanup() is disabled.
		double maxAge{6.0 * 3600.0};   // unit: s

		// time to keep origins in buffer
		double originKeep{86400};

		// time span within which we search for picks
		// which may indicate extraordinary activity
		double dynamicPickThresholdInterval{3600.0};  // unit: s

		// typically good RMS in our network
		double goodRMS{1.5};   // unit: s

		// maximum RMS of all used picks
		double maxRMS{3.5};   // unit: s

		// maximum residual of any pick to be used for location
		double maxResidualUse{7.0};   // unit: s

		// maximum residual of any pick to be kept associated
		double maxResidualKeep{21.0};   // unit: s

		// NOTE: maxRMS < maxResidualUse < maxResidualKeep
		// typically:
		//    maxResidualKeep = 3*maxResidualUse
		//    maxResidualUse  = 2*maxRMS


		// Use this depth if there is no depth resolution
		double defaultDepth{10.0};           // unit: km
		double defaultDepthStickiness{0.5};  // 0...1

		// Try to relocate an origin using the configured default depth.
		// If the resulting origin is "not much worse" than the free-depth
		// origin, fix the depth at the default depth.
		bool tryDefaultDepth{true};         // default true

		bool adoptManualDepth{false};

		// Minimum depth in case there is depth resolution
		double minimumDepth{5.0};          // uni: 5 km

		// maximum depth of origin, checked before sending
		double maxDepth{1000.0};

		// Max. secondary azimuthal gap
		double maxAziGapSecondary{360.0};

		// Maximum distance of stations used in computing
		// the azimuthal gap
		// double maxAziGapDist; unused

		// Max. distance of stations to be used
		double maxStaDist{180.0};

		// Default max. distance of stations to be used in
		// the nucleator
		// May be overridden by individual values per station.
		double defaultMaxNucDist{180.0};

		// Minimum signal-to-noise ratio for a pick to
		// be processed
		double minPickSNR{3.0};

		// Unless we have a clear definition of "affinity",
		// do not change this!
		double minPickAffinity{0.05};

		// Minimum number of picks for a normal origin
		// to be reported
		size_t minPhaseCount{6};

		// Minimum score for an origin to be reported;
		// this seems to be a more reliable criterion
		// than number of phases.
		double minScore{8.0};

		// Minimum station count for which we ignore PKP phases
		size_t minStaCountIgnorePKP{15};

		// if a pick can be associated to an origin with at
		// least this high a score, bypass the nucleator,
		// which will improve speed
		double minScoreBypassNucleator{40.0};

		// Maximum allowes "probability" for an origin to be a
		// fake event. Value between 0 and 1
		double maxAllowedFakeProbability{0.2};

		// EXPERIMENTAL!!!
		double distSlope{1.0};

		// EXPERIMENTAL!!!
		double maxRadiusFactor{1.0};

		// EXPERIMENTAL!!!
		NetworkType networkType{Autoloc::GlobalNetwork};

		double cleanupInterval{3600.0};

		double publicationIntervalTimeSlope{0.5};
		double publicationIntervalTimeIntercept{0.0};
		size_t publicationIntervalPickCount{20};

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

		// If true then manual picks are being used as automatics
		// picks are
		bool useManualPicks{false};

		// The pick log file
		bool        pickLogEnable{false};
		std::string pickLogFilePrefix;

		// locator profile, e.g. "iasp91", "tab" etc.
		std::string locatorProfile{"iasp91"};

		// The station configuration file
		std::string staConfFile{"@DATADIR@/scautoloc/station.conf"};

		// misc. experimental options
		bool aggressivePKP{true};
		bool reportAllPhases{true};
		bool useManualOrigins{false};
		bool allowRejectedPicks{false};
		bool adoptManualOriginsFixedDepth;
		bool useImportedOrigins{false};

		// enable the XXL feature
		bool xxlEnabled{false};

		// minimum absolute amplitude to flag a pick as XXL
		double xxlMinAmplitude{1000.0};     // default unit: nm/s

		// minimum SNR to flag a pick as XXL
		double xxlMinSNR{8.0};

		// ignore all picks within this time window
		// length (in sec) following an XXL pick
		double xxlDeadTime{120.0};

		// Minimum number of picks for an XXL origin
		// to be reported
		size_t xxlMinPhaseCount{4};   // default 4 picks
		double xxlMaxStaDist{10.0};         // unit: degrees
		double xxlMaxDepth{100};            // unit: km

		const Seiscomp::Config::Config* scconfig{nullptr};

		std::string amplTypeSNR {"snr"};
		std::string amplTypeAbs {"mb"};

	public:
		void dump() const;
};


}  // namespace Autoloc

}  // namespace Seiscomp

#endif
