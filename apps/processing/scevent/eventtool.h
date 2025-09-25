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
#include <seiscomp/datamodel/publicobjectcache.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/journaling.h>
#include <seiscomp/wired/server.h>

#include <seiscomp/plugins/events/eventprocessor.h>
#include <seiscomp/plugins/events/scoreprocessor.h>

#define SEISCOMP_COMPONENT SCEVENT
#include <seiscomp/logging/log.h>

#include <mutex>
#include <thread>

#include "eventinfo.h"
#include "config.h"


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
		 * @brief Tries to associate the only origin in the EventParameters
		 *        structure and returns the eventID.
		 * No new eventID will be created if the origin cannot be associated
		 * with an event.
		 * @param ep The input EventParameters to be checked. Only one origin
		 *           is allowed.
		 * @return The eventID or an empty string.
		 */
		std::string tryToAssociate(const DataModel::EventParameters *ep);


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

		EventInformationPtr createEvent(DataModel::Origin *origin);
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

		typedef std::list<DelayedObject> DelayBuffer;
		typedef std::list<DelayedEventUpdate> DelayEventBuffer;
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
