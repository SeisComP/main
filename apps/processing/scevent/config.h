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


#ifndef SEISCOMP_APPLICATIONS_EVENTTOOL_CONFIG_H
#define SEISCOMP_APPLICATIONS_EVENTTOOL_CONFIG_H

#include <seiscomp/core/datetime.h>
#include <seiscomp/core/strings.h>
#include <seiscomp/config/config.h>
#include <seiscomp/datamodel/types.h>
#include <seiscomp/system/application.h>
#include <seiscomp/wired/server.h>

#include <vector>
#include <string>


namespace Seiscomp {
namespace Client {


struct Config : System::Application::AbstractSettings {
	DEFINE_SMARTPOINTER(Region);
	struct Region : public Core::BaseObject {
		virtual bool init(const Seiscomp::Config::Config &config, const std::string &prefix) = 0;
		virtual bool isInside(double lat, double lon) const = 0;
	};

	struct RegionFilter {
		RegionPtr   region;
		OPT(double) minDepth;
		OPT(double) maxDepth;
	};

	using RegionFilters = std::vector<RegionFilter>;

	struct EventFilter {
		void accept(System::Application::SettingsLinker &linker) {
			linker
			& cfg(agencyID, "agencyID")
			& cfg(author, "author")
			& cfg(evaluationMode, "evaluationMode")
			;
		}

		OPT(std::string) agencyID;
		OPT(std::string) author;
		OPT(DataModel::EvaluationMode) evaluationMode;
	};

	using StringList = std::vector<std::string>;
	using StringSet = std::set<std::string>;

	void accept(System::Application::SettingsLinker &linker) {
		linker
		& cliSwitch(
			testMode,
			"Messaging", "test",
			"Test mode, no messages are sent"
		)
		& cliSwitch(
			clearCache,
			"Messaging", "clear-cache",
			"Send a clear cache message and quit"
		)
		& cliSwitch(
			dbDisable,
			"Database", "db-disable",
			"Do not use the database at all"
		)
		& cli(
			fExpiry,
			"Generic", "expiry,x",
			"Time span in hours after which objects expire", true
		)
		& cli(
			originID,
			"Input", "origin-id,O",
			"Origin ID to associate, updates will be sent unless --test is given", true
		)
		& cli(
			eventID,
			"Input", "event-id,E",
			"Event ID to update preferred objects, updates will be sent unless --test is given", true
		)
		& cli(
			epFile,
			"Input", "ep",
			"Event parameters XML file for offline processing "
			"of all contained origins. Use '-' to read from "
			"stdin."
		)
		& cliSwitch(
			reprocess,
			"Input", "reprocess",
			"Reprocess event parameters ignoring all event and "
			"journal objects in input file. Works only in "
			"combination with '--ep'."
		)
		& cliSwitch(
			updateEventID,
			"Input", "update-event-id",
			"Update IDs of events if they already exist. Works "
			"only in combination with '--ep'."
		)
		& cliSwitch(
			formatted,
			"Output", "formatted,f",
			"Use formatted XML output. Otherwise XML is unformatted."
		)
		& cliSwitch(
			logProcessing,
			"Output", "disable-info-log",
			"Do not populate the scevent-processing-info.log file."
		)
		& cfg(eventAssociation, "eventAssociation")
		& cfg(eventIDPrefix, "eventIDPrefix")
		& cfg(eventIDPattern,"eventIDPattern")
		& cfg(eventIDLookupMargin, "eventIDLookupMargin")
		& cfg(populateFERegion, "populateFERegion")
		& cfg(restAPI, "restAPI")
		& cfg(eventIDSync, "eventIDSync")
		;
	}

	bool               testMode{false};
	bool               clearCache{false};
	bool               dbDisable{false};
	bool               logProcessing{true};
	bool               formatted{false};
	bool               reprocess{false};
	bool               updateEventID{false};
	double             fExpiry{1.0};

	std::string        originID;
	std::string        eventID;
	std::string        epFile;

	std::string        eventIDPrefix;
	std::string        eventIDPattern{"%p%Y%04c"};
	int                eventIDLookupMargin{-1};

	RegionFilters      regionFilter;
	StringSet          blacklistIDs;
	bool               populateFERegion{false};
	Wired::BindAddress restAPI;

	struct {
		void accept(System::Application::SettingsLinker &linker) {
			linker
			& cfg(minStationMagnitudes, "minimumMagnitudes")
			& cfg(minMatchingPicks, "minimumMatchingArrivals")
			& cfg(maxMatchingPicksTimeDiff, "maximumMatchingArrivalTimeDiff")
			& cfg(matchingPicksTimeDiffAND, "compareAllArrivalTimes")
			& cfg(matchingLooseAssociatedPicks, "allowLooseAssociatedArrivals")
			& cfg(minAutomaticArrivals, "minimumDefiningPhases")
			& cfg(minAutomaticScore, "minimumScore")

			& cfg(eventTimeBefore, "eventTimeBefore")
			& cfg(eventTimeAfter, "eventTimeAfter")
			& cfg(maxTimeDiff, "maximumTimeSpan")
			& cfg(maxDist, "maximumDistance")

			& cfg(minMwCount, "minMwCount")
			& cfg(mbOverMwCount, "mbOverMwCount")
			& cfg(mbOverMwValue, "mbOverMwValue")
			& cfg(magPriorityOverStationCount, "magPriorityOverStationCount")

			& cfg(updatePreferredSolutionAfterMerge, "updatePreferredAfterMerge")
			& cfg(enableFallbackPreferredMagnitude, "enableFallbackMagnitude")
			& cfg(magTypes, "magTypes")
			& cfg(agencies, "agencies")
			& cfg(authors, "authors")
			& cfg(methods, "methods")
			& cfg(score, "score")
			& cfg(priorities, "priorities")

			& cfg(delayPrefFocMech, "delayPrefFocMech")
			& cfg(ignoreMTDerivedOrigins, "ignoreFMDerivedOrigins")
			& cfg(enablePreferredFMSelection, "enablePreferredFMSelection")
			& cfg(setAutoEventTypeNotExisting, "declareFakeEventForRejectedOrigin")

			& cfg(delayTimeSpan, "delayTimeSpan")
			& cfg(delayFilter, "delayFilter")
			;
		}

		size_t             minStationMagnitudes{4};
		size_t             minMatchingPicks{3};
		double             maxMatchingPicksTimeDiff{-1};
		bool               matchingPicksTimeDiffAND;
		bool               matchingLooseAssociatedPicks{false};
		size_t             minAutomaticArrivals{10};
		OPT(double)        minAutomaticScore;
		Core::TimeSpan     eventTimeBefore{30 * 60, 0};
		Core::TimeSpan     eventTimeAfter{30 * 60, 0};
		Core::TimeSpan     maxTimeDiff{60.};
		double             maxDist{5.0};
		size_t             minMwCount{8};
		size_t             mbOverMwCount{30};
		double             mbOverMwValue{6.0};
		bool               magPriorityOverStationCount{false};
		bool               updatePreferredSolutionAfterMerge{false};
		bool               enableFallbackPreferredMagnitude{false};
		StringList         magTypes{"mBc", "Mw(mB)", "Mwp", "ML", "MLh", "MLv", "mb"};
		StringList         agencies;
		StringList         authors;
		StringList         methods;
		std::string        score;
		StringList         priorities;
		int                delayTimeSpan{0};
		EventFilter        delayFilter;
		int                delayPrefFocMech{0};
		bool               ignoreMTDerivedOrigins{true};
		bool               enablePreferredFMSelection{true};
		bool               setAutoEventTypeNotExisting{false};
	}                  eventAssociation;

	struct {
		void accept(System::Application::SettingsLinker &linker) {
			linker
			& cfg(main, "main")
			& cfg(mainTimeout, "mainTimeout")
			& cfg(cacheRetention, "cacheRetention")
			& cfg(db, "db")
			& cfg(databaseRetention, "databaseRetention")
			;
		}

		//! URL of the eventID main instance's REST API, e.g.,
		//! "http://event-main.example.org:18180". When empty, this instance acts as a
		//! main instance (or standalone) and allocates eventIDs locally. When set, this
		//! instance is a secondary instance and will ask the main instance for an
		//! eventID before falling back to local allocation.
		std::string  main;

		//! Maximum number of seconds a secondary instance waits for a response from the
		//! main instance before falling back to local eventID allocation.
		int          mainTimeout{5};

		//! Number of seconds a main instance keeps a reserved eventID in its in-memory
		//! cache (together with the foreign origin that triggered the reservation).
		//! While the entry is cached, a local origin matching that foreign origin is
		//! assigned the same eventID instead of allocating a new one. After this time
		//! the entry is dropped from the cache; the eventID may then be reused by
		//! subsequent allocations.
		int          cacheRetention{1800};

		//! Path to an SQLite database file used by a main instance to keep the eventIDs
		//! it reserves on behalf of secondary instances beyond the lifetime of the
		//! in-memory cache. When empty (the default) the reservations live only in
		//! memory and are lost when scevent restarts. When set, reservations survive
		//! restarts and may cover a much larger origin/eventID set than the in-memory
		//! cache.
		std::string  db;

		//! Number of seconds a reserved eventID is kept in the persistent database
		//! given by 'db', measured by origin time. Should be at least as large as
		//! cacheRetention; a larger value lets the main instance keep answering
		//! secondary-instance requests for older origins after a restart. A negative
		//! value (the default) disables pruning entirely, so reservations are kept
		//! indefinitely.
		int          databaseRetention{-1};
	}                  eventIDSync;
};


}
}


#endif
