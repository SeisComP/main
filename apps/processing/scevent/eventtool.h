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


#ifndef SEISCOMP_APPLICATIONS_EVENTTOOL_H
#define SEISCOMP_APPLICATIONS_EVENTTOOL_H

#include <seiscomp/client/application.h>
#include <seiscomp/core/datetime.h>
#include <seiscomp/datamodel/publicobjectcache.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/journaling.h>
#include <seiscomp/wired/server.h>

#include <seiscomp/plugins/events/eventprocessor.h>
#include <seiscomp/plugins/events/scoreprocessor.h>

#define SEISCOMP_COMPONENT SCEVENT
#include <seiscomp/logging/log.h>

#include <memory>
#include <mutex>
#include <thread>

#include "eventinfo.h"
#include "config.h"
#ifdef SCEVENT_WITH_SQLITE3
#include "allocationstore.h"
#endif


namespace Seiscomp {

namespace DataModel {

class Pick;
class Magnitude;
class Origin;

}

namespace Client {


class EventTool : public Application {
	public:
		EventTool(int argc, char **argv);
		~EventTool();


	public:
		/**
		 * @brief Tries to associate the only origin in the EventParameters structure
		 *        and returns the eventID.
		 * No new eventID will be created if the origin cannot be associated with an
		 * event, unless \p allocate is true in which case a new eventID is reserved on
		 * behalf of the caller (typically a secondary instance asking the main instance
		 * for an ID). The reserved ID and the supplied origin are cached for
		 * eventIDSync.cacheRetention seconds so that a local origin matching the
		 * foreign one can later be assigned the same eventID.
		 * @param ep The input EventParameters to be checked. Only one origin is
		 *        allowed.
		 * @param allocate If true and the origin does not match any existing event,
		 *        reserve a new eventID and cache it. If false, behave as before.
		 * @return The eventID or an empty string.
		 */
		std::string tryToAssociate(const DataModel::EventParameters *ep,
		                           bool allocate = false);


	protected:
		bool initConfiguration() override;
		bool validateParameters() override;

		bool init() override;
		bool run() override;
		void done() override;

		void handleNetworkMessage(const Client::Packet *msg) override;
		void handleMessage(Core::Message *msg) override;
		void handleTimeout() override;

		void addObject(const std::string&, DataModel::Object* object)  override;
		void updateObject(const std::string&, DataModel::Object* object)  override;
		void removeObject(const std::string&, DataModel::Object* object)  override;

		void printUsage() const;


	private:
		enum MatchResult {
			Nothing,
			Location,
			Picks,
			PicksAndLocation
		};

		enum DelayReason {
			SetPreferredFM
		};

		bool handleJournalEntry(DataModel::JournalEntry *);

		EventInformationPtr associateOriginCheckDelay(DataModel::Origin *);
		EventInformationPtr associateOrigin(DataModel::Origin *, bool allowEventCreation,
		                                    bool *createdEvent = nullptr,
		                                    const EventInformation::PickCache *pickCache = nullptr);
		void updatedOrigin(DataModel::Origin *, DataModel::Magnitude *, bool realOriginUpdate);

		EventInformationPtr associateFocalMechanismCheckDelay(DataModel::FocalMechanism *);
		EventInformationPtr associateFocalMechanism(DataModel::FocalMechanism *);
		void updatedFocalMechanism(DataModel::FocalMechanism *, bool realFMUpdate);

		MatchResult compare(EventInformation *info, DataModel::Origin *origin,
		                    const EventInformation::PickCache *cache = nullptr) const;

		EventInformationPtr createEvent(DataModel::Origin *origin,
		                                const std::string &reservedEventID = std::string());
		EventInformationPtr findMatchingEvent(DataModel::Origin *origin,
		                                      const EventInformation::PickCache *cache = nullptr) const;
		EventInformationPtr findAssociatedEvent(DataModel::Origin *origin);
		EventInformationPtr findAssociatedEvent(DataModel::FocalMechanism *fm);

		//! Chooses the preferred origin and magnitude for an event
		void choosePreferred(EventInformation *info, DataModel::Origin *origin,
		                     DataModel::Magnitude *mag,
		                     bool realOriginUpdate = false,
		                     bool refresh = false);

		//! Chooses the preferred focal mechanism an event
		void choosePreferred(EventInformation *info, DataModel::FocalMechanism *fm);

		//! Select the preferred origin again among all associated origins
		void updatePreferredOrigin(EventInformation *info, bool refresh = false);
		void updatePreferredFocalMechanism(EventInformation *info);

		//! Merges two events. Returns false if nothing has been done due to
		//! errors. The source event
		bool mergeEvents(EventInformation *target, EventInformation *source);

		bool checkRegionFilter(const Config::RegionFilters &f, const DataModel::Origin *origin);

		//! Returns the preferred magnitude for an origin
		DataModel::Magnitude *preferredMagnitude(DataModel::Origin *origin);

		DataModel::Event *getEventForOrigin(const std::string &originID);
		DataModel::Event *getEventForFocalMechanism(const std::string &fmID);

		void cacheEvent(EventInformationPtr info);
		EventInformationPtr cachedEvent(const std::string &eventID);
		bool removeCachedEvent(const std::string &eventID);
		bool isEventCached(const std::string &eventID) const;
		void refreshEventCache(EventInformationPtr info);

		void removedFromCache(DataModel::PublicObject *);

		void updateEvent(EventInformation *info, bool = true);
		void updateRegionName(DataModel::Event *ev, DataModel::Origin *org);
		void cleanUpEventCache();

		bool hasDelayedEvent(const std::string &publicID,
		                     DelayReason reason) const;

		//! Asks the configured eventID main instance to allocate an eventID
		//! for \p origin. Returns the main instance's eventID on success or
		//! an empty string on timeout / error / 204 response. Picks
		//! known to the caller can be supplied so that the main instance can
		//! later match local origins against the foreign one.
		std::string queryMainForEventID(
			DataModel::Origin *origin,
			const EventInformation::PickCache *picks = nullptr
		);

		//! Returns the eventID of a cached allocation whose foreign
		//! origin matches \p origin according to the same
		//! time / distance / matching-picks criteria used for live
		//! events, and removes the matched entry from the cache.
		//! Returns an empty string when no match is found. The caller
		//! must hold _associationMutex.
		std::string findAllocatedMatch(DataModel::Origin *origin);

		//! Looks up a reserved eventID in the persistent allocation store
		//! (if configured). First tries a direct origin-publicID match
		//! (fast path), then a windowed epicenter/pick comparison over all
		//! stored origins around the incoming origin's time. Returns an
		//! empty string when no match is found or no store is configured.
		//! The caller must hold _associationMutex.
		std::string findPersistedMatch(
			DataModel::Origin *origin,
			const EventInformation::PickCache *picks);

		//! Persists a reservation (origin time, eventID, origin publicID and
		//! the origin/picks serialized as SCML) to the allocation store, if
		//! configured. The caller must hold _associationMutex.
		void persistAllocation(const std::string &eventID,
		                       const DataModel::EventParameters *ep);

		//! Shared matching predicate used by both the in-memory and the
		//! persistent match paths: true if candidate matches incoming by
		//! shared picks or by epicenter/time within the configured limits.
		bool matchOrigin(
			DataModel::Origin *incoming,
			DataModel::Origin *candidate,
			const EventInformation::PickCache &candidatePicks) const;

		//! Decrements the TTL on every cached allocation and drops
		//! expired entries. Called from handleTimeout(). The caller
		//! must hold _associationMutex.
		void cleanUpAllocatedEventIDs();


	private:
		struct TodoEntry {
			TodoEntry(DataModel::PublicObjectPtr p,
			          DataModel::PublicObjectPtr t = NULL) : primary(p), triggered(t) {}
			DataModel::PublicObjectPtr primary;
			DataModel::PublicObjectPtr triggered;

			DataModel::PublicObject *get() const { return primary.get(); }

			DataModel::PublicObject *operator->() const {
				return primary.get();
			}

			bool operator==(const TodoEntry &other) const {
				return primary == other.primary;
			}

			bool operator<(const TodoEntry &other) const {
				return primary < other.primary;
			}
		};

		typedef DataModel::PublicObjectTimeSpanBuffer Cache;
		typedef std::map<std::string, EventInformationPtr> EventMap;

		// Bit more complicated class to avoid duplicates and to maintain
		// the order of incoming requests
		class TodoList {
			public:
				typedef std::deque<TodoEntry>::iterator iterator;

				void insert(const TodoEntry &e) {
					std::pair<std::set<DataModel::PublicObject*>::iterator, bool> itp;
					itp = _register.insert(e.primary.get());
					if ( !itp.second ) return;
					_entries.push_back(e);
				}

				iterator begin() { return _entries.begin(); }
				iterator end() { return _entries.end(); }

				iterator find(const TodoEntry &e) {
					return std::find(_entries.begin(), _entries.end(), e);
				}

				void erase(const iterator &it) {
					_register.erase(_register.find(it->primary.get()));
					_entries.erase(it);
				}

				void clear() {
					_register.clear();
					_entries.clear();
				}

			private:
				std::set<DataModel::PublicObject*> _register;
				std::deque<TodoEntry> _entries;
		};

		//typedef std::set<TodoEntry> TodoList;

		struct DelayedObject {
			DelayedObject(const DataModel::PublicObjectPtr &o, int t)
			: obj(o), timeout(t){}

			DataModel::PublicObjectPtr obj;
			int timeout;
		};

		struct DelayedEventUpdate {
			DelayedEventUpdate(const std::string &eid, int t, DelayReason r)
			: id(eid), timeout(t), reason(r) {}

			std::string id;
			int timeout;
			DelayReason reason;
		};

		//! Cache entry tracking an eventID reserved on behalf of a
		//! secondary instance along with the foreign origin that triggered the
		//! reservation. When a local origin later matches the foreign
		//! one, the cached eventID is reused so that the main instance and the
		//! secondary instance converge on the same identifier.
		struct AllocatedEventID {
			AllocatedEventID(const std::string &id,
			                 const DataModel::OriginPtr &org,
			                 EventInformation::PickCache pc,
			                 int t)
			: eventID(id), origin(org), picks(std::move(pc)), timeout(t) {}

			std::string                 eventID;
			DataModel::OriginPtr        origin;
			EventInformation::PickCache picks;
			int                         timeout;
		};

		typedef std::list<DelayedObject> DelayBuffer;
		typedef std::list<DelayedEventUpdate> DelayEventBuffer;
		typedef std::list<AllocatedEventID> AllocatedEventIDBuffer;
		typedef std::set<std::string> IDSet;

		typedef std::list<EventProcessorPtr> EventProcessors;

		Client::PacketCPtr            _lastNetworkMessage;
		Cache                         _cache;
		Util::StopWatch               _timer;

		Config                        _config;
		EventProcessors               _processors;
		ScoreProcessorPtr             _score;

		EventMap                      _events;
		DataModel::EventParametersPtr _ep;
		DataModel::JournalingPtr      _journal;

		TodoList                      _adds;
		TodoList                      _updates;
		TodoList                      _realUpdates;
		IDSet                         _originBlackList;
		DelayBuffer                   _delayBuffer;
		DelayEventBuffer              _delayEventBuffer;
		AllocatedEventIDBuffer        _allocatedEventIDs;

#ifdef SCEVENT_WITH_SQLITE3
		//! Optional persistent backing store for reserved eventIDs, enabled
		//! via eventIDSync.db. Survives restarts and can hold a much larger
		//! reservation set than the in-memory buffer above. Only available
		//! when scevent is built with SQLite3 support
		//! (SCEVENT_WITH_SQLITE3).
		AllocationStorePtr            _allocationStore;
		//! Prune store once every minute.
		OPT(Core::Time)               _allocationStorePruneTime;
#endif

		Seiscomp::Wired::ServerPtr    _restAPI;
		std::thread                   _restAPIThread;
		mutable std::mutex            _associationMutex;

		Logging::Channel             *_infoChannel{nullptr};
		Logging::Output              *_infoOutput{nullptr};

		ObjectLog                    *_inputOrigin;
		ObjectLog                    *_inputMagnitude;
		ObjectLog                    *_inputFocalMechanism;
		ObjectLog                    *_inputMomentTensor;
		ObjectLog                    *_inputOriginRef;
		ObjectLog                    *_inputFMRef;
		ObjectLog                    *_inputEvent;
		ObjectLog                    *_inputJournal;
		ObjectLog                    *_outputEvent;
		ObjectLog                    *_outputOriginRef;
		ObjectLog                    *_outputFMRef;
};


}
}

#endif
