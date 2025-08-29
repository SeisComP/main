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


#include "eventtool.h"
#include "util.h"

#include <seiscomp/logging/filerotator.h>
#include <seiscomp/logging/channel.h>

#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/magnitude.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/focalmechanism.h>
#include <seiscomp/datamodel/momenttensor.h>
#include <seiscomp/datamodel/eventdescription.h>
#include <seiscomp/datamodel/inventory.h>
#include <seiscomp/datamodel/journalentry.h>
#include <seiscomp/datamodel/notifier.h>
#include <seiscomp/datamodel/utils.h>

#include <seiscomp/io/archive/xmlarchive.h>
#include <seiscomp/seismology/regions.h>

#include <seiscomp/core/genericmessage.h>
#include <seiscomp/math/geo.h>
#include <seiscomp/system/hostinfo.h>
#include <seiscomp/wired/protocols/http.h>

#include <functional>
#include <stdexcept>


using namespace std;
using namespace Seiscomp;
using namespace Seiscomp::Core;
using namespace Seiscomp::DataModel;
using namespace Seiscomp::Client;
using namespace Seiscomp::Private;

#define DELAY_CHECK_INTERVAL 1
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
namespace {


void makeUpper(std::string &src) {
	for ( size_t i = 0; i < src.size(); ++i )
		src[i] = toupper(src[i]);
}

const char *PRIORITY_TOKENS[] = {
	"AGENCY", "AUTHOR", "MODE", "STATUS", "METHOD",
	"PHASES", "PHASES_AUTOMATIC",
	"RMS", "RMS_AUTOMATIC",
	"TIME", "TIME_AUTOMATIC", "SCORE"
};


// Global region class defining a rectangular region
// by latmin, lonmin, latmax, lonmax.
DEFINE_SMARTPOINTER(GlobalRegion);
class GlobalRegion : public Client::Config::Region {
	public:
		GlobalRegion() {}

		bool init(const Seiscomp::Config::Config &config, const std::string &prefix) {
			vector<double> region;
			try { region = config.getDoubles(prefix + "rect"); }
			catch ( ... ) {
				return false;
			}

			// Parse region
			if ( region.size() != 4 ) {
				SEISCOMP_ERROR("%srect: expected 4 values in region definition, got %d",
							   prefix, (int)region.size());
				return false;
			}

			latMin = region[0];
			lonMin = region[1];
			latMax = region[2];
			lonMax = region[3];

			return true;
		}

		bool isInside(double lat, double lon) const {
			double len, dist;

			if ( lat < latMin || lat > latMax ) return false;

			len = lonMax - lonMin;
			if ( len < 0 )
				len += 360.0;

			dist = lon - lonMin;
			if ( dist < 0 )
				dist += 360.0;

			return dist <= len;
		}

		double latMin, lonMin;
		double latMax, lonMax;
};


DEFINE_SMARTPOINTER(ClearCacheRequestMessage);

/**
 * \brief Message for requesting a clearing of the cache
 * This message type requests a response from a peer.
 */
class SC_SYSTEM_CLIENT_API ClearCacheRequestMessage : public Seiscomp::Core::Message {
	DECLARE_SC_CLASS(ClearCacheRequestMessage)
	DECLARE_SERIALIZATION;

	public:
		//! Constructor
		ClearCacheRequestMessage() {}

		//! Implemented interface from Message
		virtual bool empty() const  { return false; }
};

void ClearCacheRequestMessage::serialize(Archive& ar) {}

IMPLEMENT_SC_CLASS_DERIVED(
	ClearCacheRequestMessage, Message, "clear_cache_request_message"
);

DEFINE_SMARTPOINTER(ClearCacheResponseMessage);

/**
 * \brief Message to respond to a clear cache request
 */
class SC_SYSTEM_CLIENT_API ClearCacheResponseMessage : public Seiscomp::Core::Message {
	DECLARE_SC_CLASS(ClearCacheResponseMessage)
	DECLARE_SERIALIZATION;

	public:
		//! Constructor
		ClearCacheResponseMessage() {}

		//! Implemented interface from Message
		virtual bool empty() const  { return false; }
};

void ClearCacheResponseMessage::serialize(Archive& ar) {}

IMPLEMENT_SC_CLASS_DERIVED(
	ClearCacheResponseMessage, Message, "clear_cache_response_message"
);


bool isRejected(Magnitude *mag) {
	try {
		return mag->evaluationStatus() == REJECTED;
	}
	catch (Core::ValueException &) {}

	return false;
}


class RESTAPISession : public Wired::HttpSession {
	public:
		RESTAPISession(Wired::Device *sock, const char *protocol, EventTool *app)
		: Wired::HttpSession(sock, protocol), _app(app) {}

	protected:
		static inline const string ErrorContentType = "Invalid content type: only text/xml is supported.";
		static inline const string ErrorInvalidXMLDocument = "Invalid input document: XML format expected.";
		static inline const string ErrorInvalidInputDocument = "Invalid input document: EventParameters expected.";

		bool handleGETRequest(Wired::HttpRequest &req) override {
			if ( req.path == "/api/1/info" ) {
				static auto json =
				"{"
					"\"name\":\"" + _app->name() + "\","
					"\"framework\":\"" + _app->frameworkVersion() + "\"" +
				( _app->version() ? string(",\"version\":\"") + _app->version() + "\"" : string() ) +
				"}";
				sendResponse(json, Wired::HTTP_200, "application/json");
				return true;
			}

			sendStatus(Wired::HTTP_404);
			return true;
		}

		bool handlePOSTRequest(Wired::HttpRequest &req) override {
			if ( req.path == "/api/1/try-to-associate" ) {
				if ( req.contentType != "text/xml" ) {
					sendStatus(Wired::HTTP_400, ErrorContentType);
					return true;
				}

				IO::XMLArchive ar;
				InputStringViewBuf buf(req.data);
				if ( !ar.open(&buf) ) {
					sendStatus(Wired::HTTP_400, ErrorInvalidXMLDocument);
					return true;
				}
				PublicObject::SetRegistrationEnabled(false);
				EventParametersPtr ep;
				ar >> ep;
				if ( !ep ) {
					sendStatus(Wired::HTTP_400, ErrorInvalidInputDocument);
					return true;
				}

				SEISCOMP_DEBUG("Handle association REST request");
				try {
					auto eventID = _app->tryToAssociate(ep.get());
					if ( eventID.empty() ) {
						sendStatus(Wired::HTTP_204);
					}
					else {
						sendStatus(Wired::HTTP_200, eventID);
					}
				}
				catch ( exception &e ) {
					SEISCOMP_WARNING("Association REST request: %s", e.what());
					sendStatus(Wired::HTTP_400, e.what());
				}

				return true;
			}

			sendStatus(Wired::HTTP_404);
			return true;
		}

	private:
		EventTool *_app;
};


class RESTAPIEndpoint : public Wired::Endpoint {
	public:
		RESTAPIEndpoint(EventTool *app) : Wired::Endpoint(nullptr), _app(app) {}

	protected:
		Wired::Session *createSession(Wired::Socket *socket) override {
			return new RESTAPISession(socket, "http", _app);
		}

	private:
		EventTool *_app;
};


} // ns anonymous
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EventTool::EventTool(int argc, char **argv) : Application(argc, argv) {
	setAutoApplyNotifierEnabled(true);
	setInterpretNotifierEnabled(true);

	setLoadRegionsEnabled(true);

	setPrimaryMessagingGroup("EVENT");

	addMessagingSubscription("LOCATION");
	addMessagingSubscription("MAGNITUDE");
	addMessagingSubscription("FOCMECH");
	addMessagingSubscription("EVENT");

	_cache.setPopCallback(bind(&EventTool::removedFromCache, this, placeholders::_1));

	_infoChannel = SEISCOMP_DEF_LOGCHANNEL("processing/info", Logging::LL_INFO);

	bindSettings(&_config);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EventTool::~EventTool() {
	delete _infoChannel;
	if ( _infoOutput ) {
		delete _infoOutput;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
std::string EventTool::tryToAssociate(const DataModel::EventParameters *ep) {
	if ( ep->originCount() != 1 ) {
		throw runtime_error("One origin is required and only one origin is allowed");
	}

	EventInformation::PickCache cache;
	auto org = ep->origin(0);
	for ( size_t i = 0; i < ep->pickCount(); ++i ) {
		auto pick = ep->pick(i);
		cache[pick->publicID()] = pick;
	}

	scoped_lock l(_associationMutex);

	auto info = associateOrigin(org, false, nullptr, &cache);
	if ( info && info->event ) {
		return info->event->publicID();
	}

	return std::string();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool EventTool::validateParameters() {
	if ( !Application::validateParameters() ) {
		return false;
	}

	if ( _config.dbDisable ) {
		setDatabaseEnabled(false, false);
	}

	if ( !_config.originID.empty() || !_config.eventID.empty() ) {
		setMessagingEnabled(false);
	}

	// For offline processing messaging is disabled and database
	if ( !_config.epFile.empty() ) {
		setMessagingEnabled(false);
		setDatabaseEnabled(false, false);
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool EventTool::initConfiguration() {
	if ( !Application::initConfiguration() ) {
		return false;
	}

	Config::RegionFilter regionFilter;
	GlobalRegionPtr region = new GlobalRegion;
	if ( region->init(configuration(), "eventAssociation.region.") ) {
		SEISCOMP_INFO("Region check activated");
		regionFilter.region = region;
	}

	try { regionFilter.minDepth = configGetDouble("eventAssociation.region.minDepth"); } catch ( ... ) {}
	try { regionFilter.maxDepth = configGetDouble("eventAssociation.region.maxDepth"); } catch ( ... ) {}

	_config.regionFilter.push_back(regionFilter);

	for ( auto &item : _config.eventAssociation.priorities ) {
		bool validToken = false;
		makeUpper(item);
		for ( unsigned int t = 0; t < sizeof(PRIORITY_TOKENS) / sizeof(char*); ++t ) {
			if ( item == PRIORITY_TOKENS[t] ) {
				validToken = true;
				break;
			}
		}

		if ( !validToken ) {
			SEISCOMP_ERROR("Unexpected token in eventAssociation.priorities: %s", item);
			return false;
		}

		// SCORE requires a score method to be set up.
		if ( item == "SCORE" && _config.eventAssociation.score.empty() ) {
			SEISCOMP_ERROR("eventAssociation.priorities defines SCORE but no score method is set up.");
			return false;
		}
	}

	try {
		Config::StringList blIDs = configGetStrings("processing.blacklist.eventIDs");
		_config.blacklistIDs.insert(blIDs.begin(), blIDs.end());
	}
	catch ( ... ) {}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool EventTool::init() {
	if ( !Application::init() ) {
		return false;
	}

	if ( _config.logProcessing ) {
		string logFile = Environment::Instance()->logFile(_name + "-processing-info");
		_infoOutput = new Logging::FileRotatorOutput(logFile.c_str(), 60*60*24, 30);
		_infoOutput->subscribe(_infoChannel);
	}

	if ( !_config.eventAssociation.score.empty() || _config.eventAssociation.minAutomaticScore ) {
		if ( _config.eventAssociation.score.empty() ) {
			SEISCOMP_ERROR("No score processor configured, eventAssociation.score is empty or not set");
			return false;
		}

		_score = ScoreProcessorFactory::Create(_config.eventAssociation.score);
		if ( !_score ) {
			SEISCOMP_ERROR("Score method '%s' is not available. Is the correct plugin loaded?",
			               _config.eventAssociation.score);
			return false;
		}

		if ( !_score->setup(configuration()) ) {
			SEISCOMP_ERROR("Score '%s' failed to initialize", _config.eventAssociation.score);
			return false;
		}
	}

	_inputOrigin = addInputObjectLog("origin");
	_inputMagnitude = addInputObjectLog("magnitude");
	_inputFocalMechanism = addInputObjectLog("focmech");
	_inputMomentTensor = addInputObjectLog("mt");
	_inputOriginRef = addInputObjectLog("originref");
	_inputFMRef = addInputObjectLog("focmechref");
	_inputEvent = addInputObjectLog("event");
	_inputJournal = addInputObjectLog("journal");
	_outputEvent = addOutputObjectLog("event", primaryMessagingGroup());
	_outputOriginRef = addOutputObjectLog("originref", primaryMessagingGroup());
	_outputFMRef = addOutputObjectLog("focmechref", primaryMessagingGroup());

	if ( _config.eventAssociation.delayTimeSpan > 0
	  || _config.eventAssociation.delayPrefFocMech > 0 ) {
		enableTimer(DELAY_CHECK_INTERVAL);
	}

	_cache.setTimeSpan(TimeSpan(_config.fExpiry * 3600.));
	_cache.setDatabaseArchive(query());

	_ep = new EventParameters;
	_journal = new Journaling;

	EventProcessorFactory::ServiceNames *services;
	services = EventProcessorFactory::Services();

	if ( services ) {
		for ( auto &service : *services ) {
			EventProcessorPtr proc = EventProcessorFactory::Create(service.c_str());
			if ( proc ) {
				if ( !proc->setup(configuration()) ) {
					SEISCOMP_WARNING("Event processor '%s' failed to initialize: skipping",
					                 service);
					continue;
				}

				SEISCOMP_INFO("Processor '%s' added", service);
				_processors.push_back(proc);
			}
		}

		delete services;
	}

	if ( _config.restAPI.valid() ) {
		_restAPI = new Wired::Server;
		if ( !_restAPI->addEndpoint(_config.restAPI.address, _config.restAPI.port,
		                            false, new RESTAPIEndpoint(this)) ) {
			SEISCOMP_ERROR("Failed to add REST API endpoint");
			return false;
		}

		if ( !_restAPI->init() ) {
			return false;
		}

		SEISCOMP_INFO("Bound REST API to port %d", _config.restAPI.port);
		_restAPIThread = thread([&] { _restAPI->run(); });
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::printUsage() const {
	cout << "Usage:"  << endl << "  " << name() << " [options]" << endl << endl
	     << "Associates Origins to Events or forms new Events if no match is "
	        "found. Selects "
	        << endl << "preferred origins, magnitudes and focal mechanisms "
	        "and sets event types."
	     << endl;

	Seiscomp::Client::Application::printUsage();

	cout << "Examples:" << endl;
	cout << "Real-time processing with informative debug output." << endl
	     << "  " << name() << " --debug" << endl;
	cout << endl << "None-real-time XML playback of origins."
	     << endl
	     << "  " << name() << " --ep origins.xml > events.xml"
	     << endl;
	cout << endl << "None-real-time XML playback for re-processing events "
	                "with origins."
	     << endl
	     << "  " << name() << " --ep origins.xml --reprocess > events.xml"
	     << endl << endl;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool EventTool::run() {
	if ( _config.clearCache ) {
		SEISCOMP_DEBUG("Sending clear cache request");
		ClearCacheRequestMessage cc_msg;
		connection()->send(&cc_msg);
		SEISCOMP_DEBUG("Waiting for clear cache response message...");
		while ( ! isExitRequested() ) { // sigkill or ctrl+c
			MessagePtr msg = connection()->recv();
			if ( ClearCacheResponseMessage::Cast(msg) ) {
				SEISCOMP_DEBUG("Clear cache response message received.");
				return true;
			}
		}
		return false;
	}

	if ( !_config.epFile.empty() ) {
		IO::XMLArchive ar;
		if ( !ar.open(_config.epFile.c_str()) ) {
			SEISCOMP_ERROR("Failed to open %s", _config.epFile);
			return false;
		}

		_ep = nullptr;
		ar >> _ep;
		if ( !_config.reprocess ) {
			ar >> _journal;
			if ( !_journal ) {
				_journal = new Journaling;
			}
		}

		ar.close();

		if ( !_ep ) {
			SEISCOMP_ERROR("No event parameters found in %s", _config.epFile);
			return false;
		}

		// Ignore all event objects when reprocessing
		if ( _config.reprocess ) {
			while ( _ep->eventCount() ) {
				_ep->removeEvent(0);
			}
		}

		for ( size_t i = 0; i < _ep->eventCount(); ++i ) {
			EventPtr evt = _ep->event(i);
			if ( _config.updateEventID ) {
				auto *origin = _ep->findOrigin(evt->preferredOriginID());
				if ( origin ) {
					string eventID = allocateEventID(query(), origin, _config);
					evt->setPublicID(eventID);
				}
				else {
					SEISCOMP_DEBUG("Cannot update event ID: preferred origin '%s'"
					               " not found", evt->preferredOriginID());
				}
			}
			EventInformationPtr info = new EventInformation(
				&_cache, &_config, query(), evt, author()
			);

			// Loading the references does not make sense here, because
			// the event has been just added
			cacheEvent(info);
		}

		for ( size_t i = 0; i < _ep->originCount(); ++i ) {
			EventInformationPtr info;

			OriginPtr org = _ep->origin(i);
			SEISCOMP_INFO("Processing origin %s", org->publicID());
			info = findAssociatedEvent(org.get());

			if ( info ) {
				SEISCOMP_DEBUG("... %s already associated with event %s",
				               org->publicID(),
				               info->event->publicID());
				continue;
			}

			info = associateOrigin(org.get(), true);
			if ( !info ) {
				SEISCOMP_DEBUG("... cannot associate with an event");
				continue;
			}

			updatePreferredOrigin(info.get());
		}


		for ( size_t i = 0; i < _ep->focalMechanismCount(); ++i ) {
			EventInformationPtr info;

			FocalMechanismPtr fm = _ep->focalMechanism(i);
			SEISCOMP_INFO("Processing focal mechanism %s", fm->publicID());
			info = findAssociatedEvent(fm.get());
			if ( info ) {
				SEISCOMP_DEBUG("... %s already associated with event %s",
				               fm->publicID(),
				               info->event->publicID());
				continue;
			}

			info = associateFocalMechanism(fm.get());
			if ( !info ) {
				SEISCOMP_DEBUG("... cannot associate with an event");
				continue;
			}

			updatePreferredFocalMechanism(info.get());
		}

		NotifierMessagePtr nmsg = Notifier::GetMessage(true);
		if ( nmsg ) {
			for ( const auto &n : *nmsg ) {
				if ( JournalEntry::Cast(n->object()) ) {
					n->apply();
				}
			}
		}

		ar.create("-");
		ar.setFormattedOutput(_config.formatted);
		ar << _ep;
		ar << _journal;
		ar.close();

		cerr << _ep->eventCount() << " events found" << endl;
		return true;
	}

	if ( !_config.originID.empty() ) {
		if ( !query() ) {
			cerr << "No database connection available" << endl;
			return false;
		}

		OriginPtr origin = Origin::Cast(query()->getObject(Origin::TypeInfo(), _config.originID));
		if ( !origin ) {
			cout << "Origin " << _config.originID << " has not been found, exiting" << endl;
			return true;
		}

		query()->loadArrivals(origin.get());

		EventInformationPtr info = associateOrigin(origin.get(), true);
		if ( !info ) {
			cout << "Origin " << _config.originID << " has not been associated to any event (already associated?)" << endl;
			return true;
		}

		cout << "Origin " << _config.originID << " has been associated to event " << info->event->publicID() << endl;
		updatePreferredOrigin(info.get());

		return true;
	}

	if ( !_config.eventID.empty() ) {
		EventInformationPtr info = new EventInformation(
			&_cache, &_config, query(), _config.eventID, author()
		);
		if ( !info->event ) {
			cout << "Event " << _config.eventID << " not found" << endl;
			return false;
		}

		info->loadAssocations(query());
		updatePreferredOrigin(info.get());
		return true;
	}

	return Application::run();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::done() {
	if ( _restAPI ) {
		_restAPI->shutdown();
		_restAPIThread.join();
		_restAPI = nullptr;
	}

	Client::Application::done();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::handleNetworkMessage(const Client::Packet *pkt) {
	Application::handleNetworkMessage(pkt);
	_lastNetworkMessage = pkt;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::handleMessage(Core::Message *msg) {
	_adds.clear();
	_updates.clear();
	_realUpdates.clear();
	_originBlackList.clear();

	// Lock the entire association process and all the supporting data
	// structures. Only tryToAssociate will also lock this mutex from
	// within the REST API thread.
	scoped_lock l(_associationMutex);

	Application::handleMessage(msg);

	ClearCacheRequestMessage *cc_msg = ClearCacheRequestMessage::Cast(msg);
	if ( cc_msg ) {
		SEISCOMP_DEBUG("Received clear cache request");
		_cache.clear();
		SEISCOMP_DEBUG("Sending clear cache response");
		ClearCacheResponseMessage cc_resp;
		connection()->send(&cc_resp);
	}

	SEISCOMP_DEBUG("Work on TODO list");
	for ( auto it = _adds.begin(); it != _adds.end(); ++it ) {
		SEISCOMP_DEBUG("Check ID %s", (*it)->publicID());
		OriginPtr org = Origin::Cast(it->get());
		if ( org ) {
			if ( _originBlackList.find(org->publicID()) == _originBlackList.end() ) {
				SEISCOMP_DEBUG("* work on new origin %s (%ld, %d/%lu)",
				               org->publicID(), (long int)org.get(),
				               definingPhaseCount(org.get()), (unsigned long)org->arrivalCount());
				associateOriginCheckDelay(org.get());
			}
			else
				SEISCOMP_DEBUG("* skipped new origin %s: blacklisted", org->publicID());

			continue;
		}

		FocalMechanismPtr fm = FocalMechanism::Cast(it->get());
		if ( fm ) {
			SEISCOMP_DEBUG("* work on new focal mechanism %s",
			               fm->publicID());
			associateFocalMechanism(fm.get());

			continue;
		}

		SEISCOMP_DEBUG("* unhandled object of class %s", (*it)->className());
	}

	for ( auto it = _updates.begin(); it != _updates.end(); ++it ) {
		// Has this object already been added in the previous step or delayed?
		bool delayed = false;
		for ( auto dit = _delayBuffer.rbegin(); dit != _delayBuffer.rend(); ++dit ) {
			if ( dit->obj == it->get() ) {
				delayed = true;
				break;
			}
		}

		if ( !delayed && (_adds.find(*it) == _adds.end()) ) {
			OriginPtr org = Origin::Cast(it->get());
			if ( org ) {
				if ( _originBlackList.find(org->publicID()) == _originBlackList.end() )
					updatedOrigin(org.get(), Magnitude::Cast(it->triggered.get()),
					              _realUpdates.find(*it) != _realUpdates.end());
				else
					SEISCOMP_DEBUG("* skipped origin %s: blacklisted", org->publicID());

				continue;
			}

			FocalMechanismPtr fm = FocalMechanism::Cast(it->get());
			if ( fm ) {
				updatedFocalMechanism(fm.get(), _realUpdates.find(*it) != _realUpdates.end());
				continue;
			}
		}
	}

	// Clean up event cache
	cleanUpEventCache();

	NotifierMessagePtr nmsg = Notifier::GetMessage(true);
	if ( nmsg ) {
		SEISCOMP_DEBUG("%d notifier available", static_cast<int>(nmsg->size()));
		if ( !_config.testMode ) {
			connection()->send(nmsg.get());
		}
	}
	else {
		SEISCOMP_DEBUG("No notifier available");
	}

	_lastNetworkMessage = nullptr;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::handleTimeout() {
	// First pass: decrease delay time and try to associate
	for ( auto it = _delayBuffer.begin(); it != _delayBuffer.end(); ) {
		it->timeout -= DELAY_CHECK_INTERVAL;
		if ( it->timeout <= -DELAY_CHECK_INTERVAL ) {
			OriginPtr org = Origin::Cast(it->obj);
			if ( org ) {
				SEISCOMP_LOG(_infoChannel, "Processing delayed origin %s",
				             org->publicID());
				bool createdEvent;
				associateOrigin(org.get(), true, &createdEvent);
				if ( createdEvent ) {
					// In case an event was created based on a delayed origin
					// then we immediately release this information. All other
					// origins will be associated in a row without intermediate
					// messages.
					NotifierMessagePtr nmsg = Notifier::GetMessage(true);
					if ( nmsg ) {
						SEISCOMP_DEBUG("%d notifier available", static_cast<int>(nmsg->size()));
						if ( !_config.testMode ) {
							connection()->send(nmsg.get());
						}
					}
				}
			}
			else {
				FocalMechanismPtr fm = FocalMechanism::Cast(it->obj);
				if ( fm ) {
					SEISCOMP_LOG(_infoChannel, "Processing delayed focal mechanism %s",
					             fm->publicID());
					associateFocalMechanism(fm.get());
				}
			}

			it = _delayBuffer.erase(it);
		}
		else
			++it;
	}

	// Second pass: flush pending origins (if possible)
	for ( auto it = _delayBuffer.begin(); it != _delayBuffer.end(); ) {
		OriginPtr org = Origin::Cast(it->obj);
		EventInformationPtr info;
		if ( org ) {
			SEISCOMP_LOG(_infoChannel, "Processing delayed origin %s (no event "
			             "creation before %i s)", org->publicID(),
			             it->timeout + DELAY_CHECK_INTERVAL);
			info = associateOrigin(org.get(), false);
		}
		else {
			FocalMechanismPtr fm = FocalMechanism::Cast(it->obj);
			if ( fm ) {
				SEISCOMP_LOG(_infoChannel, "Processing delayed focal mechanism %s",
				             fm->publicID());
				info = associateFocalMechanism(fm.get());
			}
		}

		// Has been associated
		if ( info )
			it = _delayBuffer.erase(it);
		else
			++it;
	}

	// Third pass: check pending event updates
	for ( auto it = _delayEventBuffer.begin(); it != _delayEventBuffer.end(); ) {
		it->timeout -= DELAY_CHECK_INTERVAL;
		if ( it->timeout <= -DELAY_CHECK_INTERVAL ) {
			if ( it->reason == SetPreferredFM ) {
				SEISCOMP_LOG(_infoChannel, "Handling pending event update for %s", it->id);
				EventInformationPtr info = cachedEvent(it->id);
				if ( !info ) {
					info = new EventInformation(&_cache, &_config, query(), it->id, author());
					if ( !info->event ) {
						SEISCOMP_ERROR("event %s not found", it->id);
						SEISCOMP_LOG(_infoChannel, "Skipped delayed preferred FM update, "
						             "event %s not found in database", it->id);
					}
					else {
						info->loadAssocations(query());
						cacheEvent(info);
						Notifier::Enable();
						updatePreferredFocalMechanism(info.get());
						Notifier::Disable();
					}
				}
				else {
					Notifier::Enable();
					updatePreferredFocalMechanism(info.get());
					Notifier::Disable();
				}
			}

			it = _delayEventBuffer.erase(it);
		}
		else
			++it;
	}

	// Clean up event cache
	cleanUpEventCache();

	NotifierMessagePtr nmsg = Notifier::GetMessage(true);
	if ( nmsg ) {
		SEISCOMP_DEBUG("%d notifier available", static_cast<int>(nmsg->size()));
		if ( !_config.testMode ) {
			connection()->send(nmsg.get());
		}
	}
	else
		SEISCOMP_DEBUG("No notifier available");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::cleanUpEventCache() {
	for ( auto it = _events.begin(); it != _events.end(); ) {
		if ( it->second->aboutToBeRemoved ) {
			SEISCOMP_DEBUG("... remove event %s from cache",
			               it->second->event->publicID());
			_events.erase(it++);
		}
		else
			++it;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool EventTool::hasDelayedEvent(const std::string &publicID,
                                DelayReason reason) const {
	DelayEventBuffer::const_iterator it;
	for ( it = _delayEventBuffer.begin(); it != _delayEventBuffer.end(); ++it ) {
		if ( publicID == it->id && reason == it->reason )
			return true;
	}

	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
#define LOG_WITH_SOURCE(MSG, ...) \
	do {\
		if ( _lastNetworkMessage ) {\
			SEISCOMP_LOG(_infoChannel, MSG " from %s through %s", __VA_ARGS__,\
			             _lastNetworkMessage->sender, _lastNetworkMessage->target);\
		}\
		else {\
			SEISCOMP_LOG(_infoChannel, MSG, __VA_ARGS__);\
		}\
	}\
	while ( 0 )

void EventTool::addObject(const string &parentID, Object* object) {
	OriginPtr org = Origin::Cast(object);
	if ( org ) {
		logObject(_inputOrigin, Core::Time::UTC());
		SEISCOMP_DEBUG("* queued new origin %s (%ld, %d/%lu)",
		              org->publicID(), (long int)org.get(),
		              definingPhaseCount(org.get()), (unsigned long)org->arrivalCount());
		LOG_WITH_SOURCE("Received new origin %s", org->publicID());
		_adds.insert(TodoEntry(org));
		return;
	}

	Magnitude *mag = Magnitude::Cast(object);
	if ( mag ) {
		logObject(_inputMagnitude, Core::Time::UTC());
		org = _cache.get<Origin>(parentID);
		if ( org ) {
			if ( org != mag->origin() )
				org->add(mag);

			try {
				LOG_WITH_SOURCE("Received new magnitude %s (%s %.2f)",
				                mag->publicID(), mag->type(), mag->magnitude().value());
			}
			catch ( ... ) {
				LOG_WITH_SOURCE("Received new magnitude %s (%s -.--)",
				                mag->publicID(), mag->type());
			}

			_updates.insert(TodoEntry(org));
		}
		return;
	}

	FocalMechanismPtr fm = FocalMechanism::Cast(object);
	if ( fm ) {
		logObject(_inputFocalMechanism, Core::Time::UTC());
		SEISCOMP_DEBUG("* queued new focal mechanism %s (%ld)",
		               fm->publicID(), (long int)fm.get());
		LOG_WITH_SOURCE("Received new focalmechanism %s", fm->publicID());
		_adds.insert(TodoEntry(fm));
		return;
	}

	MomentTensor *mt = MomentTensor::Cast(object);
	if ( mt ) {
		logObject(_inputMomentTensor, Core::Time::UTC());
		fm = _cache.get<FocalMechanism>(parentID);
		if ( fm ) {
			if ( fm != mt->focalMechanism() )
				fm->add(mt);

			LOG_WITH_SOURCE("Received new momenttensor %s", mt->publicID());

			if ( _config.eventAssociation.ignoreMTDerivedOrigins ) {
				// Blacklist the derived originID to prevent event
				// association.
				_originBlackList.insert(mt->derivedOriginID());
			}
		}
		return;
	}


	OriginReference *ref = OriginReference::Cast(object);
	if ( ref && !ref->originID().empty() ) {
		logObject(_inputOriginRef, Core::Time::UTC());
		LOG_WITH_SOURCE("Received new origin reference %s for event %s",
		                ref->originID(), parentID);

		EventInformationPtr info = cachedEvent(parentID);
		if ( !info ) {
			info = new EventInformation(&_cache, &_config, query(), parentID, author());
			if ( !info->event ) {
				SEISCOMP_ERROR("event %s for OriginReference not found", parentID);
				SEISCOMP_LOG(_infoChannel, " - skipped, event %s not found in database", parentID);
				return;
			}

			info->loadAssocations(query());
			cacheEvent(info);
		}

		org = _cache.get<Origin>(ref->originID());

		auto it = _adds.find(TodoEntry(org));
		// If this origin has to be associated in this turn
		if ( it != _adds.end() ) {
			// Remove the origin from the association list
			_adds.erase(it);
			// Add it to the origin updates (not triggered by a magnitude change)
			_realUpdates.insert(TodoEntry(org));
			SEISCOMP_DEBUG("* removed new origin %s from queue because of preset association", (*it)->publicID());
		}

		_updates.insert(TodoEntry(org));

		return;
	}

	FocalMechanismReference *fm_ref = FocalMechanismReference::Cast(object);
	if ( fm_ref && !fm_ref->focalMechanismID().empty() ) {
		logObject(_inputFMRef, Core::Time::UTC());
		LOG_WITH_SOURCE("Received new focal mechanism reference %s for event %s",
		                fm_ref->focalMechanismID(), parentID);

		EventInformationPtr info = cachedEvent(parentID);
		if ( !info ) {
			info = new EventInformation(&_cache, &_config, query(), parentID, author());
			if ( !info->event ) {
				SEISCOMP_ERROR("event %s for ForcalMechanismReference not found", parentID);
				SEISCOMP_LOG(_infoChannel, " - skipped, event %s not found in database", parentID);
				return;
			}

			info->loadAssocations(query());
			cacheEvent(info);
		}

		fm = _cache.get<FocalMechanism>(fm_ref->focalMechanismID());

		auto it = _adds.find(TodoEntry(fm));
		// If this origin has to be associated in this turn
		if ( it != _adds.end() ) {
			// Remove the focal mechanism from the association list
			_adds.erase(it);
			// Add it to the focal mechanism updates (not triggered by a magnitude change)
			_realUpdates.insert(TodoEntry(fm));
			SEISCOMP_DEBUG("* removed new focal mechanism %s from queue because of preset association", (*it)->publicID());
		}

		_updates.insert(TodoEntry(fm));

		return;
	}

	EventPtr evt = Event::Cast(object);
	if ( evt ) {
		logObject(_inputEvent, Core::Time::UTC());
		LOG_WITH_SOURCE("Received new event %s", evt->publicID());
		EventInformationPtr info = cachedEvent(evt->publicID());
		if ( !info ) {
			info = new EventInformation(&_cache, &_config, query(), evt, author());
			// Loading the references does not make sense here, because
			// the event has been just added
			cacheEvent(info);
		}
		return;
	}

	JournalEntryPtr journalEntry = JournalEntry::Cast(object);
	if ( journalEntry ) {
		logObject(_inputJournal, Core::Time::UTC());
		LOG_WITH_SOURCE("Received new journal entry from %s for object %s: %s(%s)",
		                journalEntry->sender(), journalEntry->objectID(),
		                journalEntry->action(), journalEntry->parameters());
		if ( handleJournalEntry(journalEntry.get()) ) {
			return;
		}
	}

	// We are not interested in anything else than the parsed
	// objects
	if ( object->parent() == _ep.get() || object->parent() == _journal.get() )
		object->detach();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::updateObject(const std::string &parentID, Object* object) {
	OriginPtr org = Origin::Cast(object);
	if ( org ) {
		logObject(_inputOrigin, Core::Time::UTC());
		if ( !org->registered() ) {
			org = Origin::Find(org->publicID());
			if ( !org ) {
				SEISCOMP_WARNING("Unexpected behaviour: origin update does not have an registered counterpart");
				return;
			}
		}
		_updates.insert(TodoEntry(org));
		_realUpdates.insert(TodoEntry(org));
		SEISCOMP_DEBUG("* queued updated origin %s (%d/%lu)",
		               org->publicID(),
		               definingPhaseCount(org.get()), (unsigned long)org->arrivalCount());
		LOG_WITH_SOURCE("Received updated origin %s", org->publicID());
		return;
	}

	FocalMechanismPtr fm = FocalMechanism::Cast(object);
	if ( fm ) {
		logObject(_inputFocalMechanism, Core::Time::UTC());
		if ( !fm->registered() ) {
			fm = FocalMechanism::Find(fm->publicID());
			if ( !fm ) {
				SEISCOMP_WARNING("Unexpected behaviour: fm update does not have an registered counterpart");
				return;
			}
		}
		_updates.insert(TodoEntry(fm));
		_realUpdates.insert(TodoEntry(fm));
		SEISCOMP_DEBUG("* queued updated focal mechanism %s", fm->publicID());
		LOG_WITH_SOURCE("Received updated focal mechanism %s", fm->publicID());
		return;
	}

	MagnitudePtr mag = Magnitude::Cast(object);
	if ( mag ) {
		logObject(_inputMagnitude, Core::Time::UTC());
		if ( !mag->registered() ) {
			mag = Magnitude::Find(mag->publicID());
			if ( !mag ) {
				SEISCOMP_WARNING("Unexpected behaviour: magnitude update does not have an registered counterpart");
				return;
			}
		}

		try {
			LOG_WITH_SOURCE("Received updated magnitude %s (%s %.2f)",
			                mag->publicID(), mag->type(), mag->magnitude().value());
		}
		catch ( ... ) {
			LOG_WITH_SOURCE("Received updated magnitude %s (%s -.--)",
			                mag->publicID(), mag->type());
		}

		org = _cache.get<Origin>(parentID);
		if ( org ) {
			_updates.insert(TodoEntry(org, mag));
		}

		return;
	}

	EventPtr evt = Event::Cast(object);
	if ( evt ) {
		logObject(_inputEvent, Core::Time::UTC());
		if ( !evt->registered() ) {
			evt = Event::Find(evt->publicID());
			if ( !evt ) {
				SEISCOMP_WARNING("Unexpected behaviour: event update does not have an registered counterpart");
				return;
			}
		}
		LOG_WITH_SOURCE("Received updated event %s", evt->publicID());
		EventInformationPtr info = cachedEvent(evt->publicID());
		if ( !info ) {
			info = new EventInformation(&_cache, &_config, query(), evt, author());
			info->loadAssocations(query());
			cacheEvent(info);
		}

		// NOTE: What to do with an event update?
		// What we can do is to compare the cached event by the updated
		// one and resent it if other attributes than preferredOriginID
		// and preferredMagnitudeID has changed (e.g. status)
		// That does not avoid race conditions but minimizes them with
		// the current implementation.
		// For now it's getting cached unless it has been done already.
		return;
	}

	if ( object->parent() == _ep.get() || object->parent() == _journal.get() ) {
		object->detach();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::removeObject(const string &parentID, Object* object) {
	OriginReference *ref = OriginReference::Cast(object);
	if ( ref ) {
		SEISCOMP_DEBUG("%s: origin reference '%s' removed "
		               "from outside", parentID, ref->originID());
		SEISCOMP_LOG(_infoChannel, "%s: origin reference '%s' removed "
		             "from outside", parentID, ref->originID());

		EventInformationPtr info = cachedEvent(parentID);
		if ( !info ) {
			info = new EventInformation(&_cache, &_config, query(), parentID, author());
			info->loadAssocations(query());
			if ( !info->event ) {
				SEISCOMP_ERROR("event %s for OriginReference not found", parentID);
				return;
			}
			cacheEvent(info);
		}
		else if ( !info->event ) {
			info->dirtyPickSet = true;
			SEISCOMP_ERROR("event %s for OriginReference not found", parentID);
			return;
		}

		info->dirtyPickSet = true;

		if ( info->event->originReferenceCount() == 0 ) {
			SEISCOMP_DEBUG("%s: last origin reference removed, remove event",
			               parentID);
			SEISCOMP_LOG(_infoChannel, "%s: last origin reference removed, removing event",
			             parentID);

			Notifier::SetEnabled(true);
			info->event->detach();
			removeCachedEvent(info->event->publicID());
			_cache.remove(info->event.get());
			Notifier::SetEnabled(false);

			return;
		}

		if ( info->event->preferredOriginID() == ref->originID() ) {
			SEISCOMP_DEBUG("%s: removed origin reference was the preferred origin, set to NULL",
			               parentID);
			// Reset preferred information
			info->event->setPreferredOriginID("");
			info->event->setPreferredMagnitudeID("");
			info->preferredOrigin = nullptr;
			info->preferredMagnitude = nullptr;
			Notifier::Enable();
			// Select the preferred origin again among all remaining origins
			updatePreferredOrigin(info.get());
			Notifier::Disable();
		}

		return;
	}

	PublicObject *po = PublicObject::Cast(object);
	if ( po ) {
		SEISCOMP_DEBUG("[notifier] %s removed from %s", po->publicID(),
		               parentID);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
namespace {

DataModel::JournalEntryPtr
createEntry(const std::string &id, const std::string &proc,
            const std::string &param) {
	DataModel::JournalEntryPtr e = new DataModel::JournalEntry;
	e->setObjectID(id);
	e->setAction(proc);
	e->setParameters(param);
	e->setCreated(Core::Time::UTC());
	return e;
}

}

bool EventTool::handleJournalEntry(DataModel::JournalEntry *entry) {
	JournalEntryPtr response;

	static const std::string Failed = "Failed";
	static const std::string OK = "OK";

	// Not event specific journals
	if ( entry->action() == "EvNewEvent" ) {
		SEISCOMP_INFO("Handling journal entry for origin %s: %s(%s)",
		              entry->objectID(), entry->action(), entry->parameters());

		EventPtr e = getEventForOrigin(entry->objectID());
		if ( e )
			response = createEntry(entry->objectID(), entry->action() + Failed, ":origin already associated:");
		else {
			OriginPtr origin = _cache.get<Origin>(entry->objectID());
			if ( !origin ) {
				SEISCOMP_INFO("origin %s for JournalEntry not found", entry->objectID());
				return false;
			}

			if ( !origin->magnitudeCount() && query() ) {
				SEISCOMP_DEBUG("... loading magnitudes for origin %s", origin->publicID());
				query()->loadMagnitudes(origin.get());
			}

			EventInformationPtr info = createEvent(origin.get());
			if ( info ) {
				SEISCOMP_INFO("%s: created", info->event->publicID());
				SEISCOMP_LOG(_infoChannel, "Origin %s created a new event %s",
				             origin->publicID(), info->event->publicID());

				Notifier::Enable();
				if ( !info->associate(origin.get()) ) {
					SEISCOMP_ERROR("Association of origin %s to event %s failed",
					               origin->publicID(), info->event->publicID());
					SEISCOMP_LOG(_infoChannel, "Failed to associate origin %s to event %s",
					             origin->publicID(), info->event->publicID());
				}
				else {
					logObject(_outputOriginRef, Time::UTC());
					SEISCOMP_INFO("%s: associated origin %s", info->event->publicID(),
					              origin->publicID());
					SEISCOMP_LOG(_infoChannel, "Origin %s associated to event %s",
					             origin->publicID(), info->event->publicID());
					choosePreferred(info.get(), origin.get(), nullptr, true);
				}

				Notifier::Disable();

				response = createEntry(info->event->publicID(), entry->action() + OK, "created by command");
				response->setSender(author());
				info->addJournalEntry(response.get(), author());
				Notifier::Enable();
				Notifier::Create(_journal->publicID(), OP_ADD, response.get());
				Notifier::Disable();

				response = createEntry(entry->objectID(), entry->action() + OK, string("associated to event ") + info->event->publicID());
			}
			else
				response = createEntry(entry->objectID(), entry->action() + Failed, ":running out of eventIDs:");
		}

		if ( response ) {
			response->setSender(author());
			Notifier::Enable();
			Notifier::Create(_journal->publicID(), OP_ADD, response.get());
			Notifier::Disable();
		}

		return true;
	}


	SEISCOMP_INFO("Handling journal entry for event %s: %s(%s)",
	              entry->objectID(), entry->action(), entry->parameters());

	EventInformationPtr info = cachedEvent(entry->objectID());

	if ( !info ) {
		// No chached information yet -> load it and cache it
		info = new EventInformation(&_cache, &_config, query(), entry->objectID(), author());
		if ( !info->event ) {
			SEISCOMP_INFO("event %s for JournalEntry not found", entry->objectID());
			return false;
		}

		info->loadAssocations(query());
		cacheEvent(info);
	}
	else
		info->addJournalEntry(entry, author());

	if ( !info->event->parent() ) {
		SEISCOMP_ERROR("%s: internal error: no parent", info->event->publicID());
	}

	if ( entry->action() == "EvPrefMagType" ) {
		SEISCOMP_DEBUG("...set preferred magnitude type");
		response = createEntry(entry->objectID(), entry->action() + OK, !info->constraints.preferredMagnitudeType.empty()?info->constraints.preferredMagnitudeType:":automatic:");

		if ( info->preferredOrigin && !info->preferredOrigin->magnitudeCount() && query() ) {
			SEISCOMP_DEBUG("... loading magnitudes for origin %s", info->preferredOrigin->publicID());
			query()->loadMagnitudes(info->preferredOrigin.get());
		}

		// Choose the new preferred magnitude
		Notifier::Enable();
		choosePreferred(info.get(), info->preferredOrigin.get(), NULL);
		Notifier::Disable();
	}
	else if ( entry->action() == "EvPrefOrgID" ) {
		SEISCOMP_DEBUG("...set preferred origin by ID");

		// Release fixed origin and choose the best one automatically
		if ( info->constraints.preferredOriginID.empty() ) {
			response = createEntry(entry->objectID(), entry->action() + OK, ":automatic:");

			Notifier::Enable();
			updatePreferredOrigin(info.get());
			Notifier::Disable();
		}
		else {
			if ( !info->event->originReference(info->constraints.preferredOriginID) ) {
				response = createEntry(entry->objectID(), entry->action() + Failed, ":unreferenced:");
			}
			else {
				OriginPtr org = _cache.get<Origin>(info->constraints.preferredOriginID);
				if ( org ) {
					if ( !org->magnitudeCount() && query() ) {
						SEISCOMP_DEBUG("... loading magnitudes for origin %s", org->publicID());
						query()->loadMagnitudes(org.get());
					}

					response = createEntry(entry->objectID(), entry->action() + OK, info->constraints.preferredOriginID);
					Notifier::Enable();
					choosePreferred(info.get(), org.get(), nullptr);
					Notifier::Disable();
				}
				else {
					response = createEntry(entry->objectID(), entry->action() + Failed, ":not available:");
				}
			}
		}
	}
	else if ( entry->action() == "EvPrefOrgEvalMode" ) {
		SEISCOMP_DEBUG("...set preferred origin by evaluation mode");

		DataModel::EvaluationMode em;
		if ( !em.fromString(entry->parameters().c_str()) && !entry->parameters().empty() ) {
			// If a mode is requested but an invalid mode identifier has been submitted
			response = createEntry(entry->objectID(), entry->action() + Failed, string(":mode '") + entry->parameters() + "' unknown:");
		}
		else {
			// Release fixed origin mode and choose the best one automatically
			if ( !info->constraints.preferredOriginEvaluationMode )
				response = createEntry(entry->objectID(), entry->action() + OK, ":automatic:");
			else
				response = createEntry(entry->objectID(), entry->action() + OK, entry->parameters());

			Notifier::Enable();
			updatePreferredOrigin(info.get());
			Notifier::Disable();
		}
	}
	else if ( entry->action() == "EvPrefOrgAutomatic" ) {
		response = createEntry(entry->objectID(), entry->action() + OK, ":automatic mode:");

		Notifier::Enable();
		updatePreferredOrigin(info.get());
		Notifier::Disable();
	}
	else if ( entry->action() == "EvType" ) {
		SEISCOMP_DEBUG("...set event type");

		OPT(EventType) et;
		EventType newEt;

		if ( !entry->parameters().empty() && !newEt.fromString(entry->parameters()) ) {
			response = createEntry(entry->objectID(), entry->action() + Failed, ":invalid type:");
		}
		else {
			try { et = info->event->type(); }
			catch ( ... ) {}

			if ( !et && entry->parameters().empty() ) {
				response = createEntry(entry->objectID(), entry->action() + Failed, ":not modified:");
			}
			else if ( et && !entry->parameters().empty() && *et == newEt ) {
				response = createEntry(entry->objectID(), entry->action() + Failed, ":not modified:");
			}
			else {
				if ( !entry->parameters().empty() ) {
					info->event->setType(newEt);
					response = createEntry(entry->objectID(), entry->action() + OK, entry->parameters());
				}
				else {
					info->event->setType(None);
					response = createEntry(entry->objectID(), entry->action() + OK, ":unset:");
				}

				Notifier::Enable();
				updateEvent(info.get());
				Notifier::Disable();
			}
		}
	}
	else if ( entry->action() == "EvTypeCertainty" ) {
		SEISCOMP_DEBUG("...set event type certainty");

		OPT(EventTypeCertainty) etc;
		EventTypeCertainty newEtc;

		if ( !entry->parameters().empty() && !newEtc.fromString(entry->parameters()) ) {
			response = createEntry(entry->objectID(), entry->action() + Failed, ":invalid type certainty:");
		}
		else {
			try { etc = info->event->typeCertainty(); }
			catch ( ... ) {}

			if ( !etc && entry->parameters().empty() ) {
				response = createEntry(entry->objectID(), entry->action() + Failed, ":not modified:");
			}
			else if ( etc && !entry->parameters().empty() && *etc == newEtc ) {
				response = createEntry(entry->objectID(), entry->action() + Failed, ":not modified:");
			}
			else {
				if ( !entry->parameters().empty() ) {
					info->event->setTypeCertainty(newEtc);
					response = createEntry(entry->objectID(), entry->action() + OK, entry->parameters());
				}
				else {
					info->event->setTypeCertainty(None);
					response = createEntry(entry->objectID(), entry->action() + OK, ":unset:");
				}
				Notifier::Enable();
				updateEvent(info.get());
				Notifier::Disable();
			}
		}
	}
	else if ( entry->action() == "EvName" ) {
		SEISCOMP_DEBUG("...set event name");

		string error;
		Notifier::Enable();
		if ( info->setEventName(entry, error) )
			response = createEntry(entry->objectID(), entry->action() + OK, entry->parameters());
		else
			response = createEntry(entry->objectID(), entry->action() + Failed, error);
		Notifier::Disable();
	}
	else if ( entry->action() == "EvOpComment" ) {
		SEISCOMP_DEBUG("...set event operator's comment");

		string error;
		Notifier::Enable();
		if ( info->setEventOpComment(entry, error) )
			response = createEntry(entry->objectID(), entry->action() + OK, entry->parameters());
		else
			response = createEntry(entry->objectID(), entry->action() + Failed, error);
		Notifier::Disable();
	}
	else if ( entry->action() == "EvPrefFocMecID" ) {
		SEISCOMP_DEBUG("...set event preferred focal mechanism");

		if ( entry->parameters().empty() ) {
			info->event->setPreferredFocalMechanismID(string());
			response = createEntry(entry->objectID(), entry->action() + OK, ":unset:");
		}
		else {
			if ( !info->event->focalMechanismReference(info->constraints.preferredFocalMechanismID) ) {
				response = createEntry(entry->objectID(), entry->action() + Failed, ":unreferenced:");
			}
			else {
				FocalMechanismPtr fm = _cache.get<FocalMechanism>(info->constraints.preferredFocalMechanismID);
				if ( fm ) {
					if ( !fm->momentTensorCount() && query() ) {
						SEISCOMP_DEBUG("... loading moment tensors for focal mechanism %s", fm->publicID());
						query()->loadMomentTensors(fm.get());
					}

					response = createEntry(entry->objectID(), entry->action() + OK, info->constraints.preferredFocalMechanismID);
					Notifier::Enable();
					choosePreferred(info.get(), fm.get());
					Notifier::Disable();
				}
				else {
					response = createEntry(entry->objectID(), entry->action() + Failed, ":not available:");
				}
			}
		}
		Notifier::Enable();
		updateEvent(info.get());
		Notifier::Disable();
	}
	else if ( entry->action() == "EvPrefMw" ) {
		SEISCOMP_DEBUG("...set Mw from focal mechanism as preferred magnitude");
		if ( entry->parameters().empty() ) {
			response = createEntry(entry->objectID(), entry->action() + Failed, ":empty parameter (true or false expected):");
		}
		else {
			if ( entry->parameters() == "true" || entry->parameters() == "false" ) {
				response = createEntry(entry->objectID(), entry->action() + OK, entry->parameters());

				Notifier::Enable();
				choosePreferred(info.get(), info->preferredOrigin.get(), nullptr);
				Notifier::Disable();
			}
			else
				response = createEntry(entry->objectID(), entry->action() + Failed, ":true or false expected:");
		}
	}
	// Merge event in  parameters into event in objectID. The source
	// event is deleted afterwards.
	else if ( entry->action() == "EvMerge" ) {
		SEISCOMP_DEBUG("...merge event '%s'", entry->parameters());

		if ( entry->parameters().empty() )
			response = createEntry(entry->objectID(), entry->action() + Failed, ":empty source event id:");
		else if ( info->event->publicID() == entry->parameters() ) {
			response = createEntry(entry->objectID(), entry->action() + Failed, ":source and target are equal:");
		}
		else {
			EventInformationPtr sourceInfo = cachedEvent(entry->parameters());
			if ( !sourceInfo ) {
				sourceInfo = new EventInformation(&_cache, &_config, query(), entry->parameters(), author());
				if ( !sourceInfo->event ) {
					SEISCOMP_ERROR("source event %s for merge not found", entry->parameters());
					SEISCOMP_LOG(_infoChannel, " - skipped, source event %s for merge not found in database", entry->parameters());
					sourceInfo = nullptr;
				}
				else {
					sourceInfo->loadAssocations(query());
					cacheEvent(sourceInfo);
				}
			}

			if ( !sourceInfo )
				response = createEntry(entry->objectID(), entry->action() + Failed, ":source event not found:");
			else {
				Notifier::Enable();
				// Do the merge
				if ( mergeEvents(info.get(), sourceInfo.get()) ) {
					JournalEntryPtr srcResponse;
					srcResponse = createEntry(sourceInfo->event->publicID(), "EvDeleteOK", string("merged into ") + info->event->publicID());
					if ( srcResponse ) {
						srcResponse->setSender(author());
						sourceInfo->addJournalEntry(srcResponse.get(), author());
						Notifier::Enable();
						Notifier::Create(_journal->publicID(), OP_ADD, srcResponse.get());
						Notifier::Disable();
					}

					response = createEntry(entry->objectID(), entry->action() + OK, sourceInfo->event->publicID());
				}
				else
					response = createEntry(entry->objectID(), entry->action() + Failed, ":internal error:");
				Notifier::Disable();
			}
		}
	}
	// Move an origin to this event. If it is already associated to another
	// event, remove this reference
	else if ( entry->action() == "EvGrabOrg") {
		SEISCOMP_DEBUG("...grab origin '%s'", entry->parameters());
		OriginPtr org = _cache.get<Origin>(entry->parameters());
		list<string> fmIDsToMove;

		if ( !org )
			response = createEntry(entry->objectID(), entry->action() + Failed, ":origin not found:");
		else {
			EventPtr e = getEventForOrigin(org->publicID());
			if ( e ) {
				EventInformationPtr sourceInfo = cachedEvent(e->publicID());
				if ( !sourceInfo ) {
					sourceInfo = new EventInformation(&_cache, &_config, query(), e, author());
					sourceInfo->loadAssocations(query());
					cacheEvent(sourceInfo);
				}

				if ( sourceInfo->event->originReferenceCount() < 2 ) {
					response = createEntry(entry->objectID(), entry->action() + Failed, ":last origin cannot be removed:");
				}
				else {
					Notifier::Enable();
					sourceInfo->event->removeOriginReference(org->publicID());

					// Remove all focal mechanism references that
					// used this origin as trigger
					bool updatedPrefFM = false;
					for ( size_t i = 0; i < sourceInfo->event->focalMechanismReferenceCount(); ) {
						FocalMechanismPtr fm = _cache.get<FocalMechanism>(sourceInfo->event->focalMechanismReference(i)->focalMechanismID());
						if ( !fm ) { ++i; continue; }
						if ( fm->triggeringOriginID() == org->publicID() ) {
							fmIDsToMove.push_back(fm->publicID());
							sourceInfo->event->removeFocalMechanismReference(i);
							if ( fm->publicID() == sourceInfo->event->preferredFocalMechanismID() )
								updatedPrefFM = true;
						}
						else
							++i;
					}

					if ( sourceInfo->event->preferredOriginID() == org->publicID() ) {
						SEISCOMP_DEBUG("%s: removed origin reference was the preferred origin, set to NULL",
						               sourceInfo->event->publicID());
						// Reset preferred information
						sourceInfo->event->setPreferredOriginID("");
						sourceInfo->event->setPreferredMagnitudeID("");
						sourceInfo->preferredOrigin = nullptr;
						sourceInfo->preferredMagnitude = nullptr;
						// Select the preferred origin again among all remaining origins
						updatePreferredOrigin(sourceInfo.get());
					}

					if ( updatedPrefFM ) {
						SEISCOMP_DEBUG("%s: removed focal mechanism reference was the preferred focal mechanism, set to NULL",
						               sourceInfo->event->publicID());
						sourceInfo->event->setPreferredFocalMechanismID(string());
						sourceInfo->preferredFocalMechanism = nullptr;
						updatePreferredFocalMechanism(sourceInfo.get());
					}

					Notifier::Disable();

					e = nullptr;
				}
			}

			// If the event is still a valid pointer an error occured while
			// removing the reference
			if ( !e ) {
				Notifier::Enable();
				info->associate(org.get());
				logObject(_outputOriginRef, Time::UTC());
				// Associate focal mechanism references
				for ( auto it = fmIDsToMove.begin(); it != fmIDsToMove.end(); ++it ) {
					logObject(_outputFMRef, Time::UTC());
					info->event->add(new FocalMechanismReference(*it));
				}

				updatePreferredOrigin(info.get());
				updatePreferredFocalMechanism(info.get());

				Notifier::Disable();

				response = createEntry(entry->objectID(), entry->action() + OK, org->publicID());
			}
		}
	}
	// Remove an origin reference from an event and create a new event for
	// this origin
	else if ( entry->action() == "EvSplitOrg" ) {
		SEISCOMP_DEBUG("...split origin '%s' and create a new event", entry->parameters());
		OriginPtr org = _cache.get<Origin>(entry->parameters());
		list<string> fmIDsToMove;

		if ( !org )
			response = createEntry(entry->objectID(), entry->action() + Failed, ":origin not found:");
		else {
			if ( !info->event->originReference(org->publicID()) )
				response = createEntry(entry->objectID(), entry->action() + Failed, ":origin not associated:");
			else {
				if ( info->event->originReferenceCount() < 2 )
					response = createEntry(entry->objectID(), entry->action() + Failed, ":last origin cannot be removed:");
				else {
					EventInformationPtr newInfo = createEvent(org.get());
					JournalEntryPtr newResponse;

					if ( newInfo ) {
						// Remove origin reference
						Notifier::SetEnabled(true);
						if ( info->event->removeOriginReference(org->publicID()) )
							info->dirtyPickSet = true;

						// Remove all focal mechanism references that
						// used this origin as trigger
						bool updatedPrefFM = false;
						for ( size_t i = 0; i < info->event->focalMechanismReferenceCount(); ) {
							FocalMechanismPtr fm = _cache.get<FocalMechanism>(info->event->focalMechanismReference(i)->focalMechanismID());
							if ( !fm ) { ++i; continue; }
							if ( fm->triggeringOriginID() == org->publicID() ) {
								SEISCOMP_DEBUG("...scheduled focal mechanism %s for split",
								               fm->publicID());
								fmIDsToMove.push_back(fm->publicID());
								info->event->removeFocalMechanismReference(i);
								if ( fm->publicID() == info->event->preferredFocalMechanismID() )
									updatedPrefFM = true;
							}
							else
								++i;
						}
						Notifier::Enable();

						if ( info->event->preferredOriginID() == org->publicID() ) {
							SEISCOMP_DEBUG("%s: removed origin reference was the preferred origin, set to NULL",
							               info->event->publicID());
							// Reset preferred information
							info->event->setPreferredOriginID("");
							info->event->setPreferredMagnitudeID("");
							info->preferredOrigin = nullptr;
							info->preferredMagnitude = nullptr;
							// Select the preferred origin again among all remaining origins
							updatePreferredOrigin(info.get());

						}

						if ( updatedPrefFM ) {
							SEISCOMP_DEBUG("%s: removed focal mechanism reference was the preferred focal mechanism, set to NULL",
							               info->event->publicID());
							info->event->setPreferredFocalMechanismID(string());
							info->preferredFocalMechanism = nullptr;
							updatePreferredFocalMechanism(info.get());
						}

						Notifier::Disable();

						response = createEntry(entry->objectID(), entry->action() + OK, org->publicID() + " removed by command");

						SEISCOMP_INFO("%s: created", newInfo->event->publicID());
						SEISCOMP_LOG(_infoChannel, "Origin %s created a new event %s",
						             org->publicID(), newInfo->event->publicID());

						Notifier::Enable();
						newInfo->associate(org.get());
						logObject(_outputOriginRef, Time::UTC());
						// Associate focal mechanism references
						for ( auto it = fmIDsToMove.begin(); it != fmIDsToMove.end(); ++it ) {
							SEISCOMP_INFO("%s: associated focal mechanism %s", newInfo->event->publicID(),
							              it->c_str());
							newInfo->event->add(new FocalMechanismReference(*it));
							logObject(_outputFMRef, Time::UTC());
						}
						SEISCOMP_INFO("%s: associated origin %s", newInfo->event->publicID(),
						              org->publicID());
						SEISCOMP_LOG(_infoChannel, "Origin %s associated to event %s",
						             org->publicID(), newInfo->event->publicID());

						if ( !org->magnitudeCount() && query() ) {
							SEISCOMP_DEBUG("... loading magnitudes for origin %s", org->publicID());
							query()->loadMagnitudes(org.get());
						}

						choosePreferred(newInfo.get(), org.get(), nullptr, true);
						updatePreferredFocalMechanism(newInfo.get());
						Notifier::Disable();

						newResponse = createEntry(newInfo->event->publicID(), "EvNewEventOK", "created by command");
						if ( newResponse ) {
							newResponse->setSender(author());
							newInfo->addJournalEntry(newResponse.get(), author());
							Notifier::Enable();
							Notifier::Create(_journal->publicID(), OP_ADD, newResponse.get());
							Notifier::Disable();
						}
					}
					else
						response = createEntry(entry->objectID(), entry->action() + Failed, ":running out of eventIDs:");
				}
			}
		}
	}
	else if ( entry->action() == "EvRefresh" ) {
		response = createEntry(entry->objectID(), entry->action() + OK, ":refreshing:");

		Notifier::Enable();
		updatePreferredOrigin(info.get(), true);
		Notifier::Disable();
	}
	// Is this command ours by starting with Ev?
	else if ( !entry->action().empty() && (entry->action().compare(0, 2,"Ev") == 0) ) {
		// Make sure we don't process result journal entries ending on either
		// OK or Failed
		if ( ( entry->action().size() >= Failed.size() + 2 &&
		       std::equal(Failed.rbegin(), Failed.rend(), entry->action().rbegin() ) ) ||
		     ( entry->action().size() >= OK.size() + 2 &&
		       std::equal(OK.rbegin(), OK.rend(), entry->action().rbegin() ) ) )
			SEISCOMP_ERROR("Ignoring already processed journal entry from %s: %s(%s)",
			               entry->sender(), entry->action(), entry->parameters());
		else
			response = createEntry(entry->objectID(), entry->action() + Failed, ":unknown command:");
	}

	if ( response ) {
		response->setSender(author());
		info->addJournalEntry(response.get(), author());
		Notifier::Enable();
		Notifier::Create(_journal->publicID(), OP_ADD, response.get());
		Notifier::Disable();
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EventInformationPtr EventTool::associateOriginCheckDelay(DataModel::Origin *origin) {
	if ( _config.eventAssociation.delayTimeSpan > 0 ) {
		SEISCOMP_LOG(_infoChannel, "Checking delay filter for origin %s",
		             origin->publicID());

		EvaluationMode mode = AUTOMATIC;
		try {
			mode = origin->evaluationMode();
		}
		catch ( ValueException& ) {}

		if ( _config.eventAssociation.delayFilter.agencyID &&
		     objectAgencyID(origin) != *_config.eventAssociation.delayFilter.agencyID ) {
			SEISCOMP_LOG(_infoChannel, " * agency does not match (%s != %s), process immediately",
			             objectAgencyID(origin), *_config.eventAssociation.delayFilter.agencyID);
			return associateOrigin(origin, true);
		}

		if ( _config.eventAssociation.delayFilter.author &&
		     objectAuthor(origin) != *_config.eventAssociation.delayFilter.author ) {
			SEISCOMP_LOG(_infoChannel, " * author does not match (%s != %s), process immediately",
			             objectAuthor(origin), *_config.eventAssociation.delayFilter.author);
			return associateOrigin(origin, true);
		}

		if ( _config.eventAssociation.delayFilter.evaluationMode &&
		     mode != *_config.eventAssociation.delayFilter.evaluationMode ) {
			SEISCOMP_LOG(_infoChannel, " * evaluationMode does not match (%s != %s), process immediately",
			             mode.toString(), _config.eventAssociation.delayFilter.evaluationMode->toString());
			return associateOrigin(origin, true);
		}

		// Filter to delay the origin passes
		SEISCOMP_LOG(_infoChannel, "Origin %s delayed for %i s",
		             origin->publicID(), _config.eventAssociation.delayTimeSpan);
		_delayBuffer.push_back(DelayedObject(origin, _config.eventAssociation.delayTimeSpan));

		return nullptr;
	}

	return associateOrigin(origin, true);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EventInformationPtr EventTool::associateOrigin(Seiscomp::DataModel::Origin *origin,
                                               bool allowEventCreation,
                                               bool *createdEvent,
                                               const EventInformation::PickCache *pickCache) {
	if ( createdEvent ) {
		*createdEvent = false;
	}

	// Default origin status
	EvaluationMode status = AUTOMATIC;
	try {
		status = origin->evaluationMode();
	}
	catch ( Core::ValueException &e ) {
	}

	// Find a matching (cached) event for this origin
	EventInformationPtr info = findMatchingEvent(origin, pickCache);

	if ( !info ) {
		Core::Time startTime = origin->time().value() - _config.eventAssociation.eventTimeBefore;
		Core::Time endTime = origin->time().value() + _config.eventAssociation.eventTimeAfter;
		MatchResult bestResult = Nothing;

		SEISCOMP_DEBUG("... search for origin's %s event in database", origin->publicID());

		if ( query() ) {
			// Look for events in a certain timewindow around the origintime
			DatabaseIterator it = query()->getEvents(startTime, endTime);

			std::vector<EventPtr> fetchedEvents;
			for ( ; *it; ++it ) {
				EventPtr e = Event::Cast(*it);
				assert(e);

				if ( isAgencyIDBlocked(objectAgencyID(e.get())) ) {
					continue;
				}

				// Is this event already cached and associated with an information
				// object?
				if ( isEventCached(e->publicID()) ) {
					continue;
				}

				fetchedEvents.push_back(e);
			}

			for ( size_t i = 0; i < fetchedEvents.size(); ++i ) {
				// Load the eventinformation for this event
				EventInformationPtr tmp = new EventInformation(
					&_cache, &_config, query(), fetchedEvents[i], author()
				);
				if ( tmp->valid() ) {
					tmp->loadAssocations(query());
					MatchResult res = compare(tmp.get(), origin, pickCache);
					if ( res > bestResult ) {
						bestResult = res;
						info = tmp;
					}
				}
			}
		}

		if ( info ) {
			SEISCOMP_LOG(_infoChannel, "Found matching event %s for origin %s (code: %d)",
			             info->event->publicID(), origin->publicID(),
			             static_cast<int>(bestResult));
			SEISCOMP_DEBUG("... found best matching event %s (code: %d)", info->event->publicID(),
			               static_cast<int>(bestResult));
			cacheEvent(info);

			if ( pickCache ) {
				return info;
			}

			if ( info->event->originReference(origin->publicID()) ) {
				SEISCOMP_DEBUG("... origin already associated to event %s", info->event->publicID());
				SEISCOMP_LOG(_infoChannel, "Origin %s skipped: already associated to event %s",
				             origin->publicID(), info->event->publicID());
				info = nullptr;
			}
		}
		// Create a new event
		else {
			if ( !allowEventCreation || pickCache ) {
				return nullptr;
			}

			if ( isAgencyIDBlocked(objectAgencyID(origin)) ) {
				SEISCOMP_LOG(_infoChannel, "Origin %s skipped: agencyID blocked and is not allowed to create a new event",
				             origin->publicID());
				return nullptr;
			}

			if ( status == AUTOMATIC ) {
				if ( _config.eventAssociation.minAutomaticScore ) {
					double score = _score->evaluate(origin);
					if ( score < *_config.eventAssociation.minAutomaticScore ) {
						SEISCOMP_DEBUG("... rejecting automatic origin %s (score: %f < %f)",
						               origin->publicID(),
						               score, *_config.eventAssociation.minAutomaticScore);
						SEISCOMP_LOG(_infoChannel,
						             "Origin %s skipped: score too low (%f < %f) to create a new event",
						             origin->publicID(),
						             score, *_config.eventAssociation.minAutomaticScore);
						return nullptr;
					}
				}
				else if ( definingPhaseCount(origin) < int(_config.eventAssociation.minAutomaticArrivals) ) {
					SEISCOMP_DEBUG("... rejecting automatic origin %s (phaseCount: %d < %zu)",
					               origin->publicID(),
					               definingPhaseCount(origin),
					               _config.eventAssociation.minAutomaticArrivals);
					SEISCOMP_LOG(_infoChannel,
					             "Origin %s skipped: phaseCount too low (%d < %zu) to create a new event",
					             origin->publicID(),
					             definingPhaseCount(origin),
					             _config.eventAssociation.minAutomaticArrivals);
					return nullptr;
				}
			}

			if ( !checkRegionFilter(_config.regionFilter, origin) ) {
				return nullptr;
			}

			info = createEvent(origin);
			if ( info ) {
				if ( createdEvent ) {
					*createdEvent = true;
				}
				SEISCOMP_INFO("%s: created", info->event->publicID());
				SEISCOMP_LOG(_infoChannel, "Origin %s created a new event %s",
				             origin->publicID(), info->event->publicID());
			}
		}
	}
	else {
		SEISCOMP_DEBUG("... found cached event information %s for origin", info->event->publicID());
		SEISCOMP_LOG(_infoChannel, "Found matching event %s for origin %s",
			         info->event->publicID(), origin->publicID());

		if ( pickCache ) {
			return info;
		}
	}

	if ( info ) {
		// Found an event => so associate the origin
		Notifier::Enable();

		refreshEventCache(info);

		if ( !info->associate(origin) ) {
			SEISCOMP_ERROR("Association of origin %s to event %s failed",
			               origin->publicID(), info->event->publicID());
			SEISCOMP_LOG(_infoChannel, "Failed to associate origin %s to event %s",
			             origin->publicID(), info->event->publicID());
		}
		else {
			logObject(_outputOriginRef, Time::UTC());
			SEISCOMP_INFO("%s: associated origin %s", info->event->publicID(),
			              origin->publicID());
			SEISCOMP_LOG(_infoChannel, "Origin %s associated to event %s",
			             origin->publicID(), info->event->publicID());
			choosePreferred(info.get(), origin, nullptr, true);
		}

		Notifier::Disable();
	}

	_cache.feed(origin);

	return info;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool EventTool::checkRegionFilter(const Config::RegionFilters &fs, const Origin *origin) {
	Config::RegionFilters::const_iterator it;
	for ( it = fs.begin(); it != fs.end(); ++it ) {
		const Config::RegionFilter &f = *it;
		if ( f.region ) {
			try {
				if ( !f.region->isInside(origin->latitude(),
				                         origin->longitude()) ) {
					SEISCOMP_DEBUG("... rejecting automatic origin %s: region "
					               "criterion not met",
					               origin->publicID());
					SEISCOMP_LOG(_infoChannel, "Origin %s skipped: region criteria not met",
					             origin->publicID());
					return false;
				}
			}
			catch ( ValueException &exc ) {
				SEISCOMP_DEBUG("...region check exception: %s", exc.what());
				SEISCOMP_LOG(_infoChannel, "Origin %s skipped: region check exception: %s",
					         origin->publicID(), exc.what());
				return false;
			}
		}

		if ( f.minDepth ) {
			try {
				if ( origin->depth().value() < *f.minDepth ) {
					SEISCOMP_DEBUG("... rejecting automatic origin %s: min depth "
					               "criterion not met",
					               origin->publicID());
					SEISCOMP_LOG(_infoChannel, "Origin %s skipped: min depth criteria not met",
					             origin->publicID());
					return false;
				}
			}
			catch ( ... ) {}
		}

		if ( f.maxDepth ) {
			try {
				if ( origin->depth().value() > *f.maxDepth ) {
					SEISCOMP_DEBUG("... rejecting automatic origin %s: max depth "
					               "criterion not met",
					               origin->publicID());
					SEISCOMP_LOG(_infoChannel, "Origin %s skipped: max depth criteria not met",
					             origin->publicID());
					return false;
				}
			}
			catch ( ... ) {}
		}
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::updatedOrigin(DataModel::Origin *org,
                              DataModel::Magnitude *mag, bool realOriginUpdate) {
	// Get the cached origin, not the one sent in this message
	// If there is no cached origin the same instance is passed back
	Origin *origin = Origin::Find(org->publicID());
	if ( origin ) {
		if ( origin != org ) {
			*origin = *org;
		}
	}
	else {
		origin = org;
	}

	EventInformationPtr info = findAssociatedEvent(origin);
	if ( !info ) {
		EventPtr e;
		if ( query() ) {
			SEISCOMP_DEBUG("... search for origin's %s event in database", origin->publicID());
			e = getEventForOrigin(origin->publicID());
		}

		if ( !e ) {
			SEISCOMP_DEBUG("... updated origin %s has not been associated yet, doing this",
			               origin->publicID());
			associateOrigin(origin, true);
			return;
		}

		info = new EventInformation(&_cache, &_config, query(), e, author());
		info->loadAssocations(query());
		cacheEvent(info);
	}
	else {
		SEISCOMP_DEBUG("... found cached event information %s for origin", info->event->publicID());
	}

	if ( !origin->arrivalCount() && query() ) {
		SEISCOMP_DEBUG("... loading arrivals for origin %s", origin->publicID());
		query()->loadArrivals(origin);
	}

	if ( !origin->magnitudeCount() && query() ) {
		SEISCOMP_DEBUG("... loading magnitudes for origin %s", origin->publicID());
		query()->loadMagnitudes(origin);
	}

	// Cache this origin
	_cache.feed(origin);

	Notifier::Enable();
	if ( realOriginUpdate &&
	     info->event->preferredOriginID() == origin->publicID() ) {
		updatePreferredOrigin(info.get());
	}
	else
		choosePreferred(info.get(), origin, mag, realOriginUpdate);
	Notifier::Disable();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EventInformationPtr EventTool::associateFocalMechanismCheckDelay(DataModel::FocalMechanism *fm) {
	if ( _config.eventAssociation.delayTimeSpan > 0 ) {
		EvaluationMode mode = AUTOMATIC;
		try {
			mode = fm->evaluationMode();
		}
		catch ( ValueException& ) {}

		if ( _config.eventAssociation.delayFilter.agencyID &&
		     objectAgencyID(fm) != *_config.eventAssociation.delayFilter.agencyID )
			return associateFocalMechanism(fm);

		if ( _config.eventAssociation.delayFilter.author &&
		     objectAuthor(fm) != *_config.eventAssociation.delayFilter.author )
			return associateFocalMechanism(fm);

		if ( _config.eventAssociation.delayFilter.evaluationMode &&
		     mode != *_config.eventAssociation.delayFilter.evaluationMode )
			return associateFocalMechanism(fm);

		// Filter to delay the origin passes
		SEISCOMP_LOG(_infoChannel, "FocalMechanism %s delayed", fm->publicID());
		_delayBuffer.push_back(DelayedObject(fm, _config.eventAssociation.delayTimeSpan));

		return nullptr;
	}

	return associateFocalMechanism(fm);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EventInformationPtr EventTool::associateFocalMechanism(FocalMechanism *fm) {
	EventInformationPtr info;

	OriginPtr triggeringOrigin = _cache.get<Origin>(fm->triggeringOriginID());
	if ( !triggeringOrigin ) {
		SEISCOMP_DEBUG("... triggering origin %s not found, skipping event association",
		               fm->triggeringOriginID());
		return info;
	}

	info = findAssociatedEvent(triggeringOrigin.get());
	if ( !info ) {
		EventPtr e;
		if ( query() ) {
			SEISCOMP_DEBUG("... search for triggering origin's %s event in database", triggeringOrigin->publicID());
			e = getEventForOrigin(triggeringOrigin->publicID());
		}

		if ( !e ) {
			SEISCOMP_DEBUG("... triggering origin %s has not been associated yet, skipping focal mechanism",
			               triggeringOrigin->publicID());
			return nullptr;
		}

		info = new EventInformation(&_cache, &_config, query(), e, author());
		info->loadAssocations(query());
		cacheEvent(info);
	}
	else {
		SEISCOMP_DEBUG("... found cached event information %s for focal mechanism", info->event->publicID());
	}

	if ( !fm->momentTensorCount() && query() ) {
		SEISCOMP_DEBUG("... loading moment tensor for focal mechanism %s", fm->publicID());
		query()->loadMomentTensors(fm);
	}

	// Cache this origin
	_cache.feed(fm);

	Notifier::Enable();
	if ( !info->associate(fm) ) {
		SEISCOMP_ERROR("Association of focal mechanism %s to event %s failed",
		               fm->publicID(), info->event->publicID());
		SEISCOMP_LOG(_infoChannel, "Failed to associate focal mechanism %s to event %s",
		             fm->publicID(), info->event->publicID());
	}
	else {
		logObject(_outputFMRef, Time::UTC());
		SEISCOMP_INFO("%s: associated focal mechanism %s", info->event->publicID(),
		              fm->publicID());
		SEISCOMP_LOG(_infoChannel, "FocalMechanism %s associated to event %s",
		             fm->publicID(), info->event->publicID());
		choosePreferred(info.get(), fm);
	}
	Notifier::Disable();

	return info;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::updatedFocalMechanism(FocalMechanism *focalMechanism, bool realFMUpdate) {
	// Get the cached focal mechanism, not the one sent in this message
	// If there is no cached origin the same instance is passed back
	FocalMechanism *fm = FocalMechanism::Find(focalMechanism->publicID());
	if ( fm ) {
		if ( focalMechanism != fm )
			*fm = *focalMechanism;
	}
	else
		fm = focalMechanism;

	EventInformationPtr info = findAssociatedEvent(fm);
	if ( !info ) {
		EventPtr e;
		if ( query() ) {
			SEISCOMP_DEBUG("... search for focal mechanisms's %s event in database", fm->publicID());
			e = getEventForFocalMechanism(fm->publicID());
		}

		if ( !e ) {
			SEISCOMP_DEBUG("... updated focal mechanism %s has not been associated yet, doing this",
			               fm->publicID());
			associateFocalMechanism(fm);
			return;
		}

		info = new EventInformation(&_cache, &_config, query(), e, author());
		info->loadAssocations(query());
		cacheEvent(info);
	}
	else {
		SEISCOMP_DEBUG("... found cached event information %s for focal mechanism", info->event->publicID());
	}

	if ( !fm->momentTensorCount() && query() ) {
		SEISCOMP_DEBUG("... loading moment tensor for focal mechanism %s", fm->publicID());
		query()->loadMomentTensors(fm);
	}

	// Cache this origin
	_cache.feed(fm);

	Notifier::Enable();
	if ( realFMUpdate && info->event->preferredFocalMechanismID() == fm->publicID() ) {
		info->event->setPreferredFocalMechanismID(string());
		info->event->setPreferredFocalMechanismID(string());
		info->preferredFocalMechanism = nullptr;
		updatePreferredFocalMechanism(info.get());
	}
	else {
		choosePreferred(info.get(), fm);
	}
	Notifier::Disable();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EventTool::MatchResult EventTool::compare(EventInformation *info,
                                          Seiscomp::DataModel::Origin *origin,
                                          const EventInformation::PickCache *cache) const {
	size_t matchingPicks = info->matchingPicks(query(), origin, cache);

	MatchResult result = Nothing;

	if ( matchingPicks >= _config.eventAssociation.minMatchingPicks ) {
		result = Picks;
	}

	if ( _config.eventAssociation.maxMatchingPicksTimeDiff >= 0 ) {
		SEISCOMP_DEBUG("... compare pick times with threshold %.2fs",
		               _config.eventAssociation.maxMatchingPicksTimeDiff);
	}
	SEISCOMP_DEBUG("... matching picks of %s and %s = %lu/%lu, need at least %lu",
	               origin->publicID(), info->event->publicID(),
	               (unsigned long)matchingPicks, (unsigned long)origin->arrivalCount(),
	               (unsigned long)_config.eventAssociation.minMatchingPicks);
	SEISCOMP_LOG(_infoChannel, "... matching picks of %s and %s = %lu/%lu, need at least %lu",
	             origin->publicID(), info->event->publicID(),
	             (unsigned long)matchingPicks, (unsigned long)origin->arrivalCount(),
	             (unsigned long)_config.eventAssociation.minMatchingPicks);

	if ( !info->preferredOrigin ) {
		return Nothing;
	}

	double dist, azi1, azi2;

	Math::Geo::delazi(origin->latitude().value(), origin->longitude().value(),
	                  info->preferredOrigin->latitude().value(),
	                  info->preferredOrigin->longitude().value(),
	                  &dist, &azi1, &azi2);

	// Dist out of range
	SEISCOMP_DEBUG("... distance of %s to %s = %f, max = %f",
	               origin->publicID(), info->event->publicID(),
	               dist, _config.eventAssociation.maxDist);
	SEISCOMP_LOG(_infoChannel, "... distance of %s to %s = %f, max = %f",
	             origin->publicID(), info->event->publicID(),
	             dist, _config.eventAssociation.maxDist);
	if ( dist <= _config.eventAssociation.maxDist ) {
		TimeSpan diffTime = info->preferredOrigin->time().value() - origin->time().value();

		SEISCOMP_DEBUG("... time diff of %s to %s = %.2fs, max = %.2f",
		               origin->publicID(), info->event->publicID(),
		               (double)diffTime, (double)_config.eventAssociation.maxTimeDiff);
		SEISCOMP_LOG(_infoChannel, "... time diff of %s to %s = %f, max = %f",
		             origin->publicID(), info->event->publicID(),
		             (double)diffTime, (double)_config.eventAssociation.maxTimeDiff);

		if ( diffTime.abs() <= _config.eventAssociation.maxTimeDiff ) {
			if ( result == Picks ) {
				result = PicksAndLocation;
			}
			else {
				result = Location;
			}
		}
	}

	switch ( result ) {
		case Nothing:
			SEISCOMP_DEBUG("... no match for %s and %s",
			               origin->publicID(), info->event->publicID());
			SEISCOMP_LOG(_infoChannel, "... no match for %s and %s",
			             origin->publicID(), info->event->publicID());
			break;
		case Location:
			SEISCOMP_DEBUG("... time/location matches for %s and %s",
			               origin->publicID(), info->event->publicID());
			SEISCOMP_LOG(_infoChannel, "... time/location matches for %s and %s",
			             origin->publicID(), info->event->publicID());
			break;
		case Picks:
			SEISCOMP_DEBUG("... matching picks for %s and %s",
			               origin->publicID(), info->event->publicID());
			SEISCOMP_LOG(_infoChannel, "... matching picks for %s and %s",
			             origin->publicID(), info->event->publicID());
			break;
		case PicksAndLocation:
			SEISCOMP_DEBUG("... matching picks and time/location for %s and %s",
			               origin->publicID(), info->event->publicID());
			SEISCOMP_LOG(_infoChannel, "... matching picks and time/location for %s and %s",
			             origin->publicID(), info->event->publicID());
			break;
	}

	return result;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EventInformationPtr EventTool::createEvent(Origin *origin) {
	string eventID = allocateEventID(query(), origin, _config);

	if ( eventID.empty() ) {
		SEISCOMP_ERROR("Unable to allocate a new eventID, skipping origin %s\n"
		               "Hint: increase event slots with eventIDPattern parameter",
		               origin->publicID());
		SEISCOMP_LOG(_infoChannel, "Event created failed: unable to allocate a new EventID");
		return nullptr;
	}
	else {
		if ( Event::Find(eventID) ) {
			SEISCOMP_ERROR("Unable to allocate a new eventID, skipping origin %s\n"
			               "Hint: increase event slots with eventIDPattern parameter",
			               origin->publicID());
			SEISCOMP_LOG(_infoChannel, "Event created failed: unable to allocate a new EventID");
			return nullptr;
		}

		Time now = Time::UTC();
		logObject(_outputEvent, now);

		Notifier::Enable();

		EventInformationPtr info = new EventInformation(&_cache, &_config);
		info->event = new Event(eventID);

		CreationInfo ci;
		ci.setAgencyID(agencyID());
		ci.setAuthor(author());
		ci.setCreationTime(now);

		info->event->setCreationInfo(ci);
		info->created = true;
		info->dirtyPickSet = true;

		cacheEvent(info);

		Notifier::Disable();

		return info;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EventInformationPtr EventTool::findMatchingEvent(
	Origin *origin,
	const EventInformation::PickCache *cache
) const {
	MatchResult bestResult = Nothing;
	EventInformationPtr bestInfo = nullptr;

	for ( auto &[id, info] : _events ) {
		if ( info->event && isAgencyIDBlocked(objectAgencyID(info->event.get())) ) {
			continue;
		}

		MatchResult res = compare(info.get(), origin, cache);
		if ( res > bestResult ) {
			bestResult = res;
			bestInfo = info;
		}
	}

	if ( bestInfo ) {
		SEISCOMP_DEBUG("... found best matching cached event %s (code: %d)",
		               bestInfo->event->publicID(),
		               static_cast<int>(bestResult));
		SEISCOMP_LOG(_infoChannel, "... found best matching cached event %s (code: %d)",
		             bestInfo->event->publicID(),
		             static_cast<int>(bestResult));
	}

	return bestInfo;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EventInformationPtr EventTool::findAssociatedEvent(DataModel::Origin *origin) {
	for ( auto &[id, info] : _events ) {
		if ( info->event->originReference(origin->publicID()) ) {
			SEISCOMP_DEBUG("... feeding cache with event %s",
			               info->event->publicID());
			refreshEventCache(info);
			return info;
		}
	}

	return nullptr;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EventInformationPtr EventTool::findAssociatedEvent(DataModel::FocalMechanism *fm) {
	for ( auto &[id, info] : _events ) {
		if ( info->event->focalMechanismReference(fm->publicID()) ) {
			SEISCOMP_DEBUG("... feeding cache with event %s",
			               info->event->publicID());
			refreshEventCache(info);
			return info;
		}
	}

	return nullptr;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Magnitude *EventTool::preferredMagnitude(Origin *origin) {
	for ( EventProcessorPtr &proc : _processors ) {
		auto mag = proc->preferredMagnitude(origin);
		if ( mag ) {
			return mag;
		}
	}

	int magMwCount = 0;
	int magMwPriority = 0;
	int magFallbackCount = 0;
	int magFallbackPriority = 0;
	Magnitude *magMwInstance = nullptr;
	Magnitude *magFallbackInstance = nullptr;

	int mbcount = 0;
	double mbval = 0.0;
	for ( size_t i = 0; i < origin->magnitudeCount(); ++i ) {
		Magnitude *mag = origin->magnitude(i);
		if ( isAgencyIDBlocked(objectAgencyID(mag)) || isRejected(mag) ) {
			continue;
		}

		if ( mag->type() == "mb" ) {
			if ( mag->magnitude().value() > mbval ) {
				mbval = mag->magnitude().value();
				mbcount = stationCount(mag);
			}
		}
	}

	for ( size_t i = 0; i < origin->magnitudeCount(); ++i ) {
		try {
			Magnitude *mag = origin->magnitude(i);
			if ( isAgencyIDBlocked(objectAgencyID(mag)) ) {
				SEISCOMP_DEBUG("...... ignoring %s magnitude %s: agency '%s' is blocked",
				               mag->type(), mag->publicID(),
				               mag->creationInfo().agencyID());
				continue;
			}

			if ( isRejected(mag) ) {
				SEISCOMP_DEBUG("...... ignoring %s magnitude %s: evaluation status is 'rejected'",
				               mag->type(), mag->publicID());
				continue;
			}

			int priority = goodness(mag, mbcount, mbval, _config);
			if ( priority <= 0 ) {
				continue;
			}

			if ( _config.eventAssociation.magPriorityOverStationCount ) {
				// Priority rules out the station count. A network
				// magnitude with lower station count can become preferred if
				// its priority is higher.
				// Station count is only taken into account if the priority is
				// equal.
				if ( isMw(mag) ) {
					if ( (priority > magMwPriority)
					  || ((priority == magMwPriority) && (stationCount(mag) > magMwCount)) ) {
						magMwPriority = priority;
						magMwCount = stationCount(mag);
						magMwInstance = mag;
					}
				}
				else {
					// No Mw
					if ( (priority > magFallbackPriority)
					  || ((priority == magFallbackPriority) && (stationCount(mag) > magFallbackCount)) ) {
						magFallbackPriority = priority;
						magFallbackCount = stationCount(mag);
						magFallbackInstance = mag;
					}
				}
			}
			else {
				// Station count rules out priority. A network
				// magnitude with lower priority can become preferred if
				// its station count is higher.
				// Priority is only taken into account when the station count
				// is equal.
				if ( isMw(mag) ) {
					if ( (stationCount(mag) > magMwCount)
					  || ((stationCount(mag) == magMwCount) && (priority > magMwPriority)) ) {
						magMwPriority = priority;
						magMwCount = stationCount(mag);
						magMwInstance = mag;
					}
				}
				else {
					// No Mw
					if ( (stationCount(mag) > magFallbackCount)
					  || ((stationCount(mag) == magFallbackCount) && (priority > magFallbackPriority)) ) {
						magFallbackPriority = priority;
						magFallbackCount = stationCount(mag);
						magFallbackInstance = mag;
					}
				}
			}
		}
		catch ( ValueException& ) {
			continue;
		}
	}

	// Prefer Mw magnitudes over "normal" magnitudes
	if ( magMwInstance ) {
		return magMwInstance;
	}

	if ( !magFallbackInstance && _config.eventAssociation.enableFallbackPreferredMagnitude ) {
		// Find the network magnitude with the most station magnitudes according the
		// magnitude priority and set this magnitude preferred
		magFallbackCount = 0;
		magFallbackPriority = 0;

		for ( size_t i = 0; i < origin->magnitudeCount(); ++i ) {
			Magnitude *mag = origin->magnitude(i);
			if ( isAgencyIDBlocked(objectAgencyID(mag)) || isRejected(mag) ) {
				continue;
			}

			int prio = magnitudePriority(mag->type(), _config);
			if ( (stationCount(mag) > magFallbackCount)
			  || (stationCount(mag) == magFallbackCount && prio > magFallbackPriority ) ) {
				magFallbackPriority = prio;
				magFallbackCount = stationCount(mag);
				magFallbackInstance = mag;
			}
		}
	}

	return magFallbackInstance;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Event *EventTool::getEventForOrigin(const std::string &originID) {
	for ( auto it = _events.begin(); it != _events.end(); ++it ) {
		Event *evt = it->second->event.get();
		if ( !evt ) continue;
		if ( evt->originReference(originID) ) return evt;
	}

	return Event::Cast(query()->getEvent(originID));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Event *EventTool::getEventForFocalMechanism(const std::string &fmID) {
	for ( auto it = _events.begin(); it != _events.end(); ++it ) {
		Event *evt = it->second->event.get();
		if ( !evt ) continue;
		if ( evt->focalMechanismReference(fmID) ) return evt;
	}

	return Event::Cast(query()->getEventForFocalMechanism(fmID));
}

// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::cacheEvent(EventInformationPtr info) {
	SEISCOMP_DEBUG("... caching event %s", info->event->publicID());

	// Cache the complete event information
	_events[info->event->publicID()] = info;
	refreshEventCache(info);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::refreshEventCache(EventInformationPtr info) {
	if ( info->aboutToBeRemoved ) {
		// Set the clean-up flag to false
		info->aboutToBeRemoved = false;
		SEISCOMP_DEBUG("... clear cache removal flag for event %s", info->event->publicID());
	}

	// Add the event to the EventParameters
	if ( !info->event->eventParameters() ) {
		_ep->add(info->event.get());
	}

	// Feed event into cache
	_cache.feed(info->event.get());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool EventTool::isEventCached(const string &eventID) const {
	return _events.find(eventID) != _events.end();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EventInformationPtr EventTool::cachedEvent(const std::string &eventID) {
	if ( auto it = _events.find(eventID); it != _events.end() ) {
		refreshEventCache(it->second);
		return it->second;
	}

	return nullptr;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool EventTool::removeCachedEvent(const std::string &eventID) {
	if ( auto it = _events.find(eventID); it != _events.end() ) {
		_events.erase(it);
		return true;
	}

	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::choosePreferred(EventInformation *info, Origin *origin,
                                DataModel::Magnitude *triggeredMag,
                                bool realOriginUpdate, bool refresh) {
	if ( isAgencyIDBlocked(objectAgencyID(info->event.get())) ) {
		SEISCOMP_DEBUG("No preferred origin selection, event %s agencyID is blocked",
		               info->event->publicID());
		SEISCOMP_LOG(_infoChannel, "No preferred origin selection, event %s agencyID is blocked",
		             info->event->publicID());
		return;
	}

	Magnitude *mag = nullptr;
	MagnitudePtr momentMag;

	SEISCOMP_DEBUG("%s: testing origin/magnitude(%s)",
	               info->event->publicID(), origin->publicID());

	// Is originID blacklisted, e.g. a derived origin of a moment tensor
	if ( _originBlackList.find(origin->publicID()) != _originBlackList.end() ) {
		SEISCOMP_DEBUG("%s: skip setting preferredOriginID, origin %s is blacklisted",
		               info->event->publicID(), origin->publicID());
		SEISCOMP_LOG(_infoChannel, "Origin %s cannot be set preferred: blacklisted",
		             origin->publicID());
		return;
	}

	if ( info->constraints.fixMw && info->preferredFocalMechanism ) {
		for ( size_t i = 0; i < info->preferredFocalMechanism->momentTensorCount(); ++i ) {
			MomentTensor *mt = info->preferredFocalMechanism->momentTensor(i);
			momentMag = _cache.get<Magnitude>(mt->momentMagnitudeID());
			if ( momentMag == nullptr ) {
				SEISCOMP_WARNING("Moment magnitude with id '%s' not found",
				                 mt->momentMagnitudeID());
				continue;
			}

			if ( isRejected(momentMag.get()) ) {
				SEISCOMP_LOG(_infoChannel,
				             "Moment magnitude with id '%s' of status REJECTED",
				             mt->momentMagnitudeID());
				continue;
			}

			mag = momentMag.get();
			SEISCOMP_DEBUG("... found preferred Mw %s", mag->publicID());

			// Just take the first moment tensor object
			break;
		}
	}

	// Special magnitude type requested?
	if ( !mag && !info->constraints.preferredMagnitudeType.empty() ) {
		SEISCOMP_DEBUG("... requested preferred magnitude type is %s",
		               info->constraints.preferredMagnitudeType);
		for ( size_t i = 0; i < origin->magnitudeCount(); ++i ) {
			Magnitude *nm = origin->magnitude(i);
			if ( nm->type() == info->constraints.preferredMagnitudeType ) {
				if ( isRejected(nm) ) {
					SEISCOMP_LOG(_infoChannel,
					             "Found magnitude '%s' with requested type but "
					             "of status REJECTED",
					             nm->publicID());
					continue;
				}
				SEISCOMP_DEBUG("... found magnitude %s with requested type",
				               nm->publicID());
				mag = nm;
				break;
			}
		}
	}

	// No magnitude found -> go on using the normal procedure
	if ( !mag ) {
		SEISCOMP_DEBUG("... looking for preferred magnitude in origin %s",
		               origin->publicID());
		mag = preferredMagnitude(origin);
		if ( !mag ) {
			SEISCOMP_DEBUG("...... found none");
		}
		else {
			SEISCOMP_DEBUG("...... preferring %s with public ID %s",
			               mag->type(), mag->publicID());
		}
	}

	bool update = refresh;
	bool needRegionNameUpdate = refresh;

	if ( !info->preferredOrigin ) {
		if ( isAgencyIDAllowed(objectAgencyID(origin)) || info->constraints.fixOrigin(origin) ) {
			info->event->setPreferredOriginID(origin->publicID());

			SEISCOMP_INFO("%s: set first preferredOriginID to %s",
			              info->event->publicID(), origin->publicID());
			SEISCOMP_LOG(_infoChannel, "Origin %s has been set preferred in event %s",
			             origin->publicID(), info->event->publicID());

			needRegionNameUpdate = true;

			info->preferredOrigin = origin;
			update = true;
		}
		else {
			if ( !isAgencyIDAllowed(objectAgencyID(origin)) ) {
				SEISCOMP_DEBUG("%s: skip setting first preferredOriginID, agencyID '%s' is blocked",
				               info->event->publicID(), objectAgencyID(origin));
				SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred: agencyID %s is blocked",
				             origin->publicID(), objectAgencyID(origin));
			}
			else {
				SEISCOMP_DEBUG("%s: skip setting first preferredOriginID, preferredOriginID is fixed to '%s'",
				               info->event->publicID(), info->constraints.preferredOriginID);
				SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred: preferredOriginID is fixed to %s",
				             origin->publicID(), info->constraints.preferredOriginID);
			}
			return;
		}

		if ( mag ) {
			info->event->setPreferredMagnitudeID(mag->publicID());
			info->preferredMagnitude = mag;
			SEISCOMP_INFO("%s: set first preferredMagnitudeID to %s",
			              info->event->publicID(), mag->publicID());
			SEISCOMP_LOG(_infoChannel, "Magnitude %s has been set preferred in event %s",
			             mag->publicID(), info->event->publicID());
			update = true;
		}
	}
	else if ( info->preferredOrigin->publicID() != origin->publicID() ) {
		SEISCOMP_DEBUG("... checking whether origin %s can become preferred",
		               origin->publicID());

		update = true;

		// Fixed origin => check if the passed origin is the fixed one
		if ( info->constraints.fixedOrigin() ) {
			if ( !info->constraints.fixOrigin(origin) ) {
				SEISCOMP_DEBUG("... skipping potential preferred origin, origin '%s' is fixed",
				               info->constraints.preferredOriginID);
				SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: origin %s is fixed",
				             origin->publicID(), info->event->publicID(),
				             info->constraints.preferredOriginID);
				return;
			}
			else {
				SEISCOMP_LOG(_infoChannel, "Origin %s is fixed as preferred origin",
				             origin->publicID());
				SEISCOMP_DEBUG("... incoming origin is fixed");
			}
		}
		// No fixed origin => select it using the automatic rules
		else {

			if ( isAgencyIDBlocked(objectAgencyID(origin)) ) {
				SEISCOMP_DEBUG("... skipping potential preferred origin, agencyID '%s' is blocked",
				               objectAgencyID(origin));
				SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: agencyID %s is blocked",
				             origin->publicID(), info->event->publicID(),
				             objectAgencyID(origin));
				return;
			}

			if ( !_config.eventAssociation.priorities.empty() ) {
				bool allowBadMagnitude = false;

				// Run through the priority list and check the values
				for ( auto &check : _config.eventAssociation.priorities ) {
					if ( check == "AGENCY" ) {
						int originAgencyPriority = agencyPriority(objectAgencyID(origin), _config);
						int preferredOriginAgencyPriority = agencyPriority(objectAgencyID(info->preferredOrigin.get()), _config);

						if ( originAgencyPriority < preferredOriginAgencyPriority ) {
							SEISCOMP_DEBUG("... skipping potential preferred origin, priority of agencyID '%s' is too low",
							               objectAgencyID(origin));
							SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: priority of agencyID %s is too low",
							             origin->publicID(), info->event->publicID(),
							             objectAgencyID(origin));
							return;
						}
						// Found origin with higher agency priority
						else if ( originAgencyPriority > preferredOriginAgencyPriority ) {
							SEISCOMP_DEBUG("... agencyID '%s' overrides current agencyID '%s'",
							               objectAgencyID(origin), objectAgencyID(info->preferredOrigin.get()));
							SEISCOMP_LOG(_infoChannel, "Origin %s: agencyID '%s' overrides agencyID '%s'",
							             origin->publicID(), objectAgencyID(origin), objectAgencyID(info->preferredOrigin.get()));

							allowBadMagnitude = true;
							break;
						}
					}
					else if ( check == "AUTHOR" ) {
						int originAuthorPriority = authorPriority(objectAuthor(origin), _config);
						int preferredOriginAuthorPriority = authorPriority(objectAuthor(info->preferredOrigin.get()), _config);

						if ( originAuthorPriority < preferredOriginAuthorPriority ) {
							SEISCOMP_DEBUG("... skipping potential preferred origin, priority of author '%s' is too low",
							               objectAuthor(origin));
							SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: priority of author %s is too low",
							             origin->publicID(), info->event->publicID(),
							             objectAuthor(origin));
							return;
						}
						// Found origin with higher author priority
						else if ( originAuthorPriority > preferredOriginAuthorPriority ) {
							SEISCOMP_DEBUG("... author '%s' overrides current author '%s'",
							               objectAuthor(origin), objectAuthor(info->preferredOrigin.get()));
							SEISCOMP_LOG(_infoChannel, "Origin %s: author '%s' overrides author '%s'",
							             origin->publicID(), objectAuthor(origin), objectAuthor(info->preferredOrigin.get()));
							break;
						}
					}
					else if ( check == "MODE" ) {
						int originPriority = modePriority(origin);
						int preferredOriginPriority = modePriority(info->preferredOrigin.get());

						if ( info->constraints.fixOriginMode(info->preferredOrigin.get()) ) {
							SEISCOMP_LOG(_infoChannel, "Origin %s: has priority %d vs %d",
							             origin->publicID(), originPriority, preferredOriginPriority);

							// Set back the evalmode to automatic if a higher priority
							// origin has been send (but not triggered by a magnitude change only)
							if ( realOriginUpdate && (originPriority > preferredOriginPriority) ) {
								SEISCOMP_LOG(_infoChannel, "Origin %s has higher priority: releasing EvPrefOrgEvalMode",
								             origin->publicID());
								JournalEntryPtr entry = new JournalEntry;
								entry->setObjectID(info->event->publicID());
								entry->setAction("EvPrefOrgEvalMode");
								entry->setParameters("");
								entry->setSender(author());
								entry->setCreated(Core::Time::UTC());
								Notifier::Create(_journal->publicID(), OP_ADD, entry.get());
								info->addJournalEntry(entry.get(), author());
							}
							else
								preferredOriginPriority = ORIGIN_PRIORITY_MAX;
						}

						if ( info->constraints.fixOriginMode(origin) )
							originPriority = ORIGIN_PRIORITY_MAX;

						if ( originPriority == 0 ) {
							SEISCOMP_DEBUG("...... origin is neither manual nor automatic");
						}

						if ( originPriority < preferredOriginPriority ) {
							SEISCOMP_DEBUG("... skipping potential preferred origin, "
							               "priority based on evaluation mode < "
							               "currently preferred (%d < %d)",
							               originPriority,
							               preferredOriginPriority);
							SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: mode priority too low (%d < %d)",
							             origin->publicID(), info->event->publicID(),
							             originPriority, preferredOriginPriority);
							return;
						}
						// Found origin with higher status priority
						else if ( originPriority > preferredOriginPriority ) {
							SEISCOMP_LOG(_infoChannel,
							             "Origin %s has higher mode priority "
							             "(%d) than currently preferred origin (%d)",
							             origin->publicID(), originPriority,
							             preferredOriginPriority);
							break;
						}
					}
					else if ( check == "STATUS" ) {
						int originPriority = priority(origin);
						int preferredOriginPriority = priority(info->preferredOrigin.get());

						if ( info->constraints.fixOriginMode(info->preferredOrigin.get()) ) {
							SEISCOMP_LOG(_infoChannel, "Origin %s: has priority %d vs %d",
							             origin->publicID(), originPriority, preferredOriginPriority);

							// Set back the evalmode to automatic if a higher priority
							// origin has been send (but not triggered by a magnitude change only)
							if ( realOriginUpdate && (originPriority > preferredOriginPriority) ) {
								SEISCOMP_LOG(_infoChannel, "Origin %s has higher priority: releasing EvPrefOrgEvalMode",
								             origin->publicID());
								JournalEntryPtr entry = new JournalEntry;
								entry->setObjectID(info->event->publicID());
								entry->setAction("EvPrefOrgEvalMode");
								entry->setParameters("");
								entry->setSender(author());
								entry->setCreated(Core::Time::UTC());
								Notifier::Create(_journal->publicID(), OP_ADD, entry.get());
								info->addJournalEntry(entry.get(), author());
							}
							else
								preferredOriginPriority = ORIGIN_PRIORITY_MAX;
						}

						if ( info->constraints.fixOriginMode(origin) )
							originPriority = ORIGIN_PRIORITY_MAX;

						if ( originPriority == 0 ) {
							SEISCOMP_DEBUG("...... origin is neither manual nor automatic");
						}

						if ( originPriority < preferredOriginPriority ) {
							SEISCOMP_DEBUG("... skipping potential preferred origin, "
							               "priority based on evaluation mode < "
							               "currently preferred (%d < %d)",
							               originPriority,
							               preferredOriginPriority);
							SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: status priority too low (%d < %d)",
							             origin->publicID(), info->event->publicID(),
							             originPriority, preferredOriginPriority);
							return;
						}
						// Found origin with higher status priority
						else if ( originPriority > preferredOriginPriority ) {
							SEISCOMP_LOG(_infoChannel, "Origin %s: status priority %d overrides status priority %d",
							             origin->publicID(), originPriority, preferredOriginPriority);
							break;
						}
					}
					else if ( check == "METHOD" ) {
						int originMethodPriority = methodPriority(origin->methodID(), _config);
						int preferredOriginMethodPriority = methodPriority(info->preferredOrigin->methodID(), _config);

						if ( originMethodPriority < preferredOriginMethodPriority ) {
							SEISCOMP_DEBUG("... skipping potential preferred origin, priority of method '%s' is too low",
							               origin->methodID());
							SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: priority of method %s is too low",
							             origin->publicID(), info->event->publicID(),
							             origin->methodID());
							return;
						}
						// Found origin with higher method priority
						else if ( originMethodPriority > preferredOriginMethodPriority ) {
							SEISCOMP_DEBUG("... methodID '%s' overrides current methodID '%s'",
							               origin->methodID(), info->preferredOrigin->methodID());
							SEISCOMP_LOG(_infoChannel, "Origin %s: methodID '%s' overrides methodID '%s'",
							             origin->publicID(), origin->methodID(),
							             info->preferredOrigin->methodID());
							break;
						}
					}
					else if ( check == "PHASES" ) {
						int originPhaseCount = definingPhaseCount(origin);
						int preferredOriginPhaseCount = definingPhaseCount(info->preferredOrigin.get());
						if ( originPhaseCount < preferredOriginPhaseCount ) {
							SEISCOMP_DEBUG("... skipping potential preferred automatic origin, phaseCount too low");
							SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: phaseCount too low (%d < %d)",
							             origin->publicID(), info->event->publicID(),
							             originPhaseCount, preferredOriginPhaseCount);
							return;
						}
						else if ( originPhaseCount > preferredOriginPhaseCount ) {
							SEISCOMP_LOG(_infoChannel, "Origin %s: phaseCount %d overrides phaseCount %d",
							             origin->publicID(), originPhaseCount,
							             preferredOriginPhaseCount);
							break;
						}
					}
					else if ( check == "PHASES_AUTOMATIC" ) {
						EvaluationMode status = AUTOMATIC;
						try {
							status = origin->evaluationMode();
						}
						catch ( ValueException& ) {}

						// Make a noop in case of non automatic origins
						if ( status != AUTOMATIC ) continue;

						int originPhaseCount = definingPhaseCount(origin);
						int preferredOriginPhaseCount = definingPhaseCount(info->preferredOrigin.get());
						if ( originPhaseCount < preferredOriginPhaseCount ) {
							SEISCOMP_DEBUG("... skipping potential preferred automatic origin, phaseCount too low");
							SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: phaseCount too low (%d < %d)",
							             origin->publicID(), info->event->publicID(),
							             originPhaseCount, preferredOriginPhaseCount);
							return;
						}
						else if ( originPhaseCount > preferredOriginPhaseCount ) {
							SEISCOMP_LOG(_infoChannel, "Origin %s: phaseCount (automatic) %d overrides phaseCount %d",
							             origin->publicID(), originPhaseCount,
							             preferredOriginPhaseCount);
							break;
						}
					}
					else if ( check == "RMS" ) {
						double originRMS = rms(origin);
						double preferredOriginRMS = rms(info->preferredOrigin.get());

						// Both RMS attributes are set: check priority
						if ( (originRMS >= 0) && (preferredOriginRMS >= 0) && (originRMS > preferredOriginRMS) ) {
							SEISCOMP_DEBUG("... skipping potential preferred automatic origin, RMS too high");
							SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: RMS too high (%.1f > %.1f",
							             origin->publicID(), info->event->publicID(),
							             originRMS, preferredOriginRMS);
							return;
						}
						// Both RMS attribute are set: check priority
						else if ( (originRMS >= 0) && (preferredOriginRMS >= 0) && (originRMS < preferredOriginRMS) ) {
							SEISCOMP_LOG(_infoChannel, "Origin %s: RMS %.1f overrides RMS %.1f",
							             origin->publicID(), originRMS,
							             preferredOriginRMS);
							break;
						}
						// Incoming RMS is not set but preferred origin has RMS: skip incoming
						else if ( (originRMS < 0) && (preferredOriginRMS >= 0) ) {
							SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: RMS not set",
							             origin->publicID(), info->event->publicID());
							return;
						}
						// Incoming RMS is set but preferred origin has no RMS: prioritize incoming
						else if ( (originRMS >= 0) && (preferredOriginRMS < 0) ) {
							SEISCOMP_LOG(_infoChannel, "Origin %s: RMS %.1f overrides current unset preferred RMS",
							             origin->publicID(), originRMS);
							break;
						}
					}
					else if ( check == "RMS_AUTOMATIC" ) {
						EvaluationMode status = AUTOMATIC;
						try {
							status = origin->evaluationMode();
						}
						catch ( ValueException& ) {}

						// Make a noop in case of non automatic origins
						if ( status != AUTOMATIC ) continue;

						double originRMS = rms(origin);
						double preferredOriginRMS = rms(info->preferredOrigin.get());

						// Both RMS attributes are set: check priority
						if ( (originRMS >= 0) && (preferredOriginRMS >= 0) && (originRMS > preferredOriginRMS) ) {
							SEISCOMP_DEBUG("... skipping potential preferred automatic origin, RMS too high");
							SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: RMS too high (%.1f > %.1f",
							             origin->publicID(), info->event->publicID(),
							             originRMS, preferredOriginRMS);
							return;
						}
						// Both RMS attribute are set: check priority
						else if ( (originRMS >= 0) && (preferredOriginRMS >= 0) && (originRMS < preferredOriginRMS) ) {
							SEISCOMP_LOG(_infoChannel, "Origin %s: RMS %.1f overrides RMS %.1f",
							             origin->publicID(), originRMS,
							             preferredOriginRMS);
							break;
						}
						// Incoming RMS is not set but preferred origin has RMS: skip incoming
						else if ( (originRMS < 0) && (preferredOriginRMS >= 0) ) {
							SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: RMS not set",
							             origin->publicID(), info->event->publicID());
							return;
						}
						// Incoming RMS is set but preferred origin has no RMS: prioritize incoming
						else if ( (originRMS >= 0) && (preferredOriginRMS < 0) ) {
							SEISCOMP_LOG(_infoChannel, "Origin %s: RMS %.1f overrides current unset preferred RMS",
							             origin->publicID(), originRMS);
							break;
						}
					}
					else if ( check == "TIME" ) {
						Core::Time originCreationTime = created(origin);
						Core::Time preferredOriginCreationTime = created(info->preferredOrigin.get());
						if ( originCreationTime < preferredOriginCreationTime ) {
							SEISCOMP_DEBUG("... skipping potential preferred origin, there is a better one created later");
							return;
						}
						else if ( originCreationTime > preferredOriginCreationTime ) {
							SEISCOMP_LOG(_infoChannel, "Origin %s: %s is more recent than %s",
							             origin->publicID(), originCreationTime.iso(),
							             preferredOriginCreationTime.iso());
							break;
						}
					}
					else if ( check == "TIME_AUTOMATIC" ) {
						EvaluationMode status = AUTOMATIC;
						try {
							status = origin->evaluationMode();
						}
						catch ( ValueException& ) {}

						// Make a noop in case of non automatic origins
						if ( status != AUTOMATIC ) continue;

						Core::Time originCreationTime = created(origin);
						Core::Time preferredOriginCreationTime = created(info->preferredOrigin.get());
						if ( originCreationTime < preferredOriginCreationTime ) {
							SEISCOMP_DEBUG("... skipping potential preferred origin, there is a better one created later");
							return;
						}
						else if ( originCreationTime > preferredOriginCreationTime ) {
							SEISCOMP_LOG(_infoChannel, "Origin %s: %s (automatic) is more recent than %s",
							             origin->publicID(), originCreationTime.iso(),
							             preferredOriginCreationTime.iso());
							break;
						}
					}
					else if ( check == "SCORE" ) {
						double score = _score->evaluate(origin);
						double preferredScore = _score->evaluate(info->preferredOrigin.get());
						if ( score < preferredScore ) {
							SEISCOMP_DEBUG("... skipping potential preferred origin, there is one with higher score: %f > %f",
							               preferredScore, score);
							return;
						}
						else if ( score > preferredScore ) {
							SEISCOMP_LOG(_infoChannel, "Origin %s: score of %f is larger than %f",
							             origin->publicID(), score,
							             preferredScore);
							break;
						}
					}
				}

				// Agency priority is a special case and an origin can become preferred without
				// a preferred magnitude if the agencyID has higher priority
				if ( !allowBadMagnitude ) {
					// The current origin has no preferred magnitude yet but
					// the event already has => ignore the origin for now
					if ( info->preferredMagnitude && !mag ) {
						SEISCOMP_DEBUG("... skipping potential preferred origin, no preferred magnitude");
						SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: no preferrable magnitude",
						             origin->publicID(), info->event->publicID());
						return;
					}
				}
			}
			else {
				int originAgencyPriority = agencyPriority(objectAgencyID(origin), _config);
				int preferredOriginAgencyPriority = agencyPriority(objectAgencyID(info->preferredOrigin.get()), _config);

				if ( originAgencyPriority < preferredOriginAgencyPriority ) {
					SEISCOMP_DEBUG("... skipping potential preferred origin, priority of agencyID '%s' is too low",
					               objectAgencyID(origin));
					SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: priority of agencyID %s is too low",
					             origin->publicID(), info->event->publicID(),
					             objectAgencyID(origin));
					return;
				}

				// Same agency priorities -> compare origin priority
				if ( originAgencyPriority == preferredOriginAgencyPriority ) {
					int originPriority = priority(origin);
					int preferredOriginPriority = priority(info->preferredOrigin.get());

					if ( info->constraints.fixOriginMode(info->preferredOrigin.get()) ) {
						SEISCOMP_LOG(_infoChannel, "Origin %s: has evaluation mode/status priority %d vs %d",
						             origin->publicID(), originPriority, preferredOriginPriority);

						// Set back the evalmode to automatic if a higher priority
						// origin has been send (but not triggered by a magnitude change only)
						if ( realOriginUpdate && (originPriority > preferredOriginPriority) ) {
							SEISCOMP_LOG(_infoChannel, "Origin %s has higher evaluation mode/status priority: releasing EvPrefOrgEvalMode",
							             origin->publicID());
							JournalEntryPtr entry = new JournalEntry;
							entry->setObjectID(info->event->publicID());
							entry->setAction("EvPrefOrgEvalMode");
							entry->setParameters("");
							entry->setSender(author());
							entry->setCreated(Core::Time::UTC());
							Notifier::Create(_journal->publicID(), OP_ADD, entry.get());
							info->addJournalEntry(entry.get(), author());
						}
						else
							preferredOriginPriority = ORIGIN_PRIORITY_MAX;
					}

					if ( info->constraints.fixOriginMode(origin) )
						originPriority = ORIGIN_PRIORITY_MAX;

					if ( originPriority == 0 ) {
						SEISCOMP_DEBUG("...... origin is neither manual nor automatic");
					}

					if ( originPriority < preferredOriginPriority ) {
						SEISCOMP_DEBUG("... skipping potential preferred origin, "
						               "priority based on evaluation mode < "
						               "currently preferred (%d < %d)",
						               originPriority,
						               preferredOriginPriority);
						SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: evaluation mode/status priority too low (%d < %d)",
						             origin->publicID(), info->event->publicID(),
						             originPriority, preferredOriginPriority);
						return;
					}

					// The current origin has no preferred magnitude yet but
					// the event already has => ignore the origin for now
					if ( info->preferredMagnitude && !mag ) {
						SEISCOMP_DEBUG("... skipping potential preferred origin, no preferred magnitude");
						SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: no preferrable magnitude",
						             origin->publicID(), info->event->publicID());
						return;
					}

					if ( originPriority == preferredOriginPriority ) {
						EvaluationMode status = AUTOMATIC;
						try {
							status = origin->evaluationMode();
						}
						catch ( ValueException& ) {}

						if ( status == AUTOMATIC ) {
							SEISCOMP_DEBUG("... same priority and mode is AUTOMATIC");

							originPriority = definingPhaseCount(origin);
							preferredOriginPriority = definingPhaseCount(info->preferredOrigin.get());
							if ( originPriority < preferredOriginPriority ) {
								SEISCOMP_DEBUG("... skipping potential preferred automatic origin, phaseCount too low");
								SEISCOMP_LOG(_infoChannel, "Origin %s has not been set preferred in event %s: priorities are equal (%d) but phaseCount too low (%d < %d)",
								             origin->publicID(), info->event->publicID(), originPriority,
								             definingPhaseCount(origin), definingPhaseCount(info->preferredOrigin.get()));
								return;
							}

							/*
							if ( created(origin) < created(info->preferredOrigin.get()) ) {
								SEISCOMP_DEBUG("... skipping potential preferred origin, there is a better one created later");
								return;
							}
							*/
						}

						if ( originPriority == preferredOriginPriority ) {
							if ( created(origin) < created(info->preferredOrigin.get()) ) {
								SEISCOMP_DEBUG("... skipping potential preferred origin, there is a better one created later");
								SEISCOMP_LOG(_infoChannel, "Origin %s: skipped potential preferred origin, there is a better one created later",
								             origin->publicID());
								return;
							}
						}
					}
					else {
						SEISCOMP_DEBUG("... priority %d overrides current prioriy %d",
						               originPriority, preferredOriginPriority);
						SEISCOMP_LOG(_infoChannel, "Origin %s: priority %d overrides priority %d",
						             origin->publicID(), originPriority, preferredOriginPriority);
					}
				}
				else {
					SEISCOMP_DEBUG("... agencyID '%s' overrides current agencyID '%s'",
					               objectAgencyID(origin), objectAgencyID(info->preferredOrigin.get()));
					SEISCOMP_LOG(_infoChannel, "Origin %s: agencyID '%s' overrides agencyID '%s'",
					             origin->publicID(), objectAgencyID(origin), objectAgencyID(info->preferredOrigin.get()));
				}
			}
		}

		info->event->setPreferredOriginID(origin->publicID());

		SEISCOMP_INFO("%s: set preferredOriginID to %s",
		              info->event->publicID(), origin->publicID());
		SEISCOMP_LOG(_infoChannel, "Origin %s has been set preferred in event %s",
		             origin->publicID(), info->event->publicID());

		needRegionNameUpdate = true;

		info->preferredOrigin = origin;

		if ( mag ) {
			if ( info->event->preferredMagnitudeID() != mag->publicID() ) {
				update = true;

				info->event->setPreferredMagnitudeID(mag->publicID());
				info->preferredMagnitude = mag;
				SEISCOMP_INFO("%s: set preferredMagnitudeID to %s",
				              info->event->publicID(), mag->publicID());
				SEISCOMP_LOG(_infoChannel, "Magnitude %s has been set preferred in event %s",
				             mag->publicID(), info->event->publicID());
			}
		}
		else {
			if ( !info->event->preferredMagnitudeID().empty() ) {
				update = true;

				info->event->setPreferredMagnitudeID("");
				info->preferredMagnitude = mag;
				SEISCOMP_INFO("%s: set preferredMagnitudeID to ''",
				              info->event->publicID());
				SEISCOMP_LOG(_infoChannel, "Set empty preferredMagnitudeID in event %s",
				             info->event->publicID());
			}
		}
	}
	else if ( mag && info->event->preferredMagnitudeID() != mag->publicID() ) {
		info->event->setPreferredMagnitudeID(mag->publicID());
		info->preferredMagnitude = mag;
		SEISCOMP_INFO("%s: set preferredMagnitudeID to %s",
		              info->event->publicID(), mag->publicID());
		SEISCOMP_INFO("%s: set preferredMagnitudeID to %s",
		              info->event->publicID(), mag->publicID());
		update = true;
	}
	else {
		SEISCOMP_INFO("%s: nothing to do", info->event->publicID());
	}

	if ( needRegionNameUpdate ) {
		updateRegionName(info->event.get(), info->preferredOrigin.get());
	}

	bool callProcessors = true;

	// If only the magnitude changed, call updated processors
	if ( !update && triggeredMag && !realOriginUpdate &&
	     triggeredMag->publicID() == info->event->preferredMagnitudeID() ) {
		// Call registered processors
		for ( EventProcessorPtr &proc : _processors ) {
			if ( proc->process(info->event.get(), info->created, info->journal) ) {
				update = true;
			}
		}

		// Processors have been called already
		callProcessors = false;
	}

	OPT(EventType) oldEventType;
	OPT(EventTypeCertainty) oldEventTypeCertainty;

	try { oldEventType = info->event->type(); }
	catch ( ... ) {}
	try { oldEventTypeCertainty = info->event->typeCertainty(); }
	catch ( ... ) {}

	// If an preferred origin is set and the event type has not been fixed
	// manually set it to 'not existing' if the preferred origin is rejected.
	if ( _config.eventAssociation.setAutoEventTypeNotExisting &&
	     info->preferredOrigin &&
	     !info->constraints.fixType ) {
		bool isRejected = false;
		bool notExistingEvent = false;

		SEISCOMP_DEBUG("%s: Try setting event type from evaluation status of "
		               "preferred origin %s",
		               info->event->publicID().c_str(),
		               info->preferredOrigin->publicID());
		try {
			if ( info->preferredOrigin->evaluationStatus() == REJECTED ) {
				isRejected = true;
			}
		}
		catch ( ... ) {}

		try {
			notExistingEvent = info->event->type() == NOT_EXISTING;
		}
		catch ( ... ) {}

		if ( isRejected ) {
			SEISCOMP_DEBUG("  + origin evaluation status: 'rejected'");

			// User has manually fixed an origin, don't touch the event type
			if ( !info->constraints.fixedOrigin() && !notExistingEvent ) {
				SEISCOMP_DEBUG("  + set event type to 'not existing' since "
				               "preferred origin has evaluation status 'rejected'");
				SEISCOMP_LOG(_infoChannel, "Set event type to 'not existing': "
				                           "preferred origin has evaluation "
				                           "status 'rejected' in event %s",
				             info->event->publicID());
				info->event->setType(EventType(NOT_EXISTING));
				update = true;
			}
			else {
				if ( info->constraints.fixedOrigin() ) {
					SEISCOMP_DEBUG("  + do not set event type: preferred origin is fixed");
				}
				else {
					SEISCOMP_DEBUG("  + do not set event type again");
				}
			}
		}
		else {
			// User has manually fixed an origin, don't touch the event type
			// Preferred origin is not rejected, remove the event type if it was
			// previously 'not existing'
			if ( !info->constraints.fixedOrigin() && notExistingEvent ) {
				SEISCOMP_INFO("  + remove event type: evaluation status of "
				              "preferred origin changed from 'rejected'");
				SEISCOMP_LOG(_infoChannel, "Remove event type since evaluation "
				                           "status of preferred origin "
				                           "changed from 'rejected' in event %s",
				             info->event->publicID());
				info->event->setType(None);
				update = true;
			}
			else {
				SEISCOMP_DEBUG("  + do not set event type");
			}
		}
		SEISCOMP_DEBUG("  + current event type: '%s'", info->event->type().toString());
	}

	if ( update ) {
		SEISCOMP_DEBUG("%s: update (created: %d, notifiers enabled: %d)",
		               info->event->publicID(), info->created,
		               Notifier::IsEnabled());

		if ( !info->created ) {
			updateEvent(info, callProcessors);
		}
		else {
			// Call registered processors
			for ( EventProcessorPtr &proc : _processors ) {
				proc->process(info->event.get(), info->created, info->journal);
			}
			info->created = false;
		}
	}

	OPT(EventType) newEventType;
	OPT(EventTypeCertainty) newEventTypeCertainty;

	try { newEventType = info->event->type(); }
	catch ( ... ) {}
	try { newEventTypeCertainty = info->event->typeCertainty(); }
	catch ( ... ) {}

	if ( oldEventType != newEventType ) {
		// Either this method or the processors have changed the event type
		SEISCOMP_LOG(_infoChannel, "Event type of %s has changed to '%s' "
		                           "by scevent or an event processor",
		             info->event->publicID(),
		             newEventType ? newEventType->toString() : "");

		Notifier::Enable();
		auto response = createEntry(
			info->event->publicID(),
			"EvType",
			newEventType ? newEventType->toString() : ""
		);

		response->setSender(author());
		info->addJournalEntry(response.get(), author());
		Notifier::Create(_journal->publicID(), OP_ADD, response.get());

		response = createEntry(
			info->event->publicID(),
			"EvTypeOK",
			newEventType ? newEventType->toString() : ":unset:"
		);

		response->setSender(author());
		info->addJournalEntry(response.get(), author());
		Notifier::Create(_journal->publicID(), OP_ADD, response.get());
		Notifier::Disable();
	}

	if ( oldEventTypeCertainty != newEventTypeCertainty ) {
		// Either this method or the processors have changed the event type
		SEISCOMP_LOG(_infoChannel, "Event type certainty of %s has changed to '%s' "
		                           "by scevent or an event processor",
		             info->event->publicID(),
		             newEventTypeCertainty ? newEventTypeCertainty->toString() : "");

		Notifier::Enable();
		auto response = createEntry(
			info->event->publicID(),
			"EvTypeCertainty",
			newEventTypeCertainty ? newEventTypeCertainty->toString() : ""
		);

		response->setSender(author());
		info->addJournalEntry(response.get(), author());
		Notifier::Create(_journal->publicID(), OP_ADD, response.get());

		response = createEntry(
			info->event->publicID(),
			"EvTypeCertaintyOK",
			newEventTypeCertainty ? newEventTypeCertainty->toString() : ":unset:"
		);

		response->setSender(author());
		info->addJournalEntry(response.get(), author());
		Notifier::Create(_journal->publicID(), OP_ADD, response.get());
		Notifier::Disable();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::choosePreferred(EventInformation *info, DataModel::FocalMechanism *fm) {
	if ( isAgencyIDBlocked(objectAgencyID(info->event.get())) ) {
		SEISCOMP_DEBUG("No preferred fm selection, event %s agencyID is blocked",
		               info->event->publicID());
		SEISCOMP_LOG(_infoChannel, "No preferred fm selection, event %s agencyID is blocked",
		             info->event->publicID());
		return;
	}

	SEISCOMP_DEBUG("%s: testing focal mechanism(%s)",
	               info->event->publicID(),
	               fm->publicID());

	if ( _config.eventAssociation.delayPrefFocMech > 0 ) {
		if ( !info->preferredOrigin ) {
			SEISCOMP_DEBUG("No preferred origins set for event %s, cannot compute delay time, ignoring focal mechanism %s",
			               info->event->publicID().c_str(), fm->publicID());
			SEISCOMP_LOG(_infoChannel, "No preferred origins set for event %s, cannot compute delay time, ignoring focal mechanism %s",
			             info->event->publicID(), fm->publicID());
			return;
		}

		Core::Time minTime = info->preferredOrigin->time().value() + Core::TimeSpan(_config.eventAssociation.delayPrefFocMech,0);
		Core::Time now = Core::Time::UTC();

		//SEISCOMP_LOG(_infoChannel, "Time to reach to set focal mechanism preferred is %s, now is %s",
		//             minTime.toString("%FT%T"), now.toString("%FT%T"));

		if ( minTime > now ) {
			int secs = (minTime - now).seconds() + 1;
			SEISCOMP_DEBUG("Setting preferred focal mechanism needs "
			               "to be delayed by config, still %d secs to go for event %s",
			               secs, info->event->publicID());
			SEISCOMP_LOG(_infoChannel, "Setting preferred focal mechanism needs "
			             "to be delayed by config, still %d secs to go for event %s",
			             secs, info->event->publicID());

			if ( !hasDelayedEvent(info->event->publicID(), SetPreferredFM) )
				_delayEventBuffer.push_back(DelayedEventUpdate(info->event->publicID(), secs, SetPreferredFM));

			return;
		}
	}

	bool update = false;

	if ( !info->preferredFocalMechanism ) {
		if ( isAgencyIDAllowed(objectAgencyID(fm)) || info->constraints.fixFocalMechanism(fm) ) {
			if ( isRejected(fm) ) {
				SEISCOMP_DEBUG("%s: skip setting first preferredFocalMechanismID, '%s' is rejected",
				               info->event->publicID(), fm->publicID());
				SEISCOMP_LOG(_infoChannel, "FocalMechanism %s has not been set preferred: status is REJECTED",
				             fm->publicID());
				return;
			}

			info->event->setPreferredFocalMechanismID(fm->publicID());

			info->preferredFocalMechanism = fm;
			SEISCOMP_INFO("%s: set first preferredFocalMechanismID to %s",
			              info->event->publicID(), fm->publicID());
			SEISCOMP_LOG(_infoChannel, "FocalMechanism %s has been set preferred in event %s",
			             fm->publicID(), info->event->publicID());
			update = true;
		}
		else {
			if ( !isAgencyIDAllowed(objectAgencyID(fm)) ) {
				SEISCOMP_DEBUG("%s: skip setting first preferredFocalMechanismID, agencyID '%s' is blocked",
				               info->event->publicID(), objectAgencyID(fm));
				SEISCOMP_LOG(_infoChannel, "FocalMechanism %s has not been set preferred: agencyID %s is blocked",
				             fm->publicID(), objectAgencyID(fm));
			}
			else {
				SEISCOMP_DEBUG("%s: skip setting first preferredFocalMechanismID, preferredFocalMechanismID is fixed to '%s'",
				               info->event->publicID(), info->constraints.preferredFocalMechanismID);
				SEISCOMP_LOG(_infoChannel, "FocalMechanism %s has not been set preferred: preferredFocalMechanismID is fixed to %s",
				             fm->publicID(), info->constraints.preferredFocalMechanismID);
			}
			return;
		}
	}
	else if ( info->preferredFocalMechanism->publicID() != fm->publicID() ) {
		SEISCOMP_DEBUG("... checking whether focal mechanism %s can become preferred",
		               fm->publicID());

		if ( isRejected(fm) ) {
			SEISCOMP_DEBUG("... skipping potential preferred focalmechanism, status is REJECTED");
			SEISCOMP_LOG(_infoChannel, "FocalMechanism %s has not been set preferred in event %s: status is REJECTED",
			             fm->publicID(), info->event->publicID());
			return;
		}

		// Fixed focalmechanism => check if the passed focalmechanism is the fixed one
		if ( info->constraints.fixedFocalMechanism() ) {
			if ( !info->constraints.fixFocalMechanism(fm) ) {
				SEISCOMP_DEBUG("... skipping potential preferred focal mechanism, focal mechanism '%s' is fixed",
				               info->constraints.preferredFocalMechanismID);
				SEISCOMP_LOG(_infoChannel, "FocalMechanism %s has not been set preferred in event %s: focal mechanism %s is fixed",
				             fm->publicID(), info->event->publicID(),
				             info->constraints.preferredFocalMechanismID);
				return;
			}
		}
		// No fixed focal mechanism => select it using the automatic rules
		else {
			if ( isAgencyIDBlocked(objectAgencyID(fm)) ) {
				SEISCOMP_DEBUG("... skipping potential preferred focal mechanism, agencyID '%s' is blocked",
				               objectAgencyID(fm));
				SEISCOMP_LOG(_infoChannel, "FocalMechanism %s has not been set preferred in event %s: agencyID %s is blocked",
				             fm->publicID(), info->event->publicID(),
				             objectAgencyID(fm));
				return;
			}

			if ( !_config.eventAssociation.priorities.empty() ) {
				// Run through the priority list and check the values
				for ( auto &check : _config.eventAssociation.priorities ) {
					if ( check == "AGENCY" ) {
						int fmAgencyPriority = agencyPriority(objectAgencyID(fm), _config);
						int preferredFMAgencyPriority = agencyPriority(objectAgencyID(info->preferredFocalMechanism.get()), _config);

						if ( fmAgencyPriority < preferredFMAgencyPriority ) {
							SEISCOMP_DEBUG("... skipping potential preferred focal mechanism, priority of agencyID '%s' is too low",
							               objectAgencyID(fm));
							SEISCOMP_LOG(_infoChannel, "FocalMechanism %s has not been set preferred in event %s: priority of agencyID %s is too low",
							             fm->publicID(), info->event->publicID(),
							             objectAgencyID(fm));
							return;
						}
						// Found origin with higher agency priority
						else if ( fmAgencyPriority > preferredFMAgencyPriority ) {
							SEISCOMP_DEBUG("... agencyID '%s' overrides current agencyID '%s'",
							               objectAgencyID(fm), objectAgencyID(info->preferredFocalMechanism.get()));
							SEISCOMP_LOG(_infoChannel, "FocalMechanism %s: agencyID '%s' overrides agencyID '%s'",
							             fm->publicID(), objectAgencyID(fm), objectAgencyID(info->preferredFocalMechanism.get()));
							break;
						}
					}
					else if ( check == "AUTHOR" ) {
						int fmAuthorPriority = authorPriority(objectAuthor(fm), _config);
						int preferredFMAuthorPriority = authorPriority(objectAuthor(info->preferredFocalMechanism.get()), _config);

						if ( fmAuthorPriority < preferredFMAuthorPriority ) {
							SEISCOMP_DEBUG("... skipping potential preferred focal mechanism, priority of author '%s' is too low",
							               objectAuthor(fm));
							SEISCOMP_LOG(_infoChannel, "FocalMechanism %s has not been set preferred in event %s: priority of author %s is too low",
							             fm->publicID(), info->event->publicID(),
							             objectAuthor(fm));
							return;
						}
						// Found focal mechanism with higher author priority
						else if ( fmAuthorPriority > preferredFMAuthorPriority ) {
							SEISCOMP_DEBUG("... author '%s' overrides current author '%s'",
							               objectAuthor(fm), objectAuthor(info->preferredFocalMechanism.get()));
							SEISCOMP_LOG(_infoChannel, "FocalMechanism %s: author '%s' overrides author '%s'",
							             fm->publicID(), objectAuthor(fm), objectAuthor(info->preferredFocalMechanism.get()));
							break;
						}
					}
					else if ( check == "MODE" ) {
						int fmPriority = modePriority(fm);
						int preferredFMPriority = modePriority(info->preferredFocalMechanism.get());

						if ( info->constraints.fixFocalMechanismMode(info->preferredFocalMechanism.get()) ) {
							SEISCOMP_LOG(_infoChannel, "FocalMechanism %s: has priority %d vs %d",
							             fm->publicID(), fmPriority, preferredFMPriority);
							// Set back the evalmode to automatic if a higher priority
							// origin has been send (but not triggered by a magnitude change only)
							if ( fmPriority > preferredFMPriority ) {
								/*
								SEISCOMP_LOG(_infoChannel, "FocalMechanism %s has higher priority: releasing EvPrefOrgEvalMode",
								             origin->publicID());
								JournalEntryPtr entry = new JournalEntry;
								entry->setObjectID(info->event->publicID());
								entry->setAction("EvPrefOrgEvalMode");
								entry->setParameters("");
								entry->setSender(author());
								entry->setCreated(Core::Time::UTC());
								Notifier::Create(_journal->publicID(), OP_ADD, entry.get());
								info->addJournalEntry(entry.get());
								*/
							}
							else
								preferredFMPriority = FOCALMECHANISM_PRIORITY_MAX;
						}

						if ( info->constraints.fixFocalMechanismMode(fm) )
							fmPriority = FOCALMECHANISM_PRIORITY_MAX;

						if ( fmPriority < preferredFMPriority ) {
							SEISCOMP_DEBUG("... skipping potential preferred focal mechanism (%d < %d)",
							               fmPriority, preferredFMPriority);
							SEISCOMP_LOG(_infoChannel, "FocalMechanism %s has not been set preferred in event %s: priority too low (%d < %d)",
							             fm->publicID(), info->event->publicID(),
							             fmPriority, preferredFMPriority);
							return;
						}
						// Found origin with higher status priority
						else if ( fmPriority > preferredFMPriority ) {
							SEISCOMP_LOG(_infoChannel, "FocalMechanism %s: status priority %d overrides status priority %d",
							             fm->publicID(), fmPriority, preferredFMPriority);
							break;
						}
					}
					else if ( check == "STATUS" ) {
						int fmPriority = priority(fm);
						int preferredFMPriority = priority(info->preferredFocalMechanism.get());

						if ( info->constraints.fixFocalMechanismMode(info->preferredFocalMechanism.get()) ) {
							SEISCOMP_LOG(_infoChannel, "FocalMechanism %s: has priority %d vs %d",
							             fm->publicID(), fmPriority, preferredFMPriority);
							// Set back the evalmode to automatic if a higher priority
							// origin has been send (but not triggered by a magnitude change only)
							if ( fmPriority > preferredFMPriority ) {
								/*
								SEISCOMP_LOG(_infoChannel, "FocalMechanism %s has higher priority: releasing EvPrefOrgEvalMode",
								             origin->publicID());
								JournalEntryPtr entry = new JournalEntry;
								entry->setObjectID(info->event->publicID());
								entry->setAction("EvPrefOrgEvalMode");
								entry->setParameters("");
								entry->setSender(author());
								entry->setCreated(Core::Time::UTC());
								Notifier::Create(_journal->publicID(), OP_ADD, entry.get());
								info->addJournalEntry(entry.get());
								*/
							}
							else
								preferredFMPriority = FOCALMECHANISM_PRIORITY_MAX;
						}

						if ( info->constraints.fixFocalMechanismMode(fm) )
							fmPriority = FOCALMECHANISM_PRIORITY_MAX;

						if ( fmPriority < preferredFMPriority ) {
							SEISCOMP_DEBUG("... skipping potential preferred focalmechanism (%d < %d)",
							               fmPriority, preferredFMPriority);
							SEISCOMP_LOG(_infoChannel, "FocalMechanism %s has not been set preferred in event %s: priority too low (%d < %d)",
							             fm->publicID(), info->event->publicID(),
							             fmPriority, preferredFMPriority);
							return;
						}
						// Found origin with higher status priority
						else if ( fmPriority > preferredFMPriority ) {
							SEISCOMP_LOG(_infoChannel, "FocalMechanism %s: status priority %d overrides status priority %d",
							             fm->publicID(), fmPriority, preferredFMPriority);
							break;
						}
					}
					else if ( check == "METHOD" ) {
						int fmMethodPriority = methodPriority(fm->methodID(), _config);
						int preferredFMMethodPriority = methodPriority(info->preferredFocalMechanism->methodID(), _config);

						if ( fmMethodPriority < preferredFMMethodPriority ) {
							SEISCOMP_DEBUG("... skipping potential preferred focalMechanism, priority of method '%s' is too low",
							               fm->methodID());
							SEISCOMP_LOG(_infoChannel, "FocalMechanism %s has not been set preferred in event %s: priority of method %s is too low",
							             fm->publicID(), info->event->publicID(),
							             fm->methodID());
							return;
						}
						// Found origin with higher method priority
						else if ( fmMethodPriority > preferredFMMethodPriority ) {
							SEISCOMP_DEBUG("... methodID '%s' overrides current methodID '%s'",
							               fm->methodID(), info->preferredFocalMechanism->methodID());
							SEISCOMP_LOG(_infoChannel, "FocalMechanism %s: methodID '%s' overrides methodID '%s'",
							             fm->publicID(), fm->methodID(),
							             info->preferredFocalMechanism->methodID());
							break;
						}
					}
					else if ( check == "TIME" ) {
						Core::Time fmCreationTime = created(fm);
						Core::Time preferredFMCreationTime = created(info->preferredFocalMechanism.get());
						if ( fmCreationTime < preferredFMCreationTime ) {
							SEISCOMP_DEBUG("... skipping potential preferred focalmechanism, there is a better one created later");
							return;
						}
						else if ( fmCreationTime > preferredFMCreationTime ) {
							SEISCOMP_LOG(_infoChannel, "FocalMechanism %s: %s is more recent than %s",
							             fm->publicID(), fmCreationTime.iso(),
							             preferredFMCreationTime.iso());
							break;
						}
					}
					else if ( check == "TIME_AUTOMATIC" ) {
						EvaluationMode status = AUTOMATIC;
						try {
							status = fm->evaluationMode();
						}
						catch ( ValueException& ) {}

						// Make a noop in case of non automatic focalmechanisms
						if ( status != AUTOMATIC ) continue;

						Core::Time fmCreationTime = created(fm);
						Core::Time preferredFMCreationTime = created(info->preferredFocalMechanism.get());
						if ( fmCreationTime < preferredFMCreationTime ) {
							SEISCOMP_DEBUG("... skipping potential preferred focalmechanism, there is a better one created later");
							return;
						}
						else if ( fmCreationTime > preferredFMCreationTime ) {
							SEISCOMP_LOG(_infoChannel, "FocalMechanism %s: %s (automatic) is more recent than %s",
							             fm->publicID(), fmCreationTime.iso(),
							             preferredFMCreationTime.iso());
							break;
						}
					}
					else if ( check == "SCORE" ) {
						double score = _score->evaluate(fm);
						double preferredScore = _score->evaluate(info->preferredFocalMechanism.get());
						if ( score < preferredScore ) {
							SEISCOMP_DEBUG("... skipping potential preferred focalmechanism, there is one with higher score: %f > %f",
							               preferredScore, score);
							return;
						}
						else if ( score > preferredScore ) {
							SEISCOMP_LOG(_infoChannel, "FocalMechanism %s: score of %f is larger than %f",
							             fm->publicID(), score,
							             preferredScore);
							break;
						}
					}
				}
			}
			else {
				int fmAgencyPriority = agencyPriority(objectAgencyID(fm), _config);
				int preferredFMAgencyPriority = agencyPriority(objectAgencyID(info->preferredFocalMechanism.get()), _config);

				if ( fmAgencyPriority < preferredFMAgencyPriority ) {
					SEISCOMP_DEBUG("... skipping potential preferred focalmechanism, priority of agencyID '%s' is too low",
					               objectAgencyID(fm));
					SEISCOMP_LOG(_infoChannel, "FocalMechanism %s has not been set preferred in event %s: priority of agencyID %s is too low",
					             fm->publicID(), info->event->publicID(),
					             objectAgencyID(fm));
					return;
				}

				// Same agency priorities -> compare fm priority
				if ( fmAgencyPriority == preferredFMAgencyPriority ) {
					int fmPriority = priority(fm);
					int preferredFMPriority = priority(info->preferredFocalMechanism.get());

					if ( info->constraints.fixFocalMechanismMode(info->preferredFocalMechanism.get()) ) {
						SEISCOMP_LOG(_infoChannel, "FocalMechanism %s: has priority %d vs %d",
						             fm->publicID(), fmPriority, preferredFMPriority);

						// Set back the evalmode to automatic if a higher priority
						// focalmechanism has been send
						if ( fmPriority > preferredFMPriority ) {
							/*
							SEISCOMP_LOG(_infoChannel, "Origin %s has higher priority: releasing EvPrefOrgEvalMode",
							             origin->publicID());
							JournalEntryPtr entry = new JournalEntry;
							entry->setObjectID(info->event->publicID());
							entry->setAction("EvPrefOrgEvalMode");
							entry->setParameters("");
							entry->setSender(author());
							entry->setCreated(Core::Time::UTC());
							Notifier::Create(_journal->publicID(), OP_ADD, entry.get());
							info->addJournalEntry(entry.get());
							*/
						}
						else
							preferredFMPriority = FOCALMECHANISM_PRIORITY_MAX;
					}

					if ( info->constraints.fixFocalMechanismMode(fm) ) {
						fmPriority = FOCALMECHANISM_PRIORITY_MAX;
					}

					if ( fmPriority < preferredFMPriority ) {
						SEISCOMP_DEBUG("... skipping potential preferred focal mechanism, "
						               "priority based on evaluation mode < "
						               "currently preferred (%d < %d)",
						               fmPriority, preferredFMPriority);
						SEISCOMP_LOG(_infoChannel, "FocalMechanism %s has not been set preferred in event %s: priority too low (%d < %d)",
						             fm->publicID(), info->event->publicID(),
						             fmPriority, preferredFMPriority);
						return;
					}

					if ( fmPriority == preferredFMPriority ) {
						EvaluationMode status = AUTOMATIC;
						try {
							status = fm->evaluationMode();
						}
						catch ( ValueException& ) {}

						if ( status == AUTOMATIC ) {
							SEISCOMP_DEBUG("Same priority and mode is AUTOMATIC");

							if ( created(fm) < created(info->preferredFocalMechanism.get()) ) {
								SEISCOMP_DEBUG("... skipping potential preferred focal mechanism, there is a better one created later");
								return;
							}
						}
					}
				}
				else {
					SEISCOMP_DEBUG("... agencyID '%s' overrides current agencyID '%s'",
					               objectAgencyID(fm), objectAgencyID(info->preferredFocalMechanism.get()));
					SEISCOMP_LOG(_infoChannel, "FocalMechanism %s: agencyID '%s' overrides agencyID '%s'",
					             fm->publicID(), objectAgencyID(fm), objectAgencyID(info->preferredFocalMechanism.get()));
				}
			}
		}

		info->event->setPreferredFocalMechanismID(fm->publicID());

		info->preferredFocalMechanism = fm;
		SEISCOMP_INFO("...... %s: set preferred in %s",
		              fm->publicID(), info->event->publicID());
		SEISCOMP_LOG(_infoChannel, "FocalMechanism %s has been set preferred in event %s",
		             fm->publicID(), info->event->publicID());

		update = true;
	}
	else {
		SEISCOMP_INFO("%s: nothing to do", info->event->publicID());
	}

	if ( update ) {
		// Update preferred magnitude based on new focal mechanism if
		// Mw is fixed
		if ( info->constraints.fixMw ) {
			choosePreferred(info, info->preferredOrigin.get(), nullptr);
		}

		if ( !info->created ) {
			updateEvent(info);
		}
		else {
			// Call registered processors
			for ( EventProcessorPtr &proc : _processors ) {
				proc->process(info->event.get(), info->created, info->journal);
			}

			info->created = false;
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::updatePreferredOrigin(EventInformation *info, bool refresh) {
	if ( !info->event ) return;

	for ( size_t i = 0; i < info->event->originReferenceCount(); ++i ) {
		OriginPtr org = _cache.get<Origin>(info->event->originReference(i)->originID());
		if ( !org ) continue;
		if ( !org->magnitudeCount() && query() ) {
			SEISCOMP_DEBUG("... loading magnitudes for origin %s", org->publicID());
			query()->loadMagnitudes(org.get());
		}
		choosePreferred(info, org.get(), nullptr, false, refresh);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::updatePreferredFocalMechanism(EventInformation *info) {
	if ( !info->event ) return;

	for ( size_t i = 0; i < info->event->focalMechanismReferenceCount(); ++i ) {
		FocalMechanismPtr fm = _cache.get<FocalMechanism>(info->event->focalMechanismReference(i)->focalMechanismID());
		if ( !fm ) continue;
		if ( !fm->momentTensorCount() && query() ) {
			SEISCOMP_DEBUG("... loading moment tensor for focal mechanism %s", fm->publicID());
			query()->loadMomentTensors(fm.get());
		}
		choosePreferred(info, fm.get());
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool EventTool::mergeEvents(EventInformation *target, EventInformation *source) {
	Event *sourceEvent = source->event.get();
	if ( !sourceEvent ) return false;

	Event *targetEvent = target->event.get();
	if ( !targetEvent ) return false;

	if ( sourceEvent->originReferenceCount() == 0 )
		query()->loadOriginReferences(sourceEvent);

	if ( sourceEvent->focalMechanismReferenceCount() == 0 )
		query()->loadFocalMechanismReferences(sourceEvent);

	while ( sourceEvent->originReferenceCount() > 0 ) {
		OriginReferencePtr ref = sourceEvent->originReference(0);
		sourceEvent->removeOriginReference(0);

		OriginPtr org = _cache.get<Origin>(ref->originID());

		if ( !org ) {
			SEISCOMP_WARNING("%s: referenced origin %s not found",
			                 sourceEvent->publicID(),
			                 ref->originID());

			if ( targetEvent->originReference(ref->originID()) ) {
				SEISCOMP_WARNING("... origin %s already associated to event %s",
				                 ref->originID(),
				                 targetEvent->publicID());
			}
			else {
				SEISCOMP_DEBUG("%s: added origin reference %s due to merge request",
				               targetEvent->publicID(),
				               ref->originID());
				SEISCOMP_LOG(_infoChannel, "OriginReference %s added to event %s due to merge request",
				             ref->originID(), targetEvent->publicID());
				OriginReferencePtr newRef = new OriginReference(*ref);
				targetEvent->add(newRef.get());
			}

			continue;
		}

		if ( !target->associate(org.get()) ) {
			SEISCOMP_WARNING("%s: associating origin %s failed",
			                 targetEvent->publicID(),
			                 org->publicID());
			continue;
		}

		SEISCOMP_INFO("%s: associated origin %s due to merge request",
		              targetEvent->publicID(),
		              org->publicID());

		if ( !org->magnitudeCount() && query() ) {
			SEISCOMP_DEBUG("... loading magnitudes for origin %s", org->publicID());
			query()->loadMagnitudes(org.get());
		}

		// Update the preferred origin if configured to do so
		if ( _config.eventAssociation.updatePreferredSolutionAfterMerge )
			choosePreferred(target, org.get(), nullptr);
	}

	while ( sourceEvent->focalMechanismReferenceCount() > 0 ) {
		FocalMechanismReferencePtr ref = sourceEvent->focalMechanismReference(0);
		sourceEvent->removeFocalMechanismReference(0);

		FocalMechanismPtr fm = _cache.get<FocalMechanism>(ref->focalMechanismID());

		if ( !fm ) {
			SEISCOMP_WARNING("%s: referenced focal mechanism %s not found",
			                 sourceEvent->publicID(),
			                 ref->focalMechanismID());

			if ( targetEvent->focalMechanismReference(ref->focalMechanismID()) ) {
				SEISCOMP_WARNING("... focal mechanism %s already associated to event %s",
				                 ref->focalMechanismID(),
				                 targetEvent->publicID());
			}
			else {
				SEISCOMP_DEBUG("%s: added focal mechanism reference %s due to merge request",
				               targetEvent->publicID(),
				               ref->focalMechanismID());
				SEISCOMP_LOG(_infoChannel, "FocalMechanismReference %s added to event %s due to merge request",
				             ref->focalMechanismID(), targetEvent->publicID());
				FocalMechanismReferencePtr newRef = new FocalMechanismReference(*ref);
				targetEvent->add(newRef.get());
			}

			continue;
		}

		if ( !target->associate(fm.get()) ) {
			SEISCOMP_WARNING("%s: associating focal mechanism %s failed",
			                 targetEvent->publicID(),
			                 fm->publicID());
			continue;
		}

		SEISCOMP_INFO("%s: associated focal mechanism %s due to merge request",
		              targetEvent->publicID(),
		              fm->publicID());

		if ( !fm->momentTensorCount() && query() ) {
			SEISCOMP_DEBUG("... loading moment tensor for focal mechanism %s", fm->publicID());
			query()->loadMomentTensors(fm.get());
		}

		// Update the preferred focalfechanism
		if ( _config.eventAssociation.updatePreferredSolutionAfterMerge )
			choosePreferred(target, fm.get());
	}

	// Remove source event
	SEISCOMP_INFO("%s: deleted", sourceEvent->publicID());
	SEISCOMP_LOG(_infoChannel, "Delete event %s after merging",
	             sourceEvent->publicID());

	sourceEvent->detach();
	_cache.remove(sourceEvent);

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::removedFromCache(Seiscomp::DataModel::PublicObject *po) {
	bool saveState = DataModel::Notifier::IsEnabled();
	DataModel::Notifier::Disable();

	auto it = _events.find(po->publicID());
	if ( it != _events.end() ) {
		// Remove EventInfo item for uncached event
		// Don't erase the element but mark it. While iterating over the
		// event cache and performing check and cache updates an event can
		// be removed from the cache which leads to crashes. We clean up
		// the removed events after the work has been done.
		it->second->aboutToBeRemoved = true;
		SEISCOMP_DEBUG("... mark event %s to be removed from cache", po->publicID());
	}

	// Only allow to detach objects from the EP instance if it hasn't been read
	// from a file
	if ( _config.epFile.empty() )
		po->detach();

	DataModel::Notifier::SetEnabled(saveState);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::updateEvent(EventInformation *info, bool callProcessors) {
	DataModel::Event *ev = info->event.get();
	Core::Time now = Core::Time::UTC();
	// Set the modification to current time
	try {
		ev->creationInfo().setModificationTime(now);
		if ( ev->creationInfo().agencyID() != agencyID() )
			ev->creationInfo().setAgencyID(agencyID());
		if ( ev->creationInfo().author() != author() )
			ev->creationInfo().setAuthor(author());
	}
	catch ( ... ) {
		DataModel::CreationInfo ci;
		ci.setAgencyID(agencyID());
		ci.setAuthor(author());
		ci.setModificationTime(now);
		ev->setCreationInfo(ci);
	}

	logObject(_outputEvent, now);

	if ( !ev->parent() ) {

	}

	// Flag the event as updated to be sent around
	ev->update();

	if ( callProcessors ) {
		// Call registered processors
		for ( EventProcessorPtr &proc : _processors ) {
			proc->process(ev, info->created, info->journal);
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EventTool::updateRegionName(DataModel::Event *ev, DataModel::Origin *org) {
	std::string reg = org ? region(org) : "";
	EventDescription *ed = eventRegionDescription(ev);
	if ( ed ) {
		if ( ed->text() != reg ) {
			SEISCOMP_INFO("%s: updating region name to '%s'",
			              ev->publicID(), reg);
			SEISCOMP_LOG(_infoChannel, "Event %s region name updated: %s",
			             ev->publicID(), reg);
			ed->setText(reg);
			ed->update();
		}
	}
	else {
		EventDescriptionPtr ed = new EventDescription(reg, REGION_NAME);
		ev->add(ed.get());
		SEISCOMP_INFO("%s: adding region name '%s'",
		              ev->publicID(), reg);
		SEISCOMP_LOG(_infoChannel, "Event %s got new region name: %s",
		             ev->publicID(), reg);
	}

	if ( _config.populateFERegion ) {
		reg = "";
		if ( org ) {
			try {
				reg = Regions::getFlinnEngdahlRegion(org->latitude(), org->longitude());
			}
			catch ( ... ) {	}
		}

		ed = eventFERegionDescription(ev);
		if ( ed ) {
			if ( ed->text() != reg ) {
				SEISCOMP_INFO("%s: updating Flinn-Engdahl region name to '%s'",
				              ev->publicID(), reg);
				SEISCOMP_LOG(_infoChannel, "Event %s Flinn-Engdahl region name updated: %s",
				             ev->publicID(), reg);
				ed->setText(reg);
				ed->update();
			}
		}
		else {
			EventDescriptionPtr ed = new EventDescription(reg, FLINN_ENGDAHL_REGION);
			ev->add(ed.get());
			SEISCOMP_INFO("%s: adding Flinn-Engdahl region name '%s'",
			              ev->publicID(), reg);
			SEISCOMP_LOG(_infoChannel, "Event %s got new Flinn-Engdahl region name: %s",
			             ev->publicID(), reg);
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
