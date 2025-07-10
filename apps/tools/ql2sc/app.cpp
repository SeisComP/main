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


#define SEISCOMP_COMPONENT QL2SC

#include "app.h"

#include <seiscomp/system/environment.h>

#include <seiscomp/datamodel/amplitude.h>
#include <seiscomp/datamodel/event.h>
#include <seiscomp/datamodel/focalmechanism.h>
#include <seiscomp/datamodel/momenttensor.h>
#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/magnitude.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/journalentry.h>
#include <seiscomp/io/archive/xmlarchive.h>
#include <seiscomp/logging/log.h>
#include <seiscomp/utils/files.h>

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>


using namespace std;
using namespace Seiscomp::DataModel;


namespace io = boost::iostreams;


namespace Seiscomp {
namespace QL2SC {
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
namespace {


using PropertyIndex = Diff3::PropertyIndex;


PropertyIndex CreationInfoIndex;


OPT(Core::Time) getLastModificationTime(const CreationInfo &ci) {
	try {
		return ci.modificationTime();
	}
	catch ( ... ) {
		try {
			return ci.creationTime();
		}
		catch ( ... ) {}
	}

	return Core::None;
}


inline OPT(Core::Time) getLastModificationTime(const CreationInfo *ci) {
	return getLastModificationTime(*ci);
}


template <typename T>
OPT(Core::Time) getLastModificationTime(const T *o) {
	try {
		return getLastModificationTime(o->creationInfo());
	}
	catch ( ... ) {}

	return Core::None;
}


template <typename T>
inline bool checkUpdateOnTimeStamps(const T *remoteO, const T *localO) {
	return getLastModificationTime(remoteO) > getLastModificationTime(localO);
}


class MyDiff : public Diff4 {
	public:
		MyDiff(const Config &cfg) : _cfg(cfg) {}

	protected:
		bool blocked(const Core::BaseObject *o, LogNode *node, bool local) {
			auto po = PublicObject::ConstCast(o);
			if ( po ) {
				if ( _cfg.publicIDFilter.isDenied(po->publicID()) ) {
					if ( node && node->level() >= LogNode::DIFFERENCES )
						node->addChild(o2t(o), "SKIP (" + string(local?"local":"remote") +
						               " publicID '" + po->publicID() + "' blocked)");
					return true;
				}
			}

			const Core::MetaProperty *prop = 0;
			auto it = CreationInfoIndex.find(o->className());
			if ( it == CreationInfoIndex.end() ) {
				if ( o->meta() ) {
					prop = o->meta()->property("creationInfo");
					CreationInfoIndex[o->className()] = prop;
				}
			}
			else {
				prop = it->second;
			}

			if ( !prop ) {
				return false;
			}

			string agencyID;

			try {
				Core::MetaValue v = prop->read(o);
				Core::BaseObject *bo = Core::metaValueCast<Core::BaseObject*>(v);
				CreationInfo *ci = CreationInfo::Cast(bo);
				if ( ci ) {
					agencyID = ci->agencyID();

					if ( !SCCoreApp->isAgencyIDAllowed(agencyID) ) {
						if ( node && node->level() >= LogNode::DIFFERENCES ) {
							node->addChild(o2t(o), "SKIP (" + string(local?"local":"remote") +
							               " agency '" + agencyID + "' blocked)");
						}
						return true;
					}
				}
			}
			catch ( ... ) {}

			return false;
		}

		bool confirmUpdate(const Core::BaseObject *localO,
		                   const Core::BaseObject *remoteO,
		                   LogNode *node) {
			const Core::MetaProperty *prop = nullptr;
			auto it = CreationInfoIndex.find(localO->className());
			if ( it == CreationInfoIndex.end() ) {
				if ( localO->meta() ) {
					prop = localO->meta()->property("creationInfo");
					CreationInfoIndex[localO->className()] = prop;
				}
			}
			else {
				prop = it->second;
			}

			// No creationInfo, no creationTime check possible
			if ( !prop ) {
				return true;
			}

			try {
				Core::MetaValue v;
				CreationInfo *ciLocal, *ciRemote;

				v = prop->read(localO);
				ciLocal = CreationInfo::Cast(Core::metaValueCast<Core::BaseObject*>(v));

				v = prop->read(remoteO);
				ciRemote = CreationInfo::Cast(Core::metaValueCast<Core::BaseObject*>(v));

				if ( ciLocal && ciRemote ) {
					if ( !checkUpdateOnTimeStamps(ciRemote, ciLocal) ) {
						if ( node && node->level() >= LogNode::OPERATIONS ) {
							node->setMessage("SKIP UPDATE due to modification time");
						}
						return false;
					}
				}
			}
			catch ( ... ) {}

			return true;
		}

		bool confirmRemove(const Core::BaseObject *localO,
		                   LogNode *node) override {
			const Core::MetaProperty *prop = nullptr;

			if ( _cfg.allowRemoval ) {
				return true;
			}

			auto it = CreationInfoIndex.find(localO->className());
			if ( it == CreationInfoIndex.end() ) {
				if ( localO->meta() ) {
					prop = localO->meta()->property("creationInfo");
					CreationInfoIndex[localO->className()] = prop;
				}
			}
			else {
				prop = it->second;
			}

			if ( !prop ) {
				// If the object does not have a creationInfo then it is considered
				// a part of the parent and is allowed to be removed.
				return true;
			}

			if ( node ) {
				node->addChild(o2t(localO), "SKIP REMOVE due to configuration");
			}

			return false;
		}

		bool confirmDescent(const Core::BaseObject *,
		                    const Core::BaseObject *,
		                    bool updateConfirmed,
		                    const Core::MetaProperty *prop,
		                    LogNode *) override {
			// If the object type of the child property does not contain
			// a creationInfo then we consider that as part of the object
			// and do not descent if the update of the parent is not
			// confirmed. Otherwise pass the modification check on to the
			// childs.
			if ( prop->isClass() && !prop->type().empty() ) {
				const Core::MetaProperty *propCreationInfo = nullptr;
				auto it = CreationInfoIndex.find(prop->type());
				if ( it == CreationInfoIndex.end() ) {
					auto meta = Core::MetaObject::Find(prop->type());
					if ( meta ) {
						propCreationInfo = meta->property("creationInfo");
						CreationInfoIndex[prop->type()] = propCreationInfo;
					}
				}
				else {
					propCreationInfo = it->second;
				}

				if ( propCreationInfo ) {
					return true;
				}
			}

			return updateConfirmed;
		}

	private:
		const Config &_cfg;
};


template<typename Container> class container_source {
	public:
		using char_type = typename Container::value_type;
		using category = io::source_tag;
		container_source(const Container& container)
		 : _con(container), _pos(0) {}
		streamsize read(char_type* s, streamsize n) {
			streamsize amt = static_cast<streamsize>(_con.size() - _pos);
			streamsize result = (min)(n, amt);
			if (result != 0) {
				copy(_con.begin() + _pos, _con.begin() + _pos + result, s);
				_pos += result;
				return result;
			}
			return -1; // EOF
		}
		Container& container() { return _con; }
	private:
		using size_type = typename Container::size_type;
		const Container&  _con;
		size_type   _pos;
};


bool load(EventParametersPtr &ep, JournalingPtr &ej,
          const string &data, bool gzip = false) {
	bool retn = false;
	bool registrationEnabled = PublicObject::IsRegistrationEnabled();
	PublicObject::SetRegistrationEnabled(false);
	try {
		io::filtering_istreambuf buf;
		container_source<string> src(data);
		if ( gzip ) {
			buf.push(io::gzip_decompressor());
		}
		buf.push(src);

		IO::XMLArchive ar;
		if ( !ar.open(&buf) )
			SEISCOMP_ERROR("[xml] could not open stream buffer for reading");
		else {
			ar >> ep >> ej;
			retn = ar.success() && ep;
			ar.close();
		}
	}
	catch (string &e) {
		SEISCOMP_ERROR("[xml] %s", e.c_str());
	}
	catch (exception &e) {
		SEISCOMP_ERROR("[xml] %s", e.what());
	}
	PublicObject::SetRegistrationEnabled(registrationEnabled);
	return retn;
}


/** Adds all PublicObjects to a cache */
class SC_SYSTEM_CORE_API PublicObjectCacheFeeder : protected Visitor {
	public:
		PublicObjectCacheFeeder(PublicObjectCache &cache)
		 : _cache(cache), _root(nullptr) {}

		void feed(Object *o, bool skipRoot = false) {
			_root = skipRoot ? o : nullptr;
			if ( o )
				o->accept(this);
		}

	private:
		bool visit(PublicObject* po) {
			if ( _root && _root == po ) // skip root node
				return true;
			_cache.feed(po);
			return true;
		}

		void visit(Object* o) {}

	private:
		PublicObjectCache &_cache;
		Object *_root;
};


/** Recursively resolves routing for a given object */
bool checkRouting(const Object *o, const RoutingTable &routing) {
	if ( !o ) return false;

	RoutingTable::const_iterator it = routing.find(o->typeInfo().className());
	if ( it != routing.end() )
		return !it->second.empty();

	return checkRouting(o->parent(), routing);
}


bool resolveRouting(string &result, const Object *o, const RoutingTable &routing) {
	if ( !o ) return false;

	RoutingTable::const_iterator it = routing.find(o->typeInfo().className());
	if ( it != routing.end() ) {
		result = it->second;
		return !result.empty();
	}

	return resolveRouting(result, o->parent(), routing);
}


JournalEntryPtr getLastJournalEntry(DatabaseQuery &query, const string &eventID,
                                    const string &action) {
	JournalEntryPtr journalEntry;
	DatabaseIterator it = query.getJournalAction(eventID, action);
	for ( ; it.valid(); ++it ) {
		JournalEntryPtr tmp = JournalEntry::Cast(it.get());
		if ( !journalEntry )
			journalEntry = tmp;
		else {
			try {
				if ( journalEntry->created() < tmp->created() )
					journalEntry = tmp;
			}
			catch ( ... ) {
				// No creation time information, take the latest with respect
				// to database order
				journalEntry = tmp;
			}
		}
	}
	it.close();
	return journalEntry;
}


JournalEntryPtr getLastJournalEntry(const Journaling *journals, const string &eventID,
                                    const string &action) {
	if ( journals ) {
		for ( size_t i = 0; i < journals->journalEntryCount(); ++i ) {
			auto entry = journals->journalEntry(i);
			if ( (entry->objectID() == eventID) && (entry->action() == action) ) {
				return entry;
			}
		}
	}

	return nullptr;
}


} // ns anonymous
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
App::App(int argc, char **argv)
: Client::Application(argc, argv)
, _waitForEventIDTimeout(0)
, _test(false) {
	setDatabaseEnabled(true, true);
	setMessagingEnabled(true);
	setPrimaryMessagingGroup("EVENT");
	addMessagingSubscription("EVENT");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
App::~App() {
	for ( auto *client : _clients ) {
		if ( client ) {
			delete client;
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::createCommandLineDescription() {
	commandline().addGroup("Mode");
	commandline().addOption("Mode", "test", "Do not send messages, just log output");
	commandline().addOption("Mode", "ep", "Check differences with given XML file and exit, can be given more than once", &_ep);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::init() {
	if ( !Client::Application::init() )
		return false;

	_test = commandline().hasOption("test");

	if ( !_config.init() )
		return false;

	int notificationID = -2;
	for ( HostConfigs::const_iterator it = _config.hosts.begin();
	      it != _config.hosts.end(); ++it, --notificationID ) {
		SEISCOMP_INFO("Initializing host '%s'", it->host.c_str());
		QLClient *client = new QLClient(notificationID, &*it, _config.backLog);
		_clients.push_back(client);
		if ( !client->init(it->url, it->options) ) {
			SEISCOMP_ERROR("Failed to initialize host '%s'", it->host.c_str());
			return false;
		}
	}

	// read previous update times
	string baseDir = Environment::Instance()->installDir() + "/var/lib";
	_lastUpdateFile = baseDir + "/" + name() + ".last_update";
	Util::createPath(baseDir);
	readLastUpdates();

	_cache.setDatabaseArchive(query());

	enableTimer(1);

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::run() {
	if ( _ep.empty() ) {
		for ( auto &client : _clients ) {
			client->run();
		}

		return Client::Application::run();
	}
	else {
		for ( size_t ei = 0; ei < _ep.size(); ++ei ) {
			SEISCOMP_INFO("---- %s", _ep[ei].c_str());

			// Offline processing
			EventParametersPtr ep;
			JournalingPtr ej;
			Notifiers notifiers, journals;
			LogNodePtr logNode;

			bool registrationEnabled = PublicObject::IsRegistrationEnabled();
			PublicObject::SetRegistrationEnabled(false);

			IO::XMLArchive ar;
			if ( !ar.open(_ep[ei].c_str()) ) {
				cerr << "Failed to open " << _ep[ei] << endl;
				return false;
			}

			ar >> ep >> ej;

			ar.close();

			PublicObject::SetRegistrationEnabled(registrationEnabled);

			// log node is enabled for notice and debug level
			if ( _baseSettings.logging.verbosity > 2 )
				logNode = new LogNode(
					EventParameters::TypeInfo().className(),
					_baseSettings.logging.verbosity > 3
					?
					LogNode::DIFFERENCES
					:
					LogNode::OPERATIONS
				);

			const string &epID = ep->publicID();

			for ( size_t i = 0; i < ep->pickCount(); ++i ) {
				diffPO(ep->pick(i), epID, notifiers, logNode.get());
			}

			// Amplitudes
			for ( size_t i = 0; i < ep->amplitudeCount(); ++i ) {
				diffPO(ep->amplitude(i), epID, notifiers, logNode.get());
			}

			// Origins
			for ( size_t i = 0; i < ep->originCount(); ++i ) {
				diffPO(ep->origin(i), epID, notifiers, logNode.get());
			}

			// FocalMechanisms
			for ( size_t i = 0; i < ep->focalMechanismCount(); ++i ) {
				diffPO(ep->focalMechanism(i), epID, notifiers, logNode.get());
			}

			// Events
			for ( size_t i = 0; i < ep->eventCount(); ++i ) {
				diffPO(ep->event(i), epID, notifiers, logNode.get());
			}

			// log diffs
			if ( logNode.get() && logNode->childCount() ) {
				stringstream ss;
				ss << endl;
				logNode->write(ss);
				if ( _baseSettings.logging.verbosity > 3 ) {
					SEISCOMP_DEBUG_S(ss.str());
				}
				else {
					SEISCOMP_INFO_S(ss.str());
				}
			}

			// No event routing, forward event attributes
			for ( size_t i = 0; i < ep->eventCount(); ++i ) {
				syncEvent(ep.get(), ej.get(), ep->event(i), nullptr, journals, true);
			}

			cerr << "Notifiers: " << notifiers.size() << endl;
			cerr << "Journals: " << journals.size() << endl;

			for ( size_t i = 0; i < notifiers.size(); ++i ) {
				applyNotifier(notifiers[i].get());
			}

			for ( size_t i = 0; i < journals.size(); ++i ) {
				applyNotifier(journals[i].get());
			}

			ar.create("-");
			ar.setFormattedOutput(true);
			ar & NAMED_OBJECT("", notifiers);
			ar & NAMED_OBJECT("", journals);
			ar.close();
		}

		return true;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::done() {
	// Flush delayed events
	for ( auto &[eventID, item] : _eventDelayBuffer ) {
		auto ep = item.ep;
		auto ej = item.ej;
		auto event = item.event;
		auto config = item.config;
		auto &routing = config->routingTable;

		Notifiers journals;
		// No event routing, forward event attributes
		syncEvent(ep.get(), ej.get(), event, &routing,
		          journals, config->syncPreferred);
		sendJournals(journals);
	}

	// Wait for threads to terminate
	if ( _ep.empty() ) {
		for ( auto client : _clients ) {
			client->join();
		}
	}

	// Disconnect from messaging
	Client::Application::done();

	SEISCOMP_INFO("application finished");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::feed(QLClient *client, IO::QuakeLink::Response *response) {
	_clientPublishMutex.lock();

	Client::Notification n(client->notificationID(), response);
	if ( !_queue.push(n) ) {
		_clientPublishMutex.unlock();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::dispatchNotification(int type, Core::BaseObject *obj) {
	if ( type >= 0 ) {
		return false;
	}

	if ( type == -1 ) {
		return true;
	}

	size_t index = -type - 2;
	if ( index >= _clients.size() ) {
		return false;
	}

	auto client = _clients[index];
	auto msg = IO::QuakeLink::Response::Cast(obj);
	if ( !msg ) {
		SEISCOMP_ERROR("received invalid message from host '%s'",
		               client->config()->host.c_str());
		return true;
	}

	if ( client->config()->delay > 0 ) {
		SEISCOMP_INFO("Delaying message from %s for %d seconds",
		              client->config()->host, client->config()->delay);
		_qlDelayBuffer.push_back({ client, msg, client->config()->delay });
	}
	else {
		handleQLMessage(client, msg);
	}

	// Allow new responses
	_clientPublishMutex.unlock();

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::handleQLMessage(QLClient *client, const IO::QuakeLink::Response *msg) {
	bool res = dispatchResponse(client, msg);

	// The cache is only maintained for one particular dispatch run because
	// updates of all objects are not captured.
	_cache.clear();

	return res;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::dispatchResponse(QLClient *client, const IO::QuakeLink::Response *msg) {
	const HostConfig *config = client->config();
	const RoutingTable &routing = config->routingTable;
	RoutingTable::const_iterator rt_it;

	SEISCOMP_INFO("Processing message from host '%s'", config->host.c_str());

	Notifiers notifiers;

	// log node is enabled for notice and debug level
	LogNodePtr logNode;
	if ( _baseSettings.logging.verbosity > 2 )
		logNode = new LogNode(DataModel::EventParameters::TypeInfo().className(),
		                      _baseSettings.logging.verbosity > 3 ? LogNode::DIFFERENCES : LogNode::OPERATIONS);

	EventParametersPtr ep;
	JournalingPtr ej;

	// event remove message
	if ( msg->disposed ) {
		if ( msg->type != IO::QuakeLink::ctText ) {
			SEISCOMP_ERROR("Content-Type of message not set to text");
			return false;
		}

		// Event removal will be ignored
		return true;
	}

	// event update
	if ( msg->type != IO::QuakeLink::ctXML ) {
		SEISCOMP_ERROR("Content-Type of message not set to XML");
		return false;
	}

	if ( !load(ep, ej, msg->data, msg->gzip) ) {
		return false;
	}

	if ( isExitRequested() ) {
		return false;
	}

	const string &epID = ep->publicID();

	// check if routing for EventParameters exists
	string epRouting;
	rt_it = routing.find(ep->typeInfo().className());
	if ( rt_it != routing.end() ) {
		epRouting = rt_it->second;
	}

	// Picks
	if ( !epRouting.empty() ||
		 routing.find(Pick::TypeInfo().className()) != routing.end() ) {
		for ( size_t i = 0; i < ep->pickCount(); ++i ) {
			if ( isExitRequested() ) {
				return false;
			}
			diffPO(ep->pick(i), epID, notifiers, logNode.get());
		}
	}

	// Amplitudes
	if ( !epRouting.empty() ||
		 routing.find(Amplitude::TypeInfo().className()) != routing.end() ) {
		for ( size_t i = 0; i < ep->amplitudeCount(); ++i ) {
			if ( isExitRequested() ) {
				return false;
			}
			diffPO(ep->amplitude(i), epID, notifiers, logNode.get());
		}
	}

	// Origins
	if ( !epRouting.empty() ||
		 routing.find(Origin::TypeInfo().className()) != routing.end() ) {
		for ( size_t i = 0; i < ep->originCount(); ++i ) {
			if ( isExitRequested() ) {
				return false;
			}
			diffPO(ep->origin(i), epID, notifiers, logNode.get());
		}
	}

	// FocalMechanisms
	if ( !epRouting.empty() ||
		 routing.find(FocalMechanism::TypeInfo().className()) != routing.end() ) {
		for ( size_t i = 0; i < ep->focalMechanismCount(); ++i ) {
			if ( isExitRequested() ) {
				return false;
			}
			diffPO(ep->focalMechanism(i), epID, notifiers, logNode.get());
		}
	}

	// Events
	if ( !epRouting.empty() ||
		 routing.find(Event::TypeInfo().className()) != routing.end() ) {
		auto it = routing.find(Event::TypeInfo().className());
		if ( it != routing.end() && !it->second.empty() ) {
			for ( size_t i = 0; i < ep->eventCount(); ++i ) {
				if ( isExitRequested() ) {
					return false;
				}
				diffPO(ep->event(i), epID, notifiers, logNode.get());
			}
		}
	}

	if ( isExitRequested() ) {
		return false;
	}

	// log diffs
	if ( logNode.get() && logNode->childCount() ) {
		stringstream ss;
		ss << endl;
		logNode->write(ss);
		if ( _baseSettings.logging.verbosity > 3 ) {
			SEISCOMP_DEBUG_S(ss.str());
		}
		else {
			SEISCOMP_INFO_S(ss.str());
		}
	}

	if ( !_test ) {
		if ( sendNotifiers(config->syncEventAttributes ? ep.get() : nullptr, notifiers, routing) ) {
			if ( config->syncEventAttributes ) {
				if ( config->syncEventDelay > 0 ) {
					for ( size_t i = 0; i < ep->eventCount(); ++i ) {
						auto event = ep->event(i);
						auto itp = _eventDelayBuffer.insert({
							event->publicID(), {
								ep, ej, event, config,
								// Add one to incorporate the current
								// running ticker.
								config->syncEventDelay + 1
							}
						});

						if ( !itp.second ) {
							// Element exists already, update the contents
							itp.first->second.ep = ep;
							itp.first->second.event = event;
							SEISCOMP_INFO("%s: updated delay buffer",
							              event->publicID(), config->syncEventDelay);
						}
						else {
							SEISCOMP_INFO("%s: synchronization delayed for %ds",
							              event->publicID(), config->syncEventDelay);
						}
					}
				}
				else {
					Notifiers journals;
					// No event routing, forward event attributes
					for ( size_t i = 0; i < ep->eventCount(); ++i ) {
						syncEvent(ep.get(), ej.get(), ep->event(i), &routing,
						          journals, config->syncPreferred);
					}
					sendJournals(journals);
				}
			}
			client->setLastUpdate(msg->timestamp);
			writeLastUpdates();
			return true;
		}
	}
	else {
		for ( auto &n : notifiers ) {
			if ( n->object() ) {
				applyNotifier(n.get());
			}
		}
		client->setLastUpdate(msg->timestamp);
		writeLastUpdates();
		return true;
	}

	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::addObject(const string& parentID, Object *obj) {
	PublicObject *po = PublicObject::Cast(obj);
	if ( po ) {
		Event *event = Event::Cast(po);
		if ( !event ) {
			_cache.feed(po);
		}
	}
	else if ( !_waitForEventIDOriginID.empty() ) {
		OriginReference *ref = OriginReference::Cast(obj);
		if ( ref ) {
			if ( ref->originID() == _waitForEventIDOriginID ) {
				originAssociatedWithEvent(parentID, ref->originID());
			}
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::updateObject(const std::string& parentID, DataModel::Object *obj) {
	addObject(parentID, obj);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::removeObject(const string& parentID, Object *obj) {
	PublicObject *po = PublicObject::Cast(obj);
	if ( po ) {
		_cache.remove(po);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::handleTimeout() {
	if ( _waitForEventIDTimeout > 0 ) {
		--_waitForEventIDTimeout;
		if ( _waitForEventIDTimeout <= 0 ) {
			_waitForEventIDOriginID = string();
			// Just wake up the event queue
			sendNotification(Client::Notification(-1));
		}

		return;
	}

	for ( auto it = _qlDelayBuffer.begin(); it != _qlDelayBuffer.end(); ) {
		auto &item = *it;
		--item.timeout;
		if ( item.timeout <= 0 ) {
			handleQLMessage(item.client, item.response.get());
			it = _qlDelayBuffer.erase(it);
		}
		else {
			++it;
		}
	}

	for ( auto it = _eventDelayBuffer.begin(); it != _eventDelayBuffer.end(); ) {
		auto &item = it->second;
		--item.timeout;
		if ( item.timeout <= 0 ) {
			auto ep = item.ep;
			auto ej = item.ej;
			auto event = item.event;
			auto config = item.config;
			auto &routing = config->routingTable;

			Notifiers journals;
			SEISCOMP_DEBUG("%s: synchronize delayed event",
			               event->publicID());
			// No event routing, forward event attributes
			syncEvent(ep.get(), ej.get(), event, &routing,
			          journals, config->syncPreferred);
			sendJournals(journals);

			it = _eventDelayBuffer.erase(it);
		}
		else {
			++it;
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
template <class T>
void App::diffPO(T *remotePO, const string &parentID, Notifiers &notifiers,
                 LogNode *logNode) {
	if ( !remotePO ) {
		return;
	}

	// search corresponding object in cache
	Core::SmartPointer<T> localPO;
	localPO = T::Cast(_cache.find(remotePO->typeInfo(), remotePO->publicID()));

	// if object was not found in cache but loaded from database, all of its
	// child objects have to be loaded too
	if ( localPO && !_cache.cached() && query() ) {
		query()->load(localPO.get());
		PublicObjectCacheFeeder(_cache).feed(localPO.get(), true);
	}

	MyDiff diff(_config);
	diff.diff(localPO.get(), remotePO, parentID, notifiers, logNode);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::originAssociatedWithEvent(const std::string &eventID,
                                    const std::string &originID) {
	_waitForEventIDOriginID = string();
	_waitForEventIDTimeout = 0;
	_waitForEventIDResult = eventID;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
string App::waitForEventAssociation(const std::string &originID, int timeout) {
	// Setup state variables
	_waitForEventIDOriginID = originID;
	_waitForEventIDResult = string();
	// Wait a least 10 seconds for event association
	_waitForEventIDTimeout = timeout;

	while ( !_waitForEventIDOriginID.empty() ) {
		if ( !waitEvent() ) {
			break;
		}

		if ( !_waitForEventIDResult.empty() ) {
			return _waitForEventIDResult;
		}

		idle();
	}

	return string();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
JournalEntry *App::createJournalEntry(const string &id, const string &action, const string &params,
                                      const Core::Time *created) {
	JournalEntry *entry = new JournalEntry;
	entry->setCreated(created ? *created : Core::Time::UTC());
	entry->setObjectID(id);
	entry->setSender(author());
	entry->setAction(action);
	entry->setParameters(params);
	return entry;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
template <typename R, typename T>
void App::checkUpdate(Notifiers &notifiers,
                      R (T::*func)() const, const T *remote, const T *local,
                      const char *name, const Journaling *journals,
                      const std::string &action) {
	typename OPT(remove_const_t<std::remove_reference_t<R>>) rR, lR;

	try {
		rR = (remote->*func)();
	}
	catch ( ... ) {}

	try {
		lR = (local->*func)();
	}
	catch ( ... ) {}

	if ( rR == lR ) {
		SEISCOMP_DEBUG("* check update of %s: '%s' => equal", name, rR ? Core::toString(*rR) : "");
		return;
	}

	SEISCOMP_DEBUG("* check update of %s: '%s'", name, rR ? Core::toString(*rR) : "");

	OPT(Core::Time) remoteChangeTime;
	try {
		remoteChangeTime = remote->creationInfo().modificationTime();
		SEISCOMP_DEBUG("  - remote change time is modification time %s",
		               remoteChangeTime->iso());
	}
	catch ( ... ) {
		try {
			remoteChangeTime = remote->creationInfo().creationTime();
			SEISCOMP_DEBUG("  - remote change time is creation time %s",
			               remoteChangeTime->iso());
		}
		catch ( ... ) {}
	}

	// Fetch last remote journal entry to set the event type
	auto entry = getLastJournalEntry(journals, remote->publicID(), action);
	if ( entry ) {
		auto rStrR = rR ? Core::toString(rR) : string();
		if ( entry->parameters() != rStrR ) {
			SEISCOMP_WARNING("%s: %s is not matching the last journal type, ignoring it",
			                 remote->publicID(), name);
		}
		else {
			try {
				remoteChangeTime = entry->created();
				SEISCOMP_DEBUG("  - remote change time is now journal creation time %s",
				               remoteChangeTime->iso());
			}
			catch ( ... ) {}
		}
	}

	// Fetch last local journal entry to set the event type
	entry = getLastJournalEntry(*query(), local->publicID(), action);
	if ( !entry ) {
		// Not a local journal yet, apply the change
		SEISCOMP_DEBUG("  => no local journal found, apply the update");
		entry = createJournalEntry(local->publicID(), action, rR ? Core::toString(*rR) : string(), remoteChangeTime ? std::addressof(*remoteChangeTime) : nullptr);
		notifiers.push_back(
			new Notifier("Journaling", OP_ADD, entry.get())
		);
	}
	else {
		// There is a local journal entry
		OPT(Core::Time) localChangeTime;

		try {
			localChangeTime = entry->created();
			SEISCOMP_DEBUG("  - local change time is %s", localChangeTime->iso());
		}
		catch ( ... ) {}

		if ( localChangeTime && remoteChangeTime ) {
			if ( *remoteChangeTime > *localChangeTime ) {
				// The remote change time is more recent, apply it
				SEISCOMP_DEBUG("  => the remote change time is more recent, apply the update");
				entry = createJournalEntry(local->publicID(), action, rR ? Core::toString(*rR) : string(), remoteChangeTime ? std::addressof(*remoteChangeTime) : nullptr);
				notifiers.push_back(
					new Notifier("Journaling", OP_ADD, entry.get())
				);
			}
			else {
				SEISCOMP_INFO("* skipping %s update because the "
				              "local change %s the remote one",
				              name,
				              *remoteChangeTime < *localChangeTime ?
				                  "is more recent than" :
				                  "happened at the same time as");
			}
		}
		else {
			if ( entry->sender() == author() ) {
				SEISCOMP_DEBUG("  => self is the last author of the journal, apply the update");
				entry = createJournalEntry(local->publicID(), action, rR ? Core::toString(*rR) : "", remoteChangeTime ? std::addressof(*remoteChangeTime) : nullptr);
				notifiers.push_back(
					new Notifier("Journaling", OP_ADD, entry.get())
				);
			}
			else {
				SEISCOMP_INFO("* skipping %s update because it "
				              "has been set already by %s",
				              name, entry->sender());
			}
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::syncEvent(const EventParameters *ep, const Journaling *journals,
                    const Event *event, const RoutingTable *routing,
                    Notifiers &notifiers,
                    bool syncPreferred) {
	if ( !query() ) {
		SEISCOMP_ERROR("No database query available for event attribute synchronization");
		return;
	}

	auto origin = ep->findOrigin(event->preferredOriginID());
	if ( !origin ) {
		SEISCOMP_ERROR("Remote preferred origin '%s' not found: skipping event synchronization",
		               event->preferredOriginID().c_str());
		return;
	}

	try {
		if ( isAgencyIDBlocked(origin->creationInfo().agencyID()) ) {
			SEISCOMP_DEBUG("Remote preferred origin '%s' agencyID '%s' is blocked: skipping event synchronization",
			               origin->publicID().c_str(),
			               origin->creationInfo().agencyID().c_str());
			return;
		}
	}
	catch ( ... ) {}

	EventPtr targetEvent = query()->getEvent(event->preferredOriginID());
	if ( !targetEvent ) {
		SEISCOMP_DEBUG("No event found for origin %s, need to wait",
		               event->preferredOriginID().c_str());
		string eventID = waitForEventAssociation(event->preferredOriginID(),
		                                         _config.maxWaitForEventIDTimeout);
		if ( eventID.empty() ) {
			SEISCOMP_ERROR("Event association timeout reached, skipping event synchronisation for input event %s",
			               event->publicID().c_str());
			return;
		}

		SEISCOMP_DEBUG("Origin %s has been associated with event %s",
		               event->preferredOriginID().c_str(), eventID.c_str());

		targetEvent = static_cast<Event*>(query()->getObject(Event::TypeInfo(), eventID));
		if ( !targetEvent ) {
			SEISCOMP_ERROR("Failed to read target event %s from database, skipping event synchronisation for input event %s",
			               eventID.c_str(), event->publicID().c_str());
			return;
		}
	}

	query()->loadComments(targetEvent.get());
	query()->loadEventDescriptions(targetEvent.get());

	SEISCOMP_INFO("Sync with event %s", targetEvent->publicID().c_str());

	// Associate all focal mechanisms
	for ( size_t i = 0; i < event->focalMechanismReferenceCount(); ++i ) {
		auto fmRef = event->focalMechanismReference(i);
		auto fm = ep->findFocalMechanism(fmRef->focalMechanismID());
		if ( !fm ) {
			SEISCOMP_WARNING("* referenced focal mechanism not found in event: '%s'",
			                 fmRef->focalMechanismID().c_str());
			continue;
		}

		EventPtr associatedEvent = query()->getEventForFocalMechanism(fm->publicID());
		if ( !associatedEvent || associatedEvent->publicID() != targetEvent->publicID() ) {
			// Force association
			notifiers.push_back(
				new Notifier(
					targetEvent->publicID(),
					OP_ADD,
					new FocalMechanismReference(fm->publicID())
				)
			);

			SEISCOMP_DEBUG("* force association of focal mechanism: '%s'",
			               fm->publicID().c_str());
		}
	}

	// Preferred origin
	if ( syncPreferred ) {
		if ( !routing || checkRouting(origin, *routing) ) {
			checkUpdate(notifiers, &Event::preferredOriginID, event, targetEvent.get(),
			            "preferred origin", journals, "EvPrefOrgID");
		}
		else {
			SEISCOMP_DEBUG("* origins are not being routed, skip checking it");
		}

		if ( !event->preferredFocalMechanismID().empty() ) {
			FocalMechanism *fm = ep->findFocalMechanism(event->preferredFocalMechanismID());
			if ( fm ) {
				if ( !routing || checkRouting(fm, *routing) ) {
					checkUpdate(notifiers, &Event::preferredFocalMechanismID, event, targetEvent.get(),
					            "preferred focal mechanism", journals, "EvPrefFocMecID");
				}
				else {
					SEISCOMP_DEBUG("* focalMechanisms are not being routed, skip checking it");
				}
			}
			else {
				SEISCOMP_WARNING("* preferred focal mechanism not found in input document, skip checking it");
			}
		}

		Magnitude *prefMag = nullptr;
		for ( size_t i = 0; i < ep->originCount(); ++i ) {
			Origin *org = ep->origin(i);
			for ( size_t j = 0; j < org->magnitudeCount(); ++j ) {
				Magnitude *mag = org->magnitude(j);
				if ( mag->publicID() == event->preferredMagnitudeID() ) {
					prefMag = mag;
					i = ep->originCount();
					break;
				}
			}
		}

		if ( prefMag ) {
			bool isMw = false;

			// Check if the preferred magnitude is part of a moment tensor
			if ( prefMag->type() == "Mw" ) {
				for ( size_t i = 0; i < ep->focalMechanismCount(); ++i ) {
					auto fm = ep->focalMechanism(i);
					for ( size_t j = 0; j < fm->momentTensorCount(); ++j ) {
						auto mt = fm->momentTensor(j);
						if ( mt->derivedOriginID() == prefMag->origin()->publicID() ) {
							isMw = true;
							i = ep->focalMechanismCount();
							break;
						}
					}
				}
			}

			auto targetPrefMag = _cache.get<Magnitude>(targetEvent->preferredMagnitudeID());
			if ( !targetPrefMag || (prefMag->type() != targetPrefMag->type()) ) {
				SEISCOMP_DEBUG("* check update of preferred magnitude: '%s'",
				               prefMag->type());

				OPT(Core::Time) remoteChangeTime;

				try {
					remoteChangeTime = event->creationInfo().modificationTime();
					SEISCOMP_DEBUG("  - remote change time is event modification time %s",
					               remoteChangeTime->iso());
				}
				catch ( ... ) {
					try {
						remoteChangeTime = event->creationInfo().creationTime();
						SEISCOMP_DEBUG("  - remote change time is event creation time %s",
						               remoteChangeTime->iso());
					}
					catch ( ... ) {}
				}

				// Get last journal entry
				auto entry = getLastJournalEntry(journals, event->publicID(), "EvPrefMagType");
				if ( !entry && isMw ) {
					SEISCOMP_DEBUG("  - no remote EvPrefMagType entry, trying EvPrefMw");
					entry = getLastJournalEntry(journals, event->publicID(), "EvPrefMagType");
				}

				if ( entry ) {
					if ( !isMw && (entry->parameters() != prefMag->type()) ) {
						SEISCOMP_WARNING("%s: preferred magnitude is not matching the last journal type, ignoring it",
						                 event->publicID());
					}
					else {
						try {
							remoteChangeTime = entry->created();
							SEISCOMP_DEBUG("  - remote change time is now journal creation time %s",
							               remoteChangeTime->iso());
						}
						catch ( ... ) {}
					}
				}

				entry = getLastJournalEntry(*query(), targetEvent->publicID(), "EvPrefMagType");
				if ( !entry ) {
					SEISCOMP_DEBUG("  - no local EvPrefMagType entry, trying EvPrefMw");
					entry = getLastJournalEntry(*query(), targetEvent->publicID(), "EvPrefMw");
				}

				if ( !entry ) {
					// Not a local journal yet, apply the change
					SEISCOMP_DEBUG("  => no local journal found, apply the update");
					if ( isMw ) {
						SEISCOMP_DEBUG("* preferred magnitude is Mw of a moment tensor, send EvPrefMw");
						entry = createJournalEntry(targetEvent->publicID(), "EvPrefMw", "true",
						                           remoteChangeTime ? std::addressof(*remoteChangeTime) : nullptr);
					}
					else {
						entry = createJournalEntry(targetEvent->publicID(), "EvPrefMagType", prefMag->type(),
						                           remoteChangeTime ? std::addressof(*remoteChangeTime) : nullptr);
					}
					notifiers.push_back(
						new Notifier("Journaling", OP_ADD, entry.get())
					);
				}
				else {
					// There is a local journal entry
					OPT(Core::Time) localChangeTime;

					try {
						localChangeTime = entry->created();
						SEISCOMP_DEBUG("  - local change time is %s", localChangeTime->iso());
					}
					catch ( ... ) {}

					if ( localChangeTime && remoteChangeTime ) {
						if ( *remoteChangeTime > *localChangeTime ) {
							// The remote change time is more recent, apply it
							SEISCOMP_DEBUG("  => the remote change time is more recent, apply the update");
							if ( isMw ) {
								SEISCOMP_DEBUG("* preferred magnitude is Mw of a moment tensor, send EvPrefMw");
								entry = createJournalEntry(targetEvent->publicID(), "EvPrefMw", "true",
								                           remoteChangeTime ? std::addressof(*remoteChangeTime) : nullptr);
							}
							else {
								entry = createJournalEntry(targetEvent->publicID(), "EvPrefMagType", prefMag->type(),
								                           remoteChangeTime ? std::addressof(*remoteChangeTime) : nullptr);
							}
							notifiers.push_back(
								new Notifier("Journaling", OP_ADD, entry.get())
							);
						}
						else {
							SEISCOMP_INFO("* skipping preferred magnitude type update because the "
							              "local change %s the remote one",
							              *remoteChangeTime < *localChangeTime ?
							                  "is more recent than" :
							                  "happened at the same time as");
						}
					}
					else {
						if ( entry->sender() == author() ) {
							SEISCOMP_DEBUG("  => self is the last author of the journal, apply the update");
							if ( isMw ) {
								SEISCOMP_DEBUG("* preferred magnitude is Mw of a moment tensor, send EvPrefMw");
								entry = createJournalEntry(targetEvent->publicID(), "EvPrefMw", "true",
								                           remoteChangeTime ? std::addressof(*remoteChangeTime) : nullptr);
							}
							else {
								entry = createJournalEntry(targetEvent->publicID(), "EvPrefMagType", prefMag->type(),
								                           remoteChangeTime ? std::addressof(*remoteChangeTime) : nullptr);
							}
							notifiers.push_back(
								new Notifier("Journaling", OP_ADD, entry.get())
							);
						}
						else {
							SEISCOMP_INFO("* skipping preferred magnitude type update because it "
							              "has been set already by %s",
							              entry->sender());
						}
					}
				}
			}
			else {
				SEISCOMP_DEBUG("* check update of preferred magnitude: '%s' => equal",
				               prefMag->type());
			}
		}
		else if ( !event->preferredMagnitudeID().empty() ) {

			SEISCOMP_WARNING("* preferred magnitude %s not found in input document, skip checking the type",
			                 event->preferredMagnitudeID());
		}
	}

	// Event type
	checkUpdate(notifiers, &Event::type, event, targetEvent.get(),
	            "event type", journals, "EvType");

	// Event type certainty
	checkUpdate(notifiers, &Event::typeCertainty, event, targetEvent.get(),
	            "event type certainty", journals, "EvTypeCertainty");

	// Event name
	{
		auto desc = event->eventDescription(EventDescriptionIndex(EARTHQUAKE_NAME));
		auto targetDesc = targetEvent->eventDescription(EventDescriptionIndex(EARTHQUAKE_NAME));

		if ( desc ) {
			if ( !targetDesc || desc->text() != targetDesc->text() ) {
				SEISCOMP_DEBUG("* check update of event name: '%s'", desc->text().c_str());

				JournalEntryPtr entry = getLastJournalEntry(*query(), targetEvent->publicID(), "EvName");
				if ( !entry || entry->sender() == author() ) {
					entry = createJournalEntry(targetEvent->publicID(), "EvName", desc->text());
					notifiers.push_back(
						new Notifier("Journaling", OP_ADD, entry.get())
					);
				}
				else {
					SEISCOMP_INFO("* skipping event name update because it "
					              "has been set already by %s",
					              entry->sender());
				}
			}
		}
		else {
			if ( targetDesc && !targetDesc->text().empty() ) {
				SEISCOMP_DEBUG("* check remove of event name");
				JournalEntryPtr entry = getLastJournalEntry(*query(), targetEvent->publicID(), "EvName");
				if ( !entry || entry->sender() == author() ) {
					entry = createJournalEntry(targetEvent->publicID(), "EvName", "");
					notifiers.push_back(
						new Notifier("Journaling", OP_ADD, entry.get())
					);
				}
				else {
					SEISCOMP_INFO("* skipping event name removal because it "
					              "has been set already by %s",
					              entry->sender());
				}
			}
		}
	}

	// Operator comment
	{
		Comment *cmt = event->comment(string("Operator"));
		Comment *targetCmt = targetEvent->comment(string("Operator"));
		if ( cmt ) {
			if ( !targetCmt || cmt->text() != targetCmt->text() ) {
				SEISCOMP_DEBUG("* check update of operator comment: %s", cmt->text().c_str());

				JournalEntryPtr entry = getLastJournalEntry(*query(), targetEvent->publicID(), "EvOpComment");
				if ( !entry || entry->sender() == author() ) {
					entry = createJournalEntry(targetEvent->publicID(), "EvOpComment", cmt->text());
					notifiers.push_back(
						new Notifier("Journaling", OP_ADD, entry.get())
					);
				}
				else {
					SEISCOMP_INFO("* skipping event opertor comment update because it "
					              "has been set already by %s",
					              entry->sender());
				}
			}
		}
		else {
			if ( targetCmt && !targetCmt->text().empty() ) {
				JournalEntryPtr entry = getLastJournalEntry(*query(), targetEvent->publicID(), "EvOpComment");
				if ( !entry || entry->sender() == author() ) {
					entry = createJournalEntry(targetEvent->publicID(), "EvOpComment", "");
					notifiers.push_back(
						new Notifier("Journaling", OP_ADD, entry.get())
					);
				}
				else {
					SEISCOMP_INFO("* skipping event operator comment removal because it "
					              "has been set already by %s",
					              entry->sender());
				}
			}
		}
	}

	// Comments in general
	for ( size_t i = 0; i < targetEvent->commentCount(); ++i ) {
		Comment *localCmt = targetEvent->comment(i);
		if ( localCmt->id() == "Operator" ) {
			continue;
		}

		SEISCOMP_DEBUG("> %s", localCmt->id().c_str());

		Comment *remoteCmt = event->comment(localCmt->id());
		if ( !remoteCmt ) {
			SEISCOMP_DEBUG("* remove comment '%s'", localCmt->id().c_str());
			// Remove comment
			notifiers.push_back(
				new Notifier(targetEvent->publicID(), OP_REMOVE, localCmt)
			);
		}
		else {
			if ( remoteCmt->text() != localCmt->text() ) {
				if ( checkUpdateOnTimeStamps(remoteCmt, localCmt) ) {
					SEISCOMP_DEBUG("* update comment '%s'", localCmt->id().c_str());
					*localCmt = *remoteCmt;
					// Update comment
					notifiers.push_back(
						new Notifier(targetEvent->publicID(), OP_UPDATE, localCmt)
					);
				}
				else {
					SEISCOMP_INFO("* skipping update for comment '%s' because the "
					              "local counterpart was updated later",
					              localCmt->id());
				}
			}
		}
	}

	for ( size_t i = 0; i < event->commentCount(); ++i ) {
		Comment *remoteCmt = event->comment(i);
		if ( remoteCmt->id() == "Operator" ) {
			continue;
		}

		SEISCOMP_DEBUG("< %s", remoteCmt->id().c_str());

		Comment *localCmt = targetEvent->comment(remoteCmt->id());
		if ( !localCmt ) {
			SEISCOMP_DEBUG("* add comment '%s'", remoteCmt->id().c_str());
			// Add comment
			notifiers.push_back(
				new Notifier(targetEvent->publicID(), OP_ADD, remoteCmt)
			);
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::sendNotifiers(const EventParameters *ep, const Notifiers &notifiers,
                        const RoutingTable &routing) {
	if ( notifiers.empty() ) {
		SEISCOMP_INFO("no modification required");
		return true;
	}

	// statistics
	int add = 0, update = 0, remove = 0, msgTotal = 0;
	map<string, int> groupMessages;

	string group, prevGroup;
	NotifierMessagePtr nm = new NotifierMessage();
	for ( auto &n : notifiers ) {
		// check if a route exists
		if ( !resolveRouting(group, n->object(), routing) ) {
			SEISCOMP_DEBUG("Skip %s of %s: no routing",
			               n->operation().toString(), n->object()->className());
			continue;
		}

		// the message has to be send if the batchSize is exceeded or the
		// message group changed
		if ( (nm->size() > 0 && group != prevGroup) ||
		     (_config.batchSize > 0 && nm->size() >= _config.batchSize) ) {
			SEISCOMP_DEBUG("sending notifier message (#%i) to group '%s'",
			               nm->size(), prevGroup.c_str());
			if ( !connection()->send(prevGroup, nm.get()) ) {
				SEISCOMP_ERROR("sending message failed with error: %s",
				               connection()->lastError().toString());
				return false;
			}

			nm = new NotifierMessage();
			++groupMessages[prevGroup];
		}

		// apply notifier locally
		if ( n->object() ) {
			applyNotifier(n.get());
		}

		prevGroup = group;
		nm->attach(n.get());

		switch ( n->operation() ) {
			case OP_ADD:
				++add;
				if ( ep && Origin::Cast(n->object()) ) {
					// If the object is an origin and if event attributes
					// should be synchronized then we add a JournalEntry which
					// adds a hint of the remote eventID for this particular
					// origin so that scevent can grab this hint and assign that
					// eventID if an event has to be created.
					auto org = static_cast<Origin*>(n->object());
					for ( size_t i = 0; i < ep->eventCount(); ++i ) {
						auto event = ep->event(i);
						if ( event->preferredOriginID() == org->publicID()
						  || event->originReference(org->publicID()) ) {
							// The origin is either the preferred origin or it
							// is associated -> use this eventID as hint
							JournalEntryPtr hint = new JournalEntry;
							hint->setCreated(Core::Time::UTC());
							hint->setObjectID(org->publicID());
							hint->setSender(author());
							hint->setAction("OrgEventIDHint");
							hint->setParameters(event->publicID());
							nm->attach(
								new Notifier(
									Journaling::ClassName(),
									OP_ADD,
									hint.get()
								)
							);
							break;
						}
					}
				}
				break;
			case OP_UPDATE:
				++update;
				break;
			case OP_REMOVE:
				++remove;
				break;
			default:
				break;
		}
	}

	// send last message
	if ( nm->size() > 0 ) {
		if ( !connection()->send(group, nm.get()) ) {
			SEISCOMP_ERROR("sending message failed with error: %s",
			               connection()->lastError().toString());
			return false;
		}

		++groupMessages[group];
	}

	if ( !groupMessages.empty() ) {
		stringstream ss;
		for ( auto &[name, count] : groupMessages ) {
			++msgTotal;
			ss << "  " << name << ": " << count << endl;
		}
		SEISCOMP_INFO("send %i notifiers (ADD: %i, UPDATE: %i, REMOVE: %i) "
		              "to the following message groups:\n%s",
		              add + update + remove, add, update, remove,
		              ss.str().c_str());
	}

	// Sync with messaging
	if ( connection() ) {
		connection()->syncOutbox();
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::sendJournals(const Notifiers &journals) {
	if ( journals.empty() ) {
		SEISCOMP_INFO("no journal entries queued");
		return true;
	}

	NotifierMessagePtr nm = new NotifierMessage();
	for ( auto &n : journals ) {
		nm->attach(n.get());
	}

	if ( !connection()->send(nm.get()) ) {
		SEISCOMP_ERROR("sending message failed with error: %s",
		               connection()->lastError().toString());
		return false;
	}

	SEISCOMP_INFO("send %i journal entries "
	              "to the message group: %s", int(nm->size()),
	              primaryMessagingGroup().c_str());

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::applyNotifier(const Notifier *n) {
	bool enabled = Notifier::IsEnabled();
	Notifier::SetEnabled(false);

	/*
	cerr << "Apply " << n->parentID()
	     << "  " << n->object()->className()
	     << "  " << n->operation().toString()
	     << endl;
	*/

	// parent not cached but current object is a cached public object,
	// this should not happen because the parent must have been loaded to
	// detect the diff in the first place
	PublicObject *parent = PublicObject::Find(n->parentID());

	if ( !parent ) {
		PublicObject *notifierPO = PublicObject::Cast(n->object());
		if ( notifierPO && n->operation() == OP_UPDATE ) {
			PublicObject *po = PublicObject::Find(notifierPO->publicID());
			if ( po ) {
				po->assign(notifierPO);
			}
		}
	}
	else {
		Object *obj = n->object();

		switch ( n->operation() ) {
			case OP_ADD:
			{
				// Create a local copy which gets added to the local
				// tree. As the notifier have been created from the input
				// document, it will be kept
				ObjectPtr clone = obj->clone();
				obj = clone.get();
				PublicObject *po = PublicObject::Cast(obj);
				if ( po && !po->registered() ) {
					po->registerMe();
				}
				obj->attachTo(parent);
				addObject(n->parentID(), obj);
				break;
			}
			case OP_REMOVE:
				obj->detachFrom(parent);
				removeObject(n->parentID(), obj);
				break;
			case OP_UPDATE:
				parent->updateChild(obj);
				break;
			default:
				break;
		}
	}

	Notifier::SetEnabled(enabled);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::readLastUpdates() {
	SEISCOMP_INFO("reading last update timestamps from file '%s'",
	              _lastUpdateFile.c_str());
	int i = 0;
	ifstream ifs(_lastUpdateFile.c_str());
	string line;
	Core::Time time;
	vector<string> toks;
	map<string, Core::Time> hostTimes;
	while ( ifs.good() && getline(ifs, line) ) {
		++i;
		if ( Core::split(toks, line.c_str(), " ") == 2 &&
		     time.fromString(toks[1].c_str(), "%FT%T.%fZ") ) {
			hostTimes[toks[0]] = time;
		}
		else {
			SEISCOMP_ERROR("line %i of last update file '%s' invalid",
			               i, _lastUpdateFile.c_str());
			break;
		}
	}
	ifs.close();

	for ( auto &client : _clients ) {
		auto entry = hostTimes.find(client->config()->host);
		if ( entry != hostTimes.end() ) {
			SEISCOMP_DEBUG("setting last update time of host '%s' to %s",
			               entry->first.c_str(), entry->second.iso().c_str());
			client->setLastUpdate(entry->second);
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::writeLastUpdates() {
	SEISCOMP_DEBUG("writing last update times");

	// write times to temporary file
	string tmpFile = _lastUpdateFile + ".tmp";
	ofstream ofs(tmpFile.c_str(), ios::trunc);
	if ( !ofs.good() ) {
		SEISCOMP_ERROR("could not open file '%s' for writing", tmpFile.c_str());
		return;
	}

	for ( auto &client : _clients ) {
		if ( !client->lastUpdate() ) {
			continue;
		}
		ofs << client->config()->host << " " << client->lastUpdate()->iso() << endl;
	}

	if ( !ofs.good() ) {
		SEISCOMP_ERROR("could not write to file '%s'", tmpFile.c_str());
		return;
	}
	ofs.close();

	// move temporary file
	if ( ::rename(tmpFile.c_str(), _lastUpdateFile.c_str()) ) {
		SEISCOMP_ERROR("Could not rename temporary file '%s' to '%s'",
		               tmpFile.c_str(), _lastUpdateFile.c_str());
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
} // ns QL2SC
} // ns Seiscomp
