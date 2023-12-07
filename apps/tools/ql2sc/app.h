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


#ifndef SEISCOMP_QL2SC_APP_H__
#define SEISCOMP_QL2SC_APP_H__

#include "config.h"
#include "quakelink.h"

#include <seiscomp/client/application.h>
#include <seiscomp/core/datetime.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/publicobjectcache.h>
#include <seiscomp/datamodel/diff.h>

#include <string>

namespace Seiscomp {

namespace DataModel {
	class FocalMechanism;
	class Magnitude;
	class Origin;
	class Event;
}

namespace IO {
	namespace QuakeLink {
		class Response;
	}
}

namespace QL2SC {


class SC_SYSTEM_CORE_API NoCache : public DataModel::PublicObjectCache {
	public:
		bool feed(DataModel::PublicObject* po) { push(po); return true; }
};


class App : public Client::Application {
	public:
		App(int argc, char **argv);
		~App();

	public:
		void feed(QLClient *client, IO::QuakeLink::Response *response);

	protected:
		typedef std::vector<QLClient*> QLClients;
		typedef DataModel::Diff2::Notifiers Notifiers;
		typedef DataModel::Diff2::LogNode LogNode;
		typedef DataModel::Diff2::LogNodePtr LogNodePtr;

		void createCommandLineDescription() override;

		bool init() override;
		bool run() override;
		void done() override;

		bool dispatchNotification(int type, BaseObject *obj) override;
		void addObject(const std::string& parentID, DataModel::Object *obj) override;
		void updateObject(const std::string& parentID, DataModel::Object *obj) override;
		void removeObject(const std::string& parentID, DataModel::Object *obj) override;
		void handleTimeout() override;

		bool dispatchResponse(QLClient *client, const IO::QuakeLink::Response *response);

		template <class T>
		void diffPO(T *remotePO, const std::string &parentID,
		            Notifiers &notifiers, LogNode *logNode = NULL);

		void syncEvent(const DataModel::EventParameters *ep,
		               const DataModel::Event *event,
		               const RoutingTable *routing,
		               Notifiers &notifiers, bool syncPreferred);

		bool sendNotifiers(const Notifiers &notifiers, const RoutingTable &routing);
		bool sendJournals(const Notifiers &journals);
		void applyNotifier(const DataModel::Notifier *n);
		void readLastUpdates();
		void writeLastUpdates();

		DataModel::JournalEntry *createJournalEntry(const std::string &id,
		                                            const std::string &action,
		                                            const std::string &params);

		std::string waitForEventAssociation(const std::string &originID, int timeout);
		void originAssociatedWithEvent(const std::string &eventID,
		                               const std::string &originID);

	private:
		struct EventDelayItem {
			DataModel::EventParametersPtr ep;
			DataModel::Event *event; // Pointer from ep
			const HostConfig *config;
			int timeout;
		};

		using EventDelayBuffer = std::map<std::string, EventDelayItem>;

		Config                   _config;
		QLClients                _clients;
		NoCache                  _cache;
		boost::mutex             _clientPublishMutex;
		std::string              _lastUpdateFile;
		EventDelayBuffer         _eventDelayBuffer;
		std::string              _waitForEventIDOriginID;
		std::string              _waitForEventIDResult;
		int                      _waitForEventIDTimeout;
		std::vector<std::string> _ep;
		bool                     _test;
};

} // ns QL2SC
} // ns Seiscomp

#endif // SEISCOMP_QL2SC_APP_H__
