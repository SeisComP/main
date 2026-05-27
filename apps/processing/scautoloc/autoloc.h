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

#include "datamodel.h"
#include "nucleator.h"
#include "associator.h"
#include "locator.h"
#include "config.h"
#include "stationconfig.h"

namespace Seiscomp {

namespace Autoloc {

class Autoloc3 {

	public:
		Autoloc3();
		virtual ~Autoloc3() = default;

	public:
		// Initialization stuff

		// Set the *SeisComP* config
		void setConfig(const Seiscomp::Config::Config*);

		// Set the *SeisComP* inventory
		void setInventory(const Seiscomp::DataModel::Inventory*);

		void setConfig(const AutolocConfig&);
		const AutolocConfig &config() const { return _config; }

		bool setGridFile(const std::string &);
		void setPickLogFilePrefix(const std::string &);
		void setPickLogFileName(const std::string &);
		
		// Initialize one station at runtime
		bool initOneStation(const DataModel::WaveformStreamID&, const Core::Time&);
		bool setStation(Station *);

		void setLocatorProfile(const std::string &);

		bool init();

	public:
		// Current time. In offline mode time of the last pick.
//		Seiscomp::Core::Time now();

		// Synchronize the internal timing.
		//
		// This is necessary in playback mode were instead of
		// using the hardware clock we either use the pick time
		// or pick creation time.
//		void sync(const Seiscomp::Core::Time &time);

		// public object input interface

		// Feed a Pick/Amplitude and eventually process it.
		//
		// If the call resulted in a new or updated result,
		// return true, otherwise false.
		bool xfeed(const Seiscomp::DataModel::Pick*);
		bool xfeed(const Seiscomp::DataModel::Amplitude*);

		// Feed an external or manual Origin
		// TODO: Ensure that all needed picks/amplitudes have
		//       been supplied *prior* to calling this.
		bool xfeed(Seiscomp::DataModel::Origin*);

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
		void sync(const Core::Time &t);

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

		Time _now {0};
		Time _nextCleanup {0};

		Core::Time syncTime;

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
		AutolocConfig _config;
		StationConfig _stationConfig;

		const Seiscomp::Config::Config *scconfig;
		const Seiscomp::DataModel::Inventory *scinventory;
};


}  // namespace Autoloc

}  // namespace Seiscomp

#endif
