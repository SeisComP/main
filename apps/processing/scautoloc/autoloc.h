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


#ifndef SEISCOMP_AUTOLOC_AUTOLOC3
#define SEISCOMP_AUTOLOC_AUTOLOC3


#include <string>
#include <map>
#include <set>

#include <seiscomp/seismology/locator/locsat.h>

#include "datamodel.h"
#include "nucleator.h"
#include "associator.h"
#include "locator.h"

namespace Autoloc {

typedef enum { GlobalNetwork, RegionalNetwork, LocalNetwork } NetworkType;

class StationConfig {
	public:
		struct Entry {
			float maxNucDist{0.0};
			float maxLocDist{0.0};
			int usage{0};
		};

		StationConfig();
		bool read(const std::string &filename);

		const Entry& get(const std::string &net, const std::string &sta) const;

	private:
		std::map<std::string, Entry> _entry;
};


class Autoloc3 {

	public:
		Autoloc3();
		virtual ~Autoloc3();

	public:
		struct Config {
		public:
			// Config();

			// White list of allowed pick authors.
			// Also defines priority in descending order.
			std::vector<std::string> authors;

			// During cleanup() all objects older than maxAge
			// (in hours) are removed.
			// If this parameter is <= 0, cleanup() is disabled.
			double maxAge{6.0 * 3600.0};   // unit: s

			// time span within which we search for picks which may indicate extraordinary activity
			double dynamicPickThresholdInterval{3600.0};   // unit: s

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
			std::string pickLogFile{""};
			int         pickLogDate;

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

		public:
			void dump() const;
		};

	public:
		// initialization stuff
		void setConfig(const Config &config);
		const Config &config() const { return _config; }

		bool setGridFile(const std::string &);
		void setPickLogFilePrefix(const std::string &);
		void setPickLogFileName(const std::string &);
		bool setStation(Station *);
		void setLocatorProfile(const std::string &);

		bool init();

	public:
		// Feed a Pick and try to get something out of it.
		//
		// The Pick may be associated to an existing Origin or
		// trigger the creation of a new Origin. If "something"
		// resulted, return true, otherwise false.
		bool feed(const Pick *);

		// Feed a PickGroup and try to get something out of it.
		//
		// This feeds all the picks in the PickGroup atomically.
		bool feed(const PickGroup&); // TODO: NOT IMPLEMENTED YET

		// Feed an external or manual Origin
		bool feed(Origin *);

		// Report all new origins and thereafter empty _newOrigins.
		// This calls _report(Origin*) for each new Origin
		bool report();

		void dumpState() const;
		void dumpConfig() const { _config.dump(); }

	public:
		// Trigger removal of old objects.
		void cleanup(Time minTime=0);

		void reset();
		void shutdown();

	public:
		// Get a Pick from Autoloc's internal buffer by ID.
		// If the pick is not found, return NULL
		const Pick* pick(const std::string &id) const;

		// Current time. In offline mode time of the last pick.
		Time now();

		// Synchronize the internal timing.
		//
		// This is necessary in playback mode were instead of
		// using the hardware clock we either use the pick time
		// or pick creation time.
		bool sync(const Time &t);

	protected:
		// This must be reimplemented in a subclass to properly
		// print/send/etc. the origin
		virtual bool _report(const Origin *);

	protected:
		// flush any pending (Origin) messages by calling _report()
		void _flush();

		// Compute author priority. First in list gets highest
		// priority. Not in list gets priority 0. No priority list
		// defined gives 1 for all authors.
		int _authorPriority(const std::string &author) const;

	private:
		//
		// tool box
		//

		// Compute the score.
		double _score(const Origin *) const;
		void _updateScore(Origin *);

		// Process the pick. Involves trying to associate with an
		// existing origin and -upon failure- nucleate a new one.
		bool _process(const Pick *);

		// If the pick is an XXL pick, try to see if there are more
		// XXL picks possibly giving rise to a preliminary origin
		OriginPtr _xxlPreliminaryOrigin(const Pick*);

		// Try to enhance the score by removing each pick
		// and relocating.
		//
		// Returns true if the score could be enhanced.
		bool _enhanceScore(Origin *, size_t maxloops=0);

		// Rename P <-> PKP accoring to distance/traveltime
		// FIXME: This is a hack we would want to avoid.
		void _rename_P_PKP(Origin *);

		// Large outliers, resulting in an obviously wrong
		// association, are excluded from the location.
		//
		// Returns number of removed outliers.
		size_t _removeOutliers(Origin *);

		// Try all that can be done to improve the origin:
		//
		// * exclude picks with unacceptably large residuals
		//
		// * see if there are picks previously excluded due to
		//   large residual, but which now have acceptably small
		//   residual
		//
		// TODO:
		// * see if any picks that previously slipped through
		//   nucleator and associator do match the origin and if
		//   so, try a relocation
		//
		// TODO:
		// * see if there are distant stations which due to a
		//   large number of near stations can be ignored
		//     -> useful to get rid of PKP
		//
		// TODO:
		// * take care of PP, SKP, possibly pP and the like
		//
		// Returns true on success.
//		bool _improve(Origin *);

		// true, if this pick was marked as bad and must not be used
		void _setBlacklisted(const Pick *, bool=true);

		// true, if this pick was marked as bad and must not be used
		bool _blacklisted(const Pick *) const;

		// true if pick comes with valid station info
		bool _addStationInfo(const Pick *);

		// feed pick to nucleator and return a new origin or NULL
		OriginPtr _tryNucleate(const Pick*);

		// feed pick to nucleator and return a new origin or NULL
		OriginPtr _tryAssociate(const Pick*);

		// one last check before the origin is accepted.
		// doesn't modify the origin
		bool _passedFinalCheck(const Origin *origin);
		// called by _passedFilter()
		// TODO: integrate into _passedFilter()?

		// check whether the origin is acceptable
		bool _passedFilter(Origin*);

		// check whether the origin meets the publication criteria
		bool _publishable(const Origin*) const;

		// store a pick in internal buffer
		bool _store(const Pick*);

		// store an origin in internal buffer
		bool _store(Origin*);

		// try to find and add more matching picks to origin
		bool _addMorePicks(Origin*, bool keepDepth=false);

		// try to associate a pick to an origin
		// true if successful
		bool _associate(Origin*, const Pick*, const std::string &phase);

		// try to judge if an origin *may* be a fake, e.g.
		// coincident PP phases if an earlier event
		//
		// returns a "probability" between 0 and 1
		double _testFake(Origin*) const;

		// set the depth of this origin to the default depth
		//
		// if successful, true is returned, otherwise false
		bool _setDefaultDepth(Origin*);

		// Attempt to decide whether the focal depth can be resolved considering
		// the station distribution. Returns true if resolvable, false if not.
		bool _depthIsResolvable(Origin*);

		bool _setTheRightDepth(Origin*);

		// Determine whether the epicenter location requires the use of the default depth.
		// This may be due to the epicenter located
		// * far outside the network or
		// * near to an ocean ridge where on one hand we don't have depth resolution
		//   but on the other hand we can safely assume a certain (shallow) depth.
		//
		// returns true if the default depth should be used, false otherwise
		bool _epicenterRequiresDefaultDepth(const Origin*) const;

		// For convenience. Checks whether a residual is within bounds.
		// TODO: Later this shall accomodate distance dependent limits.
		// The factor allows somewhat more generous residuals e.g.
		// for association.
		bool _residualOK(const Arrival&, double minFactor=1, double maxFactor=1) const;

		// Don't allow Arrivals with residual > maxResidual. This
		// criterion is currently enforced mercilessly, without
		// allowing for asymmetry. What you specify (maxResidual) is
		// what you get.
		//
		// NOTE that the behavior is partly controlled through the configuration of _relocator
		bool _trimResiduals(Origin*);

		bool _log(const Pick*);

		bool _tooLowSNR(const Pick*) const;

		bool _followsBiggerPick(const Pick*) const;

		// checks if the pick could be a Pdiff of a known origin
		bool _perhapsPdiff(const Pick *) const;

		// Suppress picks with low amplitude if station is producing
		// many unassociated picks in a recent time span
		bool _tooManyRecentPicks(const Pick *) const;

		// Merge two origins considered to be the same event
		// The second is merged into the first. A new instance
		// is returned that has the ID of the first
		Origin *merge(const Origin*, const Origin*);

		bool _associated(const Pick *) const;

		bool _rework(Origin *);

		void _ensureConsistentArrivals(Origin *);

		void _ensureAcceptableRMS(Origin *, bool keepDepth=false);

		// Exclude PKP phases if there are enough P phases.
		// This is part of _rework()
		bool _excludePKP(Origin *);

		// If all stations are close to the epicenter, but just a few
		// are very far away, exclude the farthest by setting
		// Arrival::exlcude to Arrival::StationDistance
		bool _excludeDistantStations(Origin *);

		OriginID _newOriginID();

		// try to find an "equivalent" origin
		Origin *_findEquivalent(const Origin*);

		// for a newly fed origin, find an equivalent internal origin
		Origin *_findMatchingOrigin(const Origin*);


	private:
		Associator _associator;
		GridSearch _nucleator;
		Locator    _relocator;

	private:
		// origins that were created/modified during the last
		// feed() call
		OriginVector _newOrigins;

		// origins waiting for a _flush()
		std::map<int, Time>      _nextDue;
		std::map<int, OriginPtr> _lastSent;
		std::map<int, OriginPtr> _outgoing;

		Time     _now;
		Time     _nextCleanup;

	protected:
		typedef std::map<std::string, PickCPtr> PickPool;
		PickPool pickPool;
		std::string   _pickLogFilePrefix;
		std::string   _pickLogFileName;
		std::ofstream _pickLogFile;

	private:
		StationMap _stations;

		// a list of NET.STA strings for missing stations
		std::set<std::string> _missingStations;
		std::set<PickCPtr> _blacklist;

		OriginVector _origins;
		Config   _config;
		StationConfig _stationConfig;
};


}  // namespace Autoloc


#endif
