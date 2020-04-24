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


#ifndef SEISCOMP_APPLICATIONS_EVENTTOOL_CONFIG_H__
#define SEISCOMP_APPLICATIONS_EVENTTOOL_CONFIG_H__

#include <seiscomp/core/datetime.h>
#include <seiscomp/config/config.h>
#include <seiscomp/datamodel/types.h>
#include <vector>
#include <string>


namespace Seiscomp {

namespace Client {


struct Config {
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

	typedef std::vector<RegionFilter> RegionFilters;

	struct EventFilter {
		OPT(std::string) agencyID;
		OPT(std::string) author;
		OPT(DataModel::EvaluationMode) evaluationMode;
	};

	typedef std::vector<std::string> StringList;
	typedef std::set<std::string> StringSet;

	size_t          minAutomaticArrivals;
	OPT(double)     minAutomaticScore;
	size_t          minMatchingPicks;
	double          maxMatchingPicksTimeDiff;
	bool            matchingPicksTimeDiffAND;
	bool            matchingLooseAssociatedPicks;
	size_t          minStationMagnitudes;
	double          maxDist;
	Core::TimeSpan  maxTimeDiff;
	Core::TimeSpan  eventTimeBefore;
	Core::TimeSpan  eventTimeAfter;

	RegionFilters   regionFilter;

	size_t          mbOverMwCount;
	double          mbOverMwValue;
	size_t          minMwCount;

	bool            enableFallbackPreferredMagnitude;
	bool            updatePreferredSolutionAfterMerge;

	std::string     eventIDPrefix;
	std::string     eventIDPattern;
	StringList      magTypes;
	StringList      agencies;
	StringList      authors;
	StringList      methods;
	std::string     score;
	StringSet       blacklistIDs;

	StringList      priorities;

	EventFilter     delayFilter;
	int             delayTimeSpan;

	int             delayPrefFocMech;
	bool            ignoreMTDerivedOrigins;
	bool            setAutoEventTypeNotExisting;
};


}

}


#endif
