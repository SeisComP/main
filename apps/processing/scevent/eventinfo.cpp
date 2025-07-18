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


#define SEISCOMP_COMPONENT SCEVENT
#include <seiscomp/logging/log.h>
#include <seiscomp/utils/misc.h>

#include "eventinfo.h"
#include "util.h"

#include <algorithm>


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
using namespace std;
using namespace Seiscomp;
using namespace Seiscomp::DataModel;
using namespace Seiscomp::Client;
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EventInformation::EventInformation(Cache *c, Config *cfg)
: cache(c), cfg(cfg) {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EventInformation::EventInformation(Cache *c, Config *cfg,
                                   DatabaseQuery *q, const string &eventID,
                                   const string &self)
: cache(c), cfg(cfg) {
	load(q, eventID, self);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EventInformation::EventInformation(Cache *c, Config *cfg,
                                   DatabaseQuery *q, EventPtr &event,
                                   const string &self)
: cache(c), cfg(cfg) {
	load(q, event, self);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EventInformation::~EventInformation() {
	for ( auto &entry : journal ) {
		entry->detach();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventInformation::load(DatabaseQuery *q, const string &eventID, const string &self) {
	EventPtr e = cache->get<Event>(eventID);
	load(q, e, self);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventInformation::load(DatabaseQuery *q, EventPtr &e, const string &self) {
	event = e;
	if ( !event ) {
		return;
	}

	dirtyPickSet = true;

	preferredOrigin = nullptr;
	preferredMagnitude = nullptr;
	preferredFocalMechanism = nullptr;

	if ( !event->preferredOriginID().empty() ) {
		preferredOrigin = cache->get<Origin>(event->preferredOriginID());
	}

	if ( preferredOrigin ) {
		if ( !preferredOrigin->arrivalCount() && q ) {
			q->loadArrivals(preferredOrigin.get());
		}

		if ( !preferredOrigin->magnitudeCount() && q ) {
			q->loadMagnitudes(preferredOrigin.get());
		}
	}


	if ( !event->preferredMagnitudeID().empty() ) {
		preferredMagnitude = cache->get<Magnitude>(event->preferredMagnitudeID());
	}

	if ( !event->preferredFocalMechanismID().empty() ) {
		preferredFocalMechanism = cache->get<FocalMechanism>(event->preferredFocalMechanismID());
	}

	if ( preferredFocalMechanism ) {
		if ( !preferredFocalMechanism->momentTensorCount() && q ) {
			q->loadMomentTensors(preferredFocalMechanism.get());
		}
	}

	// Read journal for event
	if ( q ) {
		auto dbit = q->getJournal(event->publicID());
		while ( dbit.get() ) {
			JournalEntryPtr entry = JournalEntry::Cast(dbit.get());
			if ( entry ) {
				addJournalEntry(entry.get(), self);
			}
			++dbit;
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventInformation::loadAssocations(DataModel::DatabaseQuery *q) {
	if ( q && event ) {
		q->load(event.get());
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
size_t EventInformation::matchingPicks(DataModel::DatabaseQuery *q,
                                       DataModel::Origin *o,
                                       const EventInformation::PickCache *pickCache) {
	if ( dirtyPickSet ) {
		pickIDs.clear();
		picks.clear();

		if ( !event ) {
			return 0;
		}

		for ( size_t i = 0; i < event->originReferenceCount(); ++i  ) {
			OriginPtr org = cache->get<Origin>(event->originReference(i)->originID());
			if ( !org ) {
				continue;
			}
			if ( q && org->arrivalCount() == 0 ) {
				q->loadArrivals(org.get());
			}
			for ( size_t j = 0; j < org->arrivalCount(); ++j ) {
				try { if ( org->arrival(j)->weight() <= 0.0 ) continue; }
				catch ( ... ) {}

				pickIDs.insert(org->arrival(j)->pickID());
				if ( cfg->eventAssociation.maxMatchingPicksTimeDiff >= 0 ) {
					PickPtr p = cache->get<Pick>(org->arrival(j)->pickID());
					if ( p ) {
						insertPick(p.get());
					}
				}
			}
		}

		dirtyPickSet = false;
	}

	size_t matches = 0;

	if ( cfg->eventAssociation.maxMatchingPicksTimeDiff < 0 ) {
		for ( size_t i = 0; i < o->arrivalCount(); ++i ) {
			if ( !o->arrival(i) ) {
				continue;
			}
			// weight = 0 => raus
			if ( !cfg->eventAssociation.matchingLooseAssociatedPicks
			  && Private::arrivalWeight(o->arrival(i)) == 0 ) {
				continue;
			}
			if ( pickIDs.find(o->arrival(i)->pickID()) != pickIDs.end() ) {
				++matches;
			}
		}
	}
	else {
		for ( size_t i = 0; i < o->arrivalCount(); ++i ) {
			if ( !o->arrival(i) ) {
				continue;
			}
			if ( !cfg->eventAssociation.matchingLooseAssociatedPicks
			  && Private::arrivalWeight(o->arrival(i)) == 0 ) {
				continue;
			}
			PickPtr p;

			if ( pickCache ) {
				if ( auto it = pickCache->find(o->arrival(i)->pickID()); it != pickCache->end() ) {
					p = it->second;
				}
			}
			else {
				p = cache->get<Pick>(o->arrival(i)->pickID());
			}

			if ( !p ) {
				SEISCOMP_WARNING("could not load origin pick %s",
				                 o->arrival(i)->pickID().c_str());
				continue;
			}

			string id = p->waveformID().networkCode() + "." +
			            p->waveformID().stationCode();

			char code = ' ';
			try {
				code = Util::getShortPhaseName(p->phaseHint().code());
			}
			catch ( ... ) {}

			auto range = picks.equal_range(id);
			int hit = 0, cnt = 0;
			for ( auto it = range.first; it != range.second; ++it ) {
				Pick *cmp = it->second.get();
				char cmpCode = ' ';
				try {
					cmpCode = Util::getShortPhaseName(cmp->phaseHint().code());
				}
				catch ( ... ) {}

				if ( code != cmpCode ) {
					continue;
				}

				++cnt;
				try {
					double diff = fabs((double)(cmp->time().value() - p->time().value()));
					if ( diff <= cfg->eventAssociation.maxMatchingPicksTimeDiff )
						++hit;
				}
				catch ( ... ) {}
			}

			// No picks checked, continue
			if ( !hit ) {
				continue;
			}

			if ( cfg->eventAssociation.matchingPicksTimeDiffAND ) {
				// Here AND is implemented. The distance to every single pick must
				// lie within the configured threshold
				if ( hit == cnt ) {
					++matches;
				}
			}
			else {
				// OR, at least one pick must match
				++matches;
			}
		}
	}

	return matches;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool EventInformation::valid() const {
	return event && preferredOrigin;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool EventInformation::associate(DataModel::Origin *o) {
	if ( !event ) return false;

	event->add(new OriginReference(o->publicID()));
	for ( size_t i = 0; i < o->arrivalCount(); ++i ) {
		if ( !o->arrival(i) ) {
			continue;
		}
		if ( !cfg->eventAssociation.matchingLooseAssociatedPicks
		  && Private::arrivalWeight(o->arrival(i)) == 0 ) {
			continue;
		}
		pickIDs.insert(o->arrival(i)->pickID());
		if ( cfg->eventAssociation.maxMatchingPicksTimeDiff >= 0 ) {
			PickPtr p = cache->get<Pick>(o->arrival(i)->pickID());
			if ( p ) {
				insertPick(p.get());
			}
			else {
				SEISCOMP_WARNING("could not load event pick %s",
				                 o->arrival(i)->pickID().c_str());
			}
		}
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool EventInformation::associate(DataModel::FocalMechanism *fm) {
	if ( !event ) {
		return false;
	}

	event->add(new FocalMechanismReference(fm->publicID()));
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool EventInformation::addJournalEntry(DataModel::JournalEntry *e,
                                       const string &self) {
	journal.push_back(e);

	// Update constraints
	if ( e->action() == "EvPrefMagType" ) {
		constraints.preferredMagnitudeType = e->parameters();
		if ( !constraints.preferredMagnitudeType.empty() ) {
			constraints.fixMw = false;
		}
	}
	else if ( e->action() == "EvPrefOrgID" ) {
		constraints.preferredOriginID = e->parameters();
		constraints.preferredOriginEvaluationMode = Core::None;
	}
	else if ( e->action() == "EvPrefFocMecID" ) {
		constraints.preferredFocalMechanismID = e->parameters();
		constraints.preferredFocalMechanismEvaluationMode = Core::None;
	}
	else if ( e->action() == "EvPrefOrgEvalMode" ) {
		if ( e->parameters().empty() ) {
			constraints.preferredOriginEvaluationMode = Core::None;
			constraints.preferredOriginID = "";
		}
		else {
			DataModel::EvaluationMode em;
			if ( em.fromString(e->parameters().c_str()) ) {
				constraints.preferredOriginID = "";
				constraints.preferredOriginEvaluationMode = em;
			}
			else {
				return false;
			}
		}
	}
	else if ( e->action() == "EvPrefOrgAutomatic" ) {
		constraints.preferredOriginEvaluationMode = Core::None;
		constraints.preferredOriginID = "";
	}
	else if ( e->action() == "EvPrefFocEvalMode" ) {
		if ( e->parameters().empty() ) {
			constraints.preferredFocalMechanismEvaluationMode = Core::None;
			constraints.preferredFocalMechanismID = "";
		}
		else {
			DataModel::EvaluationMode em;
			if ( em.fromString(e->parameters().c_str()) ) {
				constraints.preferredFocalMechanismID = "";
				constraints.preferredFocalMechanismEvaluationMode = em;
			}
			else {
				return false;
			}
		}
	}
	else if ( e->action() == "EvPrefFocAutomatic" ) {
		constraints.preferredFocalMechanismEvaluationMode = Core::None;
		constraints.preferredFocalMechanismID = "";
	}
	else if ( e->action() == "EvPrefMw" ) {
		if ( e->parameters() == "true" ) {
			constraints.fixMw = true;
			constraints.preferredMagnitudeType = "";
		}
		else if ( e->parameters() == "false" ) {
			constraints.fixMw = false;
		}
		// Else keep the last state
	}
	else if ( e->action() == "EvTypeOK" ) {
		if ( e->parameters() == ":unset:" || e->sender() == self ) {
			constraints.fixType = false;
		}
		else {
			constraints.fixType = true;
		}
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool EventInformation::setEventName(DataModel::JournalEntry *e, string &error) {
	if ( !event ) {
		error = ":internal:";
		return false;
	}

	// Check for updating an existing event description
	for ( size_t i = 0; i < event->eventDescriptionCount(); ++i ) {
		if ( event->eventDescription(i)->type() != EARTHQUAKE_NAME ) continue;
		if ( event->eventDescription(i)->text() == e->parameters() ) {
			error = ":no changes:";
			return false;
		}

		event->eventDescription(i)->setText(e->parameters());
		event->eventDescription(i)->update();

		return true;
	}

	// Create a new one
	EventDescriptionPtr ed = new EventDescription(e->parameters(), EARTHQUAKE_NAME);
	if ( !event->add(ed.get()) ) {
		error = ":add:";
		return false;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool EventInformation::setEventOpComment(DataModel::JournalEntry *e, string &error) {
	if ( !event ) {
		error = ":internal:";
		return false;
	}

	// Check for updating an existing event description
	for ( size_t i = 0; i < event->commentCount(); ++i ) {
		if ( event->comment(i)->id() != "Operator" ) continue;
		// Nothing to do
		if ( event->comment(i)->text() == e->parameters() ) {
			error = ":no changes:";
			return false;
		}

		event->comment(i)->setText(e->parameters());

		try {
			event->comment(i)->creationInfo().setModificationTime(Core::Time::UTC());
		}
		catch ( ... ) {
			CreationInfo ci;
			ci.setAuthor(e->sender());
			ci.setModificationTime(Core::Time::UTC());
			event->comment(i)->setCreationInfo(ci);
		}

		event->comment(i)->update();
		return true;
	}

	// Create a new one
	CommentPtr cmt = new Comment();
	cmt->setId("Operator");
	cmt->setText(e->parameters());
	CreationInfo ci;
	ci.setAuthor(e->sender());
	try { ci.setCreationTime(e->created()); }
	catch ( ... ) { ci.setCreationTime(Core::Time::UTC()); }
	cmt->setCreationInfo(ci);

	if ( !event->add(cmt.get()) ) {
		error = ":add:";
		return false;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventInformation::insertPick(Pick *p) {
	string id = p->waveformID().networkCode() + "." + p->waveformID().stationCode();
	picks.insert({ id, p });
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
