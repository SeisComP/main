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


#ifndef SEISCOMP_LIBAUTOLOC_AUTOLOC_H_INCLUDED
#define SEISCOMP_LIBAUTOLOC_AUTOLOC_H_INCLUDED

#include <string>
#include <map>
#include <set>

#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/amplitude.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/inventory.h>
#include <seiscomp/config/config.h>
#include <seiscomp/seismology/locator/locsat.h>

#include <seiscomp/autoloc/config.h>
#include <seiscomp/autoloc/datamodel.h>
#include <seiscomp/autoloc/nucleator.h>
#include <seiscomp/autoloc/associator.h>
#include <seiscomp/autoloc/locator.h>
#include <seiscomp/autoloc/stationconfig.h>

namespace Seiscomp {

namespace Autoloc {

class Autoloc3 {

	public:
		Autoloc3();
		virtual ~Autoloc3() = default;

	public:
		// Startup configuration and initialization
		// All of these need to be called before init();

		// Set the *Autoloc-specific* config
		// This is not read from a file but usually populated
		// with values specified as command-line arguments
		void setConfig(const Autoloc::Config &config);
		const Autoloc::Config &config() const { return _config; }
		void dumpConfig() const;

		// Set the *SeisComP* config
		void setConfig(const Seiscomp::Config::Config*);

		// Set the *SeisComP* inventory
		void setInventory(const Seiscomp::DataModel::Inventory*);

		bool setGridFilename(const std::string &);

		void setPickLogFilePrefix(const std::string &);
		void setPickLogFileName(const std::string &);

		void setStationConfigFilename(const std::string &);
		const std::string &stationConfigFilename() const;



		// Set the locator profile, e.g. "iasp91".
		//
		// Now obsolete as the same can be achieved via
		// setConfig(const Seiscomp::Config::Config*);
		void setLocatorProfile(const std::string &);

		// To be run *after* all the above configuration settings are
		// finished.
		bool init();

		void shutdown();

	public:
		// Current time. In offline mode time of the last pick.
		Seiscomp::Core::Time now();

		// Synchronize the internal timing.
		//
		// This is necessary in playback mode were instead of
		// using the hardware clock we either use the pick time
		// or pick creation time.
		void sync(const Seiscomp::Core::Time &time);

		// public object input interface

		// Feed a Pick and eventually process it.
		//
		// If this pick resulted in a new or updated result,
		// return true, otherwise false.
		bool feed(Seiscomp::DataModel::Pick*);

		bool feed(Seiscomp::DataModel::Amplitude*);

		// Feed an external or manual Origin
		bool feed(Seiscomp::DataModel::Origin*);

		// After the station config has changed we need to reconfigure
		// all already configured stations.
		bool reconfigureStations();

	private:
		// Runtime initialization.

		// Initialize one station at runtime.
		// If successful, return true, otherwise return false.
		bool setupStation(
			const Seiscomp::DataModel::WaveformStreamID &wfid,
                        const Seiscomp::Core::Time &time);

		// private object interface
		bool feed(const Autoloc::DataModel::Pick*);

		// Feed a PickGroup and try to get something out of it.
		//
		// This feeds all the picks in the PickGroup atomically.
		// TODO: NOT IMPLEMENTED YET
		bool feed(const Autoloc::DataModel::PickGroup&);

		bool feed(Autoloc::DataModel::Origin*);

	protected:
		// Report all new origins and thereafter empty _newOrigins.
		// This calls _report(Origin*) for each new Origin
		// TODO: review needed
		void report();

		// generate a time stamp in the log
                void timeStamp() const;

	private:
		// Dump some (debug) information about the internal state.
		void dumpState() const;

		// Trigger removal of old objects.
		//
		// Objects older than minTime are removed but this is not a
		// hard limit. For instance, unassociated picks can safely be
		// removed but origins with associated picks should be kept
		// for a somewhat longer time.
		void cleanup(Autoloc::DataModel::Time minTime=0);

		void reset();

		// Get a Pick from Autoloc's internal pick pool by its
		// publicID. If the pick is not found, return NULL.
		const Autoloc::DataModel::Pick* pickFromPool(
			const std::string &id) const;
		bool pickInPool(const std::string &id) const;

	protected:
	public:
		// This must be reimplemented in a subclass to properly
		// print/send/etc. the origin
		virtual bool _report(Seiscomp::DataModel::Origin*);

	private:
		// interface Autoloc::DataModel -> Seiscomp::DataModel
		bool _report(const Autoloc::DataModel::Origin*);

	private:
		// Compute author priority. First in list gets highest
		// priority. Not in list gets priority 0. No priority list
		// defined gives 1 for all authors.
		int _authorPriority(const std::string &author) const;

	private:
		//
		// tool box
		//

		// import from Seiscomp::DataModel
		Autoloc::DataModel::Origin *importFromSC(
			const Seiscomp::DataModel::Origin*);

		// Compute the score.
		double _score(const Autoloc::DataModel::Origin*) const;
		void _updateScore(Autoloc::DataModel::Origin*);

		// Process the pick. Involves trying to associate with an
		// existing origin and -upon failure- nucleate a new one.
		bool _process(const Autoloc::DataModel::Pick*);

		// If the pick is an XXL pick, try to see if there are more
		// XXL picks possibly giving rise to a preliminary origin
		Autoloc::DataModel::OriginPtr _xxlPreliminaryOrigin(
			const Autoloc::DataModel::Pick*);

		// Try to enhance the score by removing each pick
		// and relocating.
		//
		// Returns true if the score could be enhanced.
		bool _enhanceScore(Autoloc::DataModel::Origin*, int maxloops=0);

		// Rename P <-> PKP accoring to distance/traveltime
		// FIXME: This is a hack we would want to avoid.
		void _rename_P_PKP(Autoloc::DataModel::Origin *);

		// Really big outliers, where the assiciation obviously
		// went wrong and which are excluded from the location
		// anyway, are removed.
		// 
		// Returns number of removed outliers.
		int _removeWorstOutliers(Autoloc::DataModel::Origin *);

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

		// true if pick comes with valid station info
		bool _addStationInfo(const Autoloc::DataModel::Pick*);

		// feed pick to nucleator and return a new origin or NULL
		Autoloc::DataModel::OriginPtr _tryNucleate(
			const Autoloc::DataModel::Pick*);

		// feed pick to nucleator and return a new origin or NULL
		Autoloc::DataModel::OriginPtr _tryAssociate(
			const Autoloc::DataModel::Pick*);

		// one last check before the origin is accepted.
		// doesn't modify the origin
		bool _passedFinalCheck(const Autoloc::DataModel::Origin*);
		// called by _passedFilter()
		// TODO: integrate into _passedFilter()?

		// check whether the origin is acceptable
		bool _passedFilter(Autoloc::DataModel::Origin*);

		// check whether the origin meets the publication criteria
		bool _publishable(const Autoloc::DataModel::Origin*) const;

		// store a pick in pick pool. Returns true if it is a new pick
		// and false otherwise.
		bool storeInPool(const Autoloc::DataModel::Pick*);

		// store an origin in origin pool
		bool _store(Autoloc::DataModel::Origin*);

		// try to find and add more matching picks to origin
		bool _addMorePicks(
			Autoloc::DataModel::Origin*, bool keepDepth=false);

		// try to associate a pick to an origin
		// true if successful
		bool _associate(
			      Autoloc::DataModel::Origin*,
			const Autoloc::DataModel::Pick*,
			const std::string &phase);

		// Try to judge if an origin *may* be a fake, e.g.
		// coincident PP phases if an earlier event
		//
		// returns a "probability" between 0 and 1
		double _testFake(Autoloc::DataModel::Origin*) const;

		// Set the depth of this origin to the default depth
		//
		// Returns true if successful, otherwise false
		bool _setDefaultDepth(Autoloc::DataModel::Origin*);

		// Attempt to decide whether the focal depth can be resolved
		// considering the station distribution.
		//
		// Returns
		// * true if resolvable
		// * false if not.
		bool _depthIsResolvable(Autoloc::DataModel::Origin*);

		bool _setTheRightDepth(Autoloc::DataModel::Origin*);

		// Determine whether the epicenter location requires the
		// use of the default depth. This may be due to the epicenter
		// located e.g.
		// * far outside the network and/or
		// * near to an ocean ridge where on one hand we don't
		//   have depth resolution but on the other hand we can
		//   safely assume a certain (shallow) depth.
		//
		// Returns
		// * true if the default depth should be used,
		// * false otherwise
		bool _epicenterRequiresDefaultDepth(
			const Autoloc::DataModel::Origin*) const;

		// For convenience. Checks whether a residual is within bounds.
		// TODO: Later this shall accomodate distance dependent limits.
		// The factor allows somewhat more generous residuals e.g.
		// for association.
		bool _residualOK(
			const Autoloc::DataModel::Arrival&,
			double minFactor=1, double maxFactor=1) const;

		// Don't allow Arrivals with residual > maxResidual. This
		// criterion is currently enforced mercilessly, without
		// allowing for asymmetry. What you specify (maxResidual) is
		// what you get.
		//
		// NOTE that the behavior is partly controlled through
		// the configuration of _relocator
		bool _trimResiduals(Autoloc::DataModel::Origin*);

		bool _log(const Autoloc::DataModel::Pick*);

		bool _tooLowSNR(const Autoloc::DataModel::Pick*) const;

		bool _followsBiggerPick(const Autoloc::DataModel::Pick*) const;

		// checks if the pick could be a Pdiff of a known origin
		bool _perhapsPdiff(const Autoloc::DataModel::Pick*) const;

		// Suppress picks with low amplitude if station is producing
		// many unassociated picks in a recent time span
		bool _tooManyRecentPicks(const Autoloc::DataModel::Pick*) const;

		// Merge two origins considered to be the same event
		// The second is merged into the first. A new instance
		// is returned that has the ID of the first
		Autoloc::DataModel::Origin *merge(
			const Autoloc::DataModel::Origin*,
			const Autoloc::DataModel::Origin*);

		bool _associated(const Autoloc::DataModel::Pick*) const;

		bool _rework(Autoloc::DataModel::Origin*);

		void _ensureConsistentArrivals(Autoloc::DataModel::Origin*);

		void _ensureAcceptableRMS(Autoloc::DataModel::Origin*, bool keepDepth=false);

		// Exclude PKP phases if there are enough P phases.
		// This is part of _rework()
		bool _excludePKP(Autoloc::DataModel::Origin*);

		// If all stations are close to the epicenter, but just a few
		// are very far away, exclude the farthest by setting
		// Arrival::exlcude to Arrival::StationDistance
		bool _excludeDistantStations(Autoloc::DataModel::Origin*);

		Autoloc::DataModel::OriginID _newOriginID();

		// try to find an "equivalent" origin
		Autoloc::DataModel::Origin *_xxlFindEquivalentOrigin(
			const Autoloc::DataModel::Origin*);

		// for a newly fed origin, find an equivalent internal origin
		Autoloc::DataModel::Origin *_findMatchingOrigin(
			const Autoloc::DataModel::Origin*);


	private:
		Associator _associator;
		GridSearch _nucleator;
		Locator    _relocator;

		// origins waiting for a _flush()
		std::map<int, Autoloc::DataModel::Time>      _nextDue;
		std::map<int, Autoloc::DataModel::OriginPtr> _lastSent;
		std::map<int, Autoloc::DataModel::OriginPtr> _outgoing;

		Autoloc::DataModel::Time _now;
		Autoloc::DataModel::Time _nextCleanup;

		Autoloc::DataModel::PickPool pickPool;

		Autoloc::DataModel::StationMap _stations;
		// a list of NET.STA strings for missing stations
		// FIXME: review!
		std::set<std::string> _missingStations;

		Autoloc::DataModel::OriginVector _origins;
		// origins that were created/modified during the last
		// feed() call
		Autoloc::DataModel::OriginVector _newOrigins;

		Autoloc::StationConfig _stationConfig;

		Config _config;

		const Seiscomp::Config::Config *scconfig;
		const Seiscomp::DataModel::Inventory *scinventory;
};


}  // namespace Autoloc

}  // namespace Seiscomp

#endif
