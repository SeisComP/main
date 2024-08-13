/***************************************************************************
 *   Copyright (C) by gempa GmbH                                           *
 *                                                                         *
 *   You can redistribute and/or modify this program under the             *
 *   terms of the SeisComP Public License.                                 *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   SeisComP Public License for more details.                             *
 ***************************************************************************/


#define SEISCOMP_COMPONENT SETTYPE

// from base/common repository
#include <seiscomp/client/application.h>
#include <seiscomp/core/plugin.h>
#include <seiscomp/datamodel/event.h>
#include <seiscomp/datamodel/journalentry.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/publicobjectcache.h>
#include <seiscomp/logging/log.h>

// from base/main repository
#include <seiscomp/plugins/events/eventprocessor.h>

ADD_SC_PLUGIN("evtype for scevent: Set event type based on pick or origin comments",
              "Dirk Roessler, gempa GmbH <info@gempa.de>", 0, 1, 0)


using namespace std;
using namespace Seiscomp;

class EventTypeProcessor : public Client::EventProcessor {
	// ----------------------------------------------------------------------
	// X'truction
	// ----------------------------------------------------------------------
	public:
		//! C'tor
		EventTypeProcessor() {}

	// ----------------------------------------------------------------------
	// Public EventProcessor interface
	// ----------------------------------------------------------------------
	public:
		bool setup(const Config::Config &config) {
			try {
				_setEventType = config.getBool("eventType.setEventType");
			}
			catch ( ... ) {}

			try {
				_overwriteEventType = config.getBool("eventType.overwriteEventType");
			}
			catch ( ... ) {}

			try {
				_overwriteManual = config.getBool("eventType.overwriteManual");
			}
			catch ( ... ) {}

			try {
				_pickCommentIDs = config.getStrings("eventType.pickCommentIDs");
			}
			catch ( ... ) {}

			size_t bufferSize = 1000;

			try {
				int value = config.getInt("eventType.cacheSize");
				if ( value < 0 ) {
					SEISCOMP_ERROR("evtype: Invalid cache size: %d", value);
					return false;
				}

				bufferSize = value;
			}
			catch ( ... ) {}

			_cache.setBufferSize(bufferSize);
			_cache.setDatabaseArchive(SCCoreApp->query());

			return true;
		}

		/**
		 * @brief Processes and modifies the event type. It checks the comment of the
		 *        current preferredOrigin for ccloc:eventTypeHint and sets the event
		 *        type to what is found in the comment text.
		 * @param event The event to be processed
		 * @return Update flag: true, if the event type has been updated, false
		 *         otherwise.
		 */
		bool process(DataModel::Event *event, const Journal &journal) {
			SEISCOMP_DEBUG("evtype plugin: working on event %s",
			               event->publicID().c_str());

			// conditions from configuration
			if ( !_setEventType ) {
				SEISCOMP_DEBUG(" + evtype - disregard: setting eventType is "
				               "disabled by configuration");
				return false;
			}

			if ( _pickCommentIDs.empty() ) {
				SEISCOMP_DEBUG(" + evtype - disregard: setting eventType is "
				               "disabled by empty configuration of commentIDs");
				return false;
			}

			// consider preferred origin only
			DataModel::OriginPtr org = DataModel::Origin::Find(event->preferredOriginID());
			if ( !org ) {
				SEISCOMP_WARNING(" + evtype - skipping event: found no preferred origin");
				return false;
			}

			OPT(DataModel::EventType) currentType;
			try {
				currentType = event->type();
			}
			catch ( ... ) { }

			bool isTypeFixed = false;
			for ( const auto &entry : journal ) {
				if ( (entry->action() != "EvType") || (entry->sender() == SCCoreApp->author()) ) {
					continue;
				}
				isTypeFixed = !entry->parameters().empty();
				if ( isTypeFixed && !_overwriteManual ) {
					SEISCOMP_DEBUG(" + evtype: type of event %s set/unset through journal: ignoring",
					               event->publicID().c_str());
					return false;
				}
			}

			/* By default overwriting is allowed, since for a single event the
			 * majority of event types suggested by picks may differ for
			 * different origins.
			*/
			if ( currentType && !_overwriteEventType ) {
				SEISCOMP_DEBUG(" + evtype - disregard: overwriting eventType is disabled by configuration");
				return false;
			}


			// set event type by evaluating the pick/origin comments
			size_t nArrivals{0};
			DataModel::EventType eventTypeUpdate;

			// initialize counter
			struct TypeCounter {
				string evtType;
				size_t count;
			};
			vector<TypeCounter> counter;
			for ( int i = static_cast<int>(DataModel::EventType::First);
			      i < static_cast<int>(DataModel::EventType::Quantity); ++i ) {
				DataModel::EventType type;
				type.fromInt(i);
				counter.push_back({type.toString(), 0});
			}

			SEISCOMP_DEBUG(" + evtype - processing preferred origin %s",
			               org->publicID().c_str());
			try {
				if ( org->evaluationMode() == DataModel::MANUAL && !_overwriteManual ) {
					SEISCOMP_DEBUG("   + evtype - disregard: setting the "
					               "event type for manual origins is "
					               "disabled by configuration");
					return false;
				}
			}
			catch ( ... ) {}

			// count the different event types
			nArrivals = org->arrivalCount();
			for ( size_t i = 0; i < org->arrivalCount(); ++i ) {
				DataModel::Arrival *arrival = org->arrival(i);
				DataModel::PickPtr pick = _cache.get<DataModel::Pick>(arrival->pickID());
				if ( !pick ) {
					continue;
				}

				if ( (pick->commentCount() == 0) && SCCoreApp->query() ) {
					    SCCoreApp->query()->loadComments(pick.get());
				}
				for ( const auto &name : _pickCommentIDs ) {
					DataModel::CommentPtr comment = pick->comment(DataModel::CommentIndex(name));
					if ( comment ) {
						for ( auto &evtType : counter ) {
							if ( evtType.evtType == comment->text() ) {
								evtType.count++;
							}
						}
					}
				}
			}

			// evaluate the number of found event types
			size_t count = 0;
			for ( const auto &evtType : counter ) {
				// If count is equal, the last ocurrance takes priority
				if ( (evtType.count == count) && (evtType.count != 0) ) {
					SEISCOMP_DEBUG(" + evtype: found equal number of picks "
					               "classified with different types. "
					               "Ignoring previous one: %s",
					               eventTypeUpdate.toString());
					eventTypeUpdate.fromString(evtType.evtType);
				}
				else if ( evtType.count > count ) {
					eventTypeUpdate.fromString(evtType.evtType);
					count = evtType.count;
				}
			}

			if ( count > 0 ) {
				try {
					if ( eventTypeUpdate == event->type() ) {
						SEISCOMP_DEBUG(" + evtype: previous and new event types "
						               "are identical. Skip setting type again.");
						return true;
					}
				}
				catch ( ... ) {}

				SEISCOMP_DEBUG(" + evtype: setting event type to %s from %i/%i classified picks",
				               eventTypeUpdate.toString(), count, nArrivals);
				event->setType(eventTypeUpdate);
			}
			else {
				SEISCOMP_DEBUG(" + evtype: found no type comment in picks");
			}

			return true;
		}

	private:
		DataModel::PublicObjectRingBuffer _cache;
		bool                              _setEventType{false};
		bool                              _overwriteEventType{true};
		bool                              _overwriteManual{false};
		vector<string>                    _pickCommentIDs{"scrttv:eventTypeHint","deepc:eventTypeHint"};
};

REGISTER_EVENTPROCESSOR(EventTypeProcessor, "EventType");
