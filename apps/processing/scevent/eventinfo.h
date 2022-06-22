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


#ifndef SEISCOMP_APPLICATIONS_EVENTINFO_H__
#define SEISCOMP_APPLICATIONS_EVENTINFO_H__


#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/magnitude.h>
#include <seiscomp/datamodel/event.h>
#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/journalentry.h>
#include <seiscomp/datamodel/databasequery.h>
#include <seiscomp/datamodel/publicobjectcache.h>

#include "constraints.h"
#include "config.h"

#include <list>
#include <set>
#include <map>
#include <string>


namespace Seiscomp {

namespace Client {


DEFINE_SMARTPOINTER(EventInformation);

struct EventInformation : public Seiscomp::Core::BaseObject {
	typedef DataModel::PublicObjectCache Cache;

	EventInformation(Cache *cache, Config *cfg);

	EventInformation(Cache *cache, Config *cfg,
	                 DataModel::DatabaseQuery *q, const std::string &eventID,
	                 const std::string &self);

	EventInformation(Cache *cache, Config *cfg,
	                 DataModel::DatabaseQuery *q, DataModel::EventPtr &event,
	                 const std::string &self);

	~EventInformation();

	//! Loads an event from the database
	void load(DataModel::DatabaseQuery *q,
	          const std::string &eventID,
	          const std::string &self);

	void load(DataModel::DatabaseQuery *q,
	          DataModel::EventPtr &event,
	          const std::string &self);

	void loadAssocations(DataModel::DatabaseQuery *q);

	//! Returns the number of matching picks
	size_t matchingPicks(DataModel::DatabaseQuery *q, DataModel::Origin *o);

	bool valid() const;

	bool associate(DataModel::Origin *o);
	bool associate(DataModel::FocalMechanism *fm);

	bool addJournalEntry(DataModel::JournalEntry *e, const std::string &self);

	bool setEventName(DataModel::JournalEntry *e, std::string &error);
	bool setEventOpComment(DataModel::JournalEntry *e, std::string &error);

	void insertPick(DataModel::Pick *p);

	Cache                                 *cache;
	Config                                *cfg;

	typedef std::multimap<std::string, DataModel::PickPtr> PickAssociation;
	bool                                   created;
	std::set<std::string>                  pickIDs;
	PickAssociation                        picks;
	DataModel::EventPtr                    event;
	DataModel::OriginPtr                   preferredOrigin;
	DataModel::FocalMechanismPtr           preferredFocalMechanism;
	DataModel::MagnitudePtr                preferredMagnitude;
	std::list<DataModel::JournalEntryPtr>  journal;

	Constraints                            constraints;

	bool                                   aboutToBeRemoved;
	bool                                   dirtyPickSet;
};


}

}


#endif
