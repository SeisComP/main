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
			"Origin ID to associate (test only, no event updates)", true
		)
		& cli(
			eventID,
			"Input", "event-id,E",
			"Event ID to update preferred objects (test only, no event updates)", true
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

	std::string        eventIDPrefix{"gfz"};
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
};


}
}


#endif
