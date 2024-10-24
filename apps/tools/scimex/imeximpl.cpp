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

#define SEISCOMP_COMPONENT ScImEx
#include "imeximpl.h"
#include "imexmessage.h"

#include <functional>
#include <algorithm>

#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/zlib.hpp>

#include <seiscomp/logging/log.h>
#include <seiscomp/system/environment.h>
#include <seiscomp/datamodel/notifier.h>
#include <seiscomp/datamodel/stationmagnitude.h>
#include <seiscomp/datamodel/magnitude.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/utils.h>
#include <seiscomp/core/strings.h>
#include <seiscomp/core/system.h>
#include <seiscomp/core/exceptions.h>
#include <seiscomp/utils/files.h>
#include <seiscomp/client/queue.ipp>
#include <seiscomp/utils/leparser.h>
#include <seiscomp/datamodel/exchange/trunk.h>
#include <seiscomp/io/archive/xmlarchive.h>


using namespace std;


namespace Seiscomp {
namespace Applications {

#define USE_MSG_WRAPPER

#ifdef USE_MSG_WRAPPER
#define SEND_MSG(group, msg) sendMessage(group, msg)
#else
#define SEND_MSG(group, msg) sendMessage(group, msg.notifierMessage())
#endif

// debug
#define PUBLIC_OBJECT_INFO(object, notifier) \
	SEISCOMP_INFO("Sending %s with publicID %s and operation %s", \
	              object->className(), object->publicID().c_str(), \
	              (*notifier)->operation().toString())


void dataMessageInfo(Core::DataMessage* dataMessage) {
	Core::DataMessage::iterator it = dataMessage->begin();
	for ( ; it != dataMessage->end(); it++ ) {
		DataModel::Event* event = DataModel::Event::Cast(it->get());
		if ( event )
			SEISCOMP_INFO("Sending %s with publicID %s",
			              event->className(), event->publicID().c_str());
	}
}

void notifierMessageInfo(DataModel::NotifierMessage* notifierMessage) {
	DataModel::NotifierMessage::iterator notifierIt = notifierMessage->begin();

	/*
	while ( notifistderIt != notifierMessage->end() ) {
		(*notifierIt)->apply();
		++notifierIt;
	}

	notifierIt = notifierMessage->begin();
	*/
	for ( ; notifierIt != notifierMessage->end(); ++notifierIt ) {
		Core::BaseObject* object = (*notifierIt)->object();
		if ( !object ) continue;

		string className = object->className();
		if ( className == DataModel::Pick::ClassName() ) {
			DataModel::Pick* pick = DataModel::Pick::Cast(object);
			if ( pick ) PUBLIC_OBJECT_INFO(pick, notifierIt);
		}
		else if ( className == DataModel::Amplitude::ClassName() ) {
			DataModel::Amplitude* amplitude = DataModel::Amplitude::Cast(object);
			if ( amplitude ) PUBLIC_OBJECT_INFO(amplitude, notifierIt);
		}
		else if ( className == DataModel::Origin::ClassName() ) {
			DataModel::Origin* origin = DataModel::Origin::Cast(object);
			if ( origin ) PUBLIC_OBJECT_INFO(origin, notifierIt);
		}
		else if ( className == DataModel::Arrival::ClassName() ) {
			DataModel::Arrival* arrival = DataModel::Arrival::Cast(object);
			if ( arrival )
				SEISCOMP_INFO("Sending %s for pick %s and operation %s",
				              arrival->className(), arrival->pickID().c_str(),
				              (*notifierIt)->operation().toString());
		}
		else if ( className == DataModel::StationMagnitude::ClassName() ) {
			DataModel::StationMagnitude* sm = DataModel::StationMagnitude::Cast(object);
			if ( sm ) PUBLIC_OBJECT_INFO(sm, notifierIt);
		}
		else if ( className == DataModel::StationMagnitudeContribution::ClassName() ) {
			DataModel::StationMagnitudeContribution* smc = DataModel::StationMagnitudeContribution::Cast(object);
			if ( smc )
				SEISCOMP_INFO("Sending %s for stationmagnitude %s and operation %s",
				              smc->className(), smc->stationMagnitudeID().c_str(),
				              (*notifierIt)->operation().toString());
		}
		else if ( className == DataModel::Magnitude::ClassName() ) {
			DataModel::Magnitude* magnitude = DataModel::Magnitude::Cast(object);
			if ( magnitude ) PUBLIC_OBJECT_INFO(magnitude, notifierIt);
		}
		else if ( className == DataModel::Event::ClassName() ) {
			DataModel::Event* event = DataModel::Event::Cast(object);
			if ( event ) PUBLIC_OBJECT_INFO(event, notifierIt);
		}
		else {
			SEISCOMP_ERROR("notiferMessageInfo: Received unhandled object: %s with notifier type: %s", className.c_str(), (*notifierIt)->operation().toString());
		}
	}
}

void messageInfo(Core::Message* message) {

	DataModel::NotifierMessage* notifierMessage = DataModel::NotifierMessage::Cast(message);
	IMEXMessage* imexMessage = IMEXMessage::Cast(message);
	Core::DataMessage* dataMessage = Core::DataMessage::Cast(message);

	if ( notifierMessage ) {
		notifierMessageInfo(notifierMessage);
	}
	else if ( imexMessage ) {
		notifierMessageInfo(&imexMessage->notifierMessage());
	}
	else if ( dataMessage ) {
		// dataMessageInfo(dataMessage);
	}
	else {
		SEISCOMP_ERROR("messageInfo: Unknown message found");
	}

}
// end debug
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<





// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool findOrigin(const string& id, const ImExImpl::SentOriginList& list)
{
	ImExImpl::SentOriginList::const_iterator it = list.begin();
	for ( ; it != list.end(); ++it)
		if ((*it)->publicID() == id)
			return true;
	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
ImExImpl::ImExImpl(ImEx* imex, const string& sinkName)
 : _sinkName(sinkName),
   _imex(imex),
   _thread0(NULL),
   _thread1(NULL),
   _sleepDuration(3),
   _filter(true),
   _useDefinedRoutingTable(false),
   _isRunning(false),
   _maxQueueSize(5000),
   _messageQueue(_maxQueueSize),
   _contentType(Client::Protocol::Binary),
   _contentEncoding(Client::Protocol::Deflate),
   isOriginEligibleImpl(&ImExImpl::isOriginEligibleImport),
   filterMagnitudeImpl(&ImExImpl::filterMagnitudeImport),
   sendMessageImpl(&ImExImpl::sendMessageImport) {
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
ImExImpl::~ImExImpl() {
	if ( _sink ) {
		if ( _sink->isConnected() ) {
			SEISCOMP_DEBUG("Disconnecting from sink %s", _sink->source().c_str());
			_sink->disconnect();
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool ImExImpl::init() {
	try {
		_filter = _imex->configGetBool("hosts." + _sinkName + ".filter");
	}
	catch ( Config::Exception &e ) {
		SEISCOMP_DEBUG("(%s) %s - Default value is true", _sinkName.c_str(), e.what());
	}

	try {
		_useDefinedRoutingTable = _imex->configGetBool("hosts." + _sinkName + ".useDefinedRoutingTable");
	}
	catch ( Config::Exception &e ) {
		SEISCOMP_DEBUG("(%s) %s - Using default routing table", _sinkName.c_str(), e.what());
	}

	try {
		_conversion = _imex->configGetString("hosts." + _sinkName + ".conversion");
		_contentType = Client::Protocol::XML;
	}
	catch ( Config::Exception & ) {}

	// Get sink address
	try {
		_sinkAddress = _imex->configGetString("hosts." + _sinkName + ".address");
	}
	catch ( Config::Exception &e ) {
		SEISCOMP_DEBUG("(%s) %s ", e.what(), _sinkName.c_str());
		return false;
	}

	try {
		_userName = _imex->configGetString("hosts." + _sinkName + ".userName");
	}
	catch ( Config::Exception & ) {}

	// Get criteria
	try {
		string criteriaStr = _imex->configGetString("hosts." + _sinkName + ".criteria");

		Utils::LeTokenizer tokenizer(criteriaStr);
		if( !tokenizer.tokenize() ) {
			SEISCOMP_ERROR("%s", tokenizer.what().c_str());
			return false;
		}
		Utils::LeParserTypes::Tokens tokens = tokenizer.tokens();

		SEISCOMP_DEBUG("= Parsed criterion for sink: %s =", _sinkName.c_str());
		for ( size_t i = 0; i < tokens.size(); ++i )
			SEISCOMP_DEBUG("%s", tokens[i].c_str());

		CriterionFactory<CriterionInterface> factory(_sinkName, _imex);
		Utils::LeParser<CriterionInterface> parser(tokens, &factory);
		_criterion = boost::shared_ptr<CriterionInterface>(parser.parse());
		if ( !_criterion.get() ) {
			SEISCOMP_ERROR("%s", parser.what());
			return false;
		}

		if ( parser.error() )
			SEISCOMP_ERROR("%s", parser.what());
	}
	catch ( Config::Exception& e ) {
		if ( _filter ) {
			SEISCOMP_DEBUG("(%s) %s ", e.what(), _sinkName.c_str());
			return false;
		}
		_criterion.reset();
	}

	if ( _imex->mode() == ImEx::EXPORT ) {
		isOriginEligibleImpl = &ImExImpl::isOriginEligibleExport;
		filterMagnitudeImpl  = &ImExImpl::filterMagnitudeExport;
		sendMessageImpl      = &ImExImpl::sendMessageExport;
	}
	else {
		isOriginEligibleImpl = &ImExImpl::isOriginEligibleImport;
		filterMagnitudeImpl  = &ImExImpl::filterMagnitudeImport;
		sendMessageImpl      = &ImExImpl::sendMessageImport;
	}

	if ( !buildRoutingTable() )
		return false;

	_thread0 = new boost::thread(boost::bind(&ImExImpl::connectToSink, this));
	_thread0->yield();

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Client::Result ImExImpl::connectToSink() {
	Client::Result r;

	_sink = new Client::Connection;

	_sink = new Client::Connection();
	r = _sink->setSource(_sinkAddress);
	if ( r != Client::OK ) {
		SEISCOMP_ERROR("Invalid sink URL: %s", _sinkAddress.c_str());
		return r;
	}

	// Connect to the sink master and use a default name
	while ( (r = _sink->connect(_userName, Client::Protocol::IMPORT_GROUP)) != Client::OK ) {
		if ( _imex->isExitRequested() )
			return Client::Error;

		SEISCOMP_ERROR("Could not connect to the sink master %s due to error %s - Trying to reconnect ...",
		              _sinkAddress.c_str(), r.toString());
		Core::sleep(_sleepDuration);
	}

	SEISCOMP_DEBUG("Successfully connected to sink master: %s", _sinkAddress.c_str());

	_isRunning = true;

	// Spawn "send message" thread
	_thread1 = new boost::thread(boost::bind(&ImExImpl::sendMessageRaw, this));
	_thread1->yield();

	// Get rid of data messages and read commands that are may send.
	readSinkMessages();

	return Client::OK;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool ImExImpl::handleMessage(Core::Message *message) {
	if ( _messageQueue.size() >= _maxQueueSize ) {
		SEISCOMP_DEBUG("(%s) message queue exceeded maximum size of %ld. Skipping message.",
		               _sinkName.c_str(), (long int)_maxQueueSize);
		return false;
	}

	if ( _imex->mode() == ImEx::EXPORT ) {
		DataModel::NotifierMessagePtr notifierMessage = DataModel::NotifierMessage::Cast(message);
		if ( notifierMessage ) {
			SEISCOMP_DEBUG("Type of message: NotifierMessage");
			handleNotifierMessage(notifierMessage.get());
		}
	}
	else if ( _imex->mode() == ImEx::IMPORT ) {
		IMEXMessagePtr imexMessage = IMEXMessage::Cast(message);
		if ( imexMessage ) {
			SEISCOMP_DEBUG("Type of message: IMEXMessage");
			handleNotifierMessage(&imexMessage->notifierMessage());
		}

		Core::DataMessage* dataMessage = Core::DataMessage::Cast(message);
		if ( dataMessage ) {
			SEISCOMP_DEBUG("Type of message: DataMessage");
			// handleDataMessage(dataMessage);
			Core::DataMessage::iterator it = dataMessage->begin();
			while ( it != dataMessage->end() ) {
				DataModel::Event* event = DataModel::Event::Cast(it->get());
				if ( event ) {
					// handleEvent(event);
					EventList::iterator eventIt = _eventList.begin();
					for ( ; eventIt != _eventList.end(); ++eventIt ) {
						if ( eventIt->publicID() == event->publicID() ) {
							*eventIt = event;
							break;
						}
					}

					if ( eventIt == _eventList.end() )
						_eventList.push_back(event);
				}
				++it;
			}
		}
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void ImExImpl::cleanUp() {
	SEISCOMP_DEBUG("Cleaning up in implementation for %s", _sinkName.c_str());
	cleanUp(_eventList);

	Core::Time now = Core::Time::GMT();
	SentOriginList::iterator it = _sentOrigins.begin();
	while ( it != _sentOrigins.end() ) {
		if ( now - (*it)->time().value() > _imex->cleanUpInterval() ) {
			SEISCOMP_DEBUG("One %s with id: %s removed",
			               (*it)->className(), (*it)->publicID().c_str());
			// Remove sent arrivals and Amplitudes
			for ( size_t i = 0; i < (*it)->arrivalCount(); ++i ) {
				DataModel::Arrival* arrival = (*it)->arrival(i);

				SentPicks::iterator pickIt = _sentPicks.begin();
				while ( pickIt != _sentPicks.end() ) {
					if ( arrival->pickID() == (*pickIt)->publicID() )
						pickIt = _sentPicks.erase(pickIt);
					else
						++pickIt;
				}

				SentAmplitudes::iterator saIt = _sentAmplitudes.begin();
				while ( saIt != _sentAmplitudes.end() ) {
					if ( arrival->pickID() == (*saIt)->pickID() )
						saIt = _sentAmplitudes.erase(saIt);
					else
						++saIt;
				}
			}
			it = _sentOrigins.erase(it);
		}
		else {
			++it;
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void ImExImpl::stop() {
	_messageQueue.close();
	_isRunning = false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void ImExImpl::wait() {
	if ( _thread0 ) {
		_thread0->join();
		delete _thread0;
		_thread0 = NULL;
	}

	if ( _thread1 ) {
		_thread1->join();
		delete _thread1;
		_thread1 = NULL;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
string ImExImpl::sinkName() const {
	return _sinkName + "@" + _sinkAddress;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void ImExImpl::cleanUp(EventList& eventList)
{
	EventList::iterator it = eventList.begin();
	while ( it != eventList.end() ) {
		try {
			if ( Core::Time::GMT() - it->event()->creationInfo().creationTime() > _imex->cleanUpInterval() ) {
				SEISCOMP_DEBUG("One %s with id: %s removed",
						it->event()->className(), it->event()->publicID().c_str());
				it = eventList.erase(it);
			}
			else {
				++it;
			}
		}
		catch ( Core::ValueException& ) {
			SEISCOMP_ERROR("Time member for %s with id: %s has not been set",
					it->event()->className(), it->event()->publicID().c_str());
			it = eventList.erase(it);
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
ImExImpl* ImExImpl::Create(ImEx* imex, const string& implName)
{
	ImExImpl* impl = new ImExImpl(imex, implName);
	if ( !impl->init() ) {
		delete impl;
		return NULL;
	}
	return impl;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
ImExImpl::RoutingTable ImExImpl::CreateDefaultRoutingTable() {
	RoutingTable tmpRoutingTable;
	tmpRoutingTable.insert(make_pair(DataModel::Pick::ClassName(), "PICK"));
	tmpRoutingTable.insert(make_pair(DataModel::Arrival::ClassName(), "LOCATION"));
	tmpRoutingTable.insert(make_pair(DataModel::Amplitude::ClassName(), "AMPLITUDE"));
	tmpRoutingTable.insert(make_pair(DataModel::Origin::ClassName(), "LOCATION"));
	tmpRoutingTable.insert(make_pair(DataModel::StationMagnitude::ClassName(), "MAGNITUDE"));
	tmpRoutingTable.insert(make_pair(DataModel::StationMagnitudeContribution::ClassName(), "MAGNITUDE"));
	tmpRoutingTable.insert(make_pair(DataModel::Magnitude::ClassName(), "MAGNITUDE"));
	//! TODO: Implement forwarding of focal mechanisms and moment tensors
	/*
	tmpRoutingTable.insert(make_pair(DataModel::FocalMechanism::ClassName(), "FOCMECH"));
	tmpRoutingTable.insert(make_pair(DataModel::MomentTensor::ClassName(), "FOCMECH"));
	tmpRoutingTable.insert(make_pair(DataModel::MomentTensorStationContribution::ClassName(), "FOCMECH"));
	tmpRoutingTable.insert(make_pair(DataModel::MomentTensorComponentContribution::ClassName(), "FOCMECH"));
	tmpRoutingTable.insert(make_pair(DataModel::MomentTensorPhaseSetting::ClassName(), "FOCMECH"));
	*/
	tmpRoutingTable.insert(make_pair(DataModel::Event::ClassName(), "EVENT"));

	return tmpRoutingTable;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool ImExImpl::buildRoutingTable() {
	RoutingTable tmpRoutingTable = CreateDefaultRoutingTable();

	if ( _useDefinedRoutingTable ) {
		if (!configGetRoutingTable("hosts." + _sinkName, "routingTable", &_routingTable))
			return false;
	}

	if ( _imex->mode() == ImEx::EXPORT ) {
		if ( !_useDefinedRoutingTable ) {
			for ( auto &&it : tmpRoutingTable )
				_routingTable.insert(make_pair(it.first, Client::Protocol::IMPORT_GROUP));
		}
	}
	else if ( _imex->mode() == ImEx::IMPORT ) {
		if ( !_useDefinedRoutingTable )
			_routingTable = tmpRoutingTable;
	}

	// Print routing table
	SEISCOMP_DEBUG("Routing table:");
	for ( auto &&it : _routingTable )
		SEISCOMP_DEBUG("%s -> %s", it.first.c_str(), it.second.c_str());

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool ImExImpl::fillRoutingTable(vector<string>& src, RoutingTable& dest) {
	dest.clear();
	for ( size_t i = 0; i < src.size(); ++i ) {
		vector<string> tokens;
		Core::split(tokens, src[i].c_str(), ":");
		if ( tokens.size() != 2 )
			SEISCOMP_INFO("Malformed routing table entry: %s", src[i].c_str());
		else
			dest[tokens[0]] = tokens[1];
	}
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void ImExImpl::handleNotifierMessage(DataModel::NotifierMessage* notifierMessage) {
	SentOriginList tmpSentOrigins;
	DataModel::NotifierMessage::iterator notifierIt = notifierMessage->begin();
	for ( ; notifierIt!= notifierMessage->end(); ++notifierIt ) {
		Core::BaseObject* object = (*notifierIt)->object();
		if ( !object ) continue;

		DataModel::PublicObject *po = DataModel::PublicObject::Cast((*notifierIt)->object());
		if ( po )
			SEISCOMP_DEBUG("(%s) Handling notifier (%s, %s, %s [%s])", _sinkName.c_str(),
			               (*notifierIt)->parentID().c_str(), (*notifierIt)->operation().toString(),
			               (*notifierIt)->object()->className(), po->publicID().c_str());
		else
			SEISCOMP_DEBUG("(%s) Handling notifier (%s, %s, %s)", _sinkName.c_str(),
			               (*notifierIt)->parentID().c_str(), (*notifierIt)->operation().toString(),
			               (*notifierIt)->object()->className());

		string className = object->className();
		if ( className == DataModel::Origin::ClassName() ) {
			DataModel::Origin* origin = DataModel::Origin::Cast(object);

			bool hasBeenSentAlready = false;
			if ( (*notifierIt)->operation() == DataModel::OP_UPDATE )
				hasBeenSentAlready = hasOriginBeenSent(origin->publicID());

			SEISCOMP_DEBUG("(%s) Handling object of type: %s with id: %s",
			               _sinkName.c_str(), className.c_str(), origin->publicID().c_str());
			if ( hasBeenSentAlready || isOriginEligible(origin) ) {
				if ( hasBeenSentAlready || hasEventBeenSent(origin) || filter(origin) ) {
					sendOrigin(origin, hasBeenSentAlready);
					tmpSentOrigins.push_back(origin);
				}
			}
		}
		if ( className == DataModel::Arrival::ClassName() ) {
			DataModel::Arrival* arrival = DataModel::Arrival::Cast(object);
			SEISCOMP_DEBUG("(%s) Handling object of type: %s",
			               _sinkName.c_str(), className.c_str());
			DataModel::Origin* origin = arrival->origin();
			if ( !origin ) { continue; }
			if ( hasOriginBeenSent(origin->publicID()) ) {
				ImEx::PickList::const_iterator pickIt = _imex->pickList().begin();
				for ( ; pickIt != _imex->pickList().end(); ++pickIt ) {
					if ( arrival->pickID() == (*pickIt)->publicID() ) {
						break;
					}
				}
				IMEXMessage imexMessage;
				if ( pickIt != _imex->pickList().end() ) {
					imexMessage.notifierMessage().attach(
						new DataModel::Notifier(DataModel::EventParameters::ClassName(), DataModel::OP_ADD, pickIt->get())
					);
					_sentPicks.push_back(pickIt->get());
				}
				imexMessage.notifierMessage().attach(
					new DataModel::Notifier(origin->publicID(), (*notifierIt)->operation(), arrival)
				);
				SEND_MSG(_routingTable[className], &imexMessage);
			}
			else if ( isOriginEligible(origin) ) {
				if ( hasEventBeenSent(origin) || filter(origin) ) {
					sendOrigin(origin);
					tmpSentOrigins.push_back(origin);
				}
			}
		}
		else if ( className == DataModel::StationMagnitude::ClassName() ) {
			DataModel::StationMagnitude* magnitude = DataModel::StationMagnitude::Cast(object);
			SEISCOMP_DEBUG("(%s) Handling object of type: %s with id: %s",
					_sinkName.c_str(), className.c_str(), magnitude->publicID().c_str());
			string parentID = (*notifierIt)->parentID();
			if ( hasOriginBeenSent(parentID) ) {
				if ( !Applications::findOrigin(parentID, tmpSentOrigins) ) {
					SEISCOMP_DEBUG("(%s) Relaying object of type: %s with id: %s",
						_sinkName.c_str(), className.c_str(), magnitude->publicID().c_str());
					IMEXMessage imexMessage;
					imexMessage.notifierMessage().attach(notifierIt->get());
					SEND_MSG(_routingTable[className], &imexMessage);
				}
			}
			else {
				ImEx::OriginList::const_iterator originIt = findOrigin(parentID);
				if ( originIt != _imex->originList().end() ) {
					if ( isOriginEligible(originIt->get()) ) {
						if ( hasEventBeenSent(originIt->get()) || filter(originIt->get()) ) {
							sendOrigin(originIt->get());
							tmpSentOrigins.push_back(originIt->get());
						}
					}
				}
			}
		}
		else if ( className == DataModel::StationMagnitudeContribution::ClassName() ) {
			// DataModel::StationMagnitudeContribution* magReference = DataModel::StationMagnitudeContribution::Cast(object);
			SEISCOMP_DEBUG("(%s) Handling object of type: %s",
			               _sinkName.c_str(), className.c_str());
			string magnitudeParentID = (*notifierIt)->parentID();
			DataModel::Magnitude* magnitude;
			magnitude = DataModel::Magnitude::Find(magnitudeParentID);
			string parentID;
			if ( magnitude ) {
				DataModel::Origin* origin;
				origin = magnitude->origin();
				if ( origin ) {
					parentID = origin->publicID();
				}
			}

			if ( hasOriginBeenSent(parentID) ) {
				if ( !Applications::findOrigin(parentID, tmpSentOrigins) ) {
					SEISCOMP_DEBUG("(%s) Relaying object of type: %s",
					               _sinkName.c_str(), className.c_str());
					IMEXMessage imexMessage;
					imexMessage.notifierMessage().attach(notifierIt->get());
					SEND_MSG(_routingTable[className], &imexMessage);
				}
			}
			else {
				ImEx::OriginList::const_iterator originIt = findOrigin(parentID);
				if ( originIt != _imex->originList().end() ) {
					if ( isOriginEligible(originIt->get()) ) {
						if ( hasEventBeenSent(originIt->get()) || filter(originIt->get()) ) {
							sendOrigin(originIt->get());
							tmpSentOrigins.push_back(originIt->get());
						}
					}
				}
			}
		}
		else if ( className == DataModel::Magnitude::ClassName() ) {
			DataModel::Magnitude* magnitude = DataModel::Magnitude::Cast(object);
			SEISCOMP_DEBUG("(%s) Handling object of type: %s with id: %s",
					_sinkName.c_str(), className.c_str(), magnitude->publicID().c_str());
			string parentID = (*notifierIt)->parentID();
			if ( hasOriginBeenSent(parentID) ) {
				if ( !Applications::findOrigin(parentID, tmpSentOrigins) ) {
					SEISCOMP_DEBUG("(%s) Relaying object of type: %s with id: %s",
					               _sinkName.c_str(), className.c_str(),
					               magnitude->publicID().c_str());
					IMEXMessage imexMessage;
					imexMessage.notifierMessage().attach(notifierIt->get());
					SEND_MSG(_routingTable[className], &imexMessage);
				}
			}
			else {
				ImEx::OriginList::const_iterator originIt = findOrigin(parentID);
				if ( originIt != _imex->originList().end() ) {
					if ( isOriginEligible(originIt->get()) ) {
						if ( hasEventBeenSent(originIt->get()) || filter(originIt->get()) ) {
							sendOrigin(originIt->get());
							tmpSentOrigins.push_back(originIt->get());
						}
					}
				}
			}
		}
		else if ( className == DataModel::Event::ClassName() ) {
			DataModel::Event* event  = DataModel::Event::Cast(object);
			SEISCOMP_DEBUG("(%s) Handling object of type: %s with id: %s",
			               _sinkName.c_str(), className.c_str(), event->publicID().c_str());
			SEISCOMP_DEBUG("   * preferredOriginID = %s", event->preferredOriginID().c_str());
			SEISCOMP_DEBUG("   * preferredMagnitudeID = %s", event->preferredMagnitudeID().c_str());
			if ( event ) {
				// handleEvent(event);
				EventList::iterator eventIt = _eventList.begin();
				for ( ; eventIt != _eventList.end(); ++eventIt)
					if (eventIt->publicID() == event->publicID())
						break;

				if ( eventIt != _eventList.end() ) {
					*eventIt = event;

					// If the preferred origin changed, send origin
					ImEx::OriginList::const_iterator originIt = findOrigin(event->preferredOriginID());
					if ( originIt != _imex->originList().end() ) {
						if ( hasEventBeenSent(originIt->get()) || filter(originIt->get()) ) {
							if ( !hasOriginBeenSent((*originIt)->publicID()) ) {
								sendOrigin(originIt->get());
								tmpSentOrigins.push_back(originIt->get());
							}
							sendEvent(*eventIt);
						}
					}
				}
				else {
					// Send whole event
					EventWrapper ew(event);
					_eventList.push_back(ew);
					ImEx::OriginList::const_iterator originIt = findOrigin(event->preferredOriginID());
					if ( originIt != _imex->originList().end() ) {
						if ( filter(originIt->get()) ) {
							sendOrigin(originIt->get());
							tmpSentOrigins.push_back(originIt->get());
							sendEvent(ew);
						}
					}
				}
			}
		}
		else {
			SEISCOMP_DEBUG("(%s) Received object: %s with notifier type: %s",
			               _sinkName.c_str(), className.c_str(),
			               (*notifierIt)->operation().toString());
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void ImExImpl::readSinkMessages() {
	while ( !_imex->isExitRequested() ) {
		while ( _sink->isConnected() ) {
			Core::MessagePtr msg = _sink->recv();
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
ImEx::OriginList::const_iterator ImExImpl::findOrigin(const DataModel::Origin *origin) {
	return findOrigin(origin->publicID());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
ImEx::OriginList::const_iterator ImExImpl::findOrigin(const string &publicID) {
	ImEx::OriginList::const_iterator it = _imex->originList().begin();
	for ( ; it != _imex->originList().end(); ++it ) {
		if ( (*it)->publicID() == publicID ) return it;
	}
	return _imex->originList().end();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool ImExImpl::hasOriginBeenSent(const string &originID) {
	for ( auto &&org : _sentOrigins )
		if ( org->publicID() == originID )
			return true;
	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool ImExImpl::hasEventBeenSent(const DataModel::Origin *origin) {
	SEISCOMP_DEBUG("Checking if event for origin %s has been sent",
	               origin->publicID().c_str());

	for ( auto &&wrapper : _eventList ) {
		if ( wrapper.preferredOriginID() == origin->publicID() ) {
			if ( wrapper.hasBeenSent() ) {
				SEISCOMP_DEBUG("Event %s has been sent", wrapper.publicID().c_str());
				return true;
			}
			else {
				SEISCOMP_DEBUG("Event %s has not been sent", wrapper.publicID().c_str());
				break;
			}
		}
	}
	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool ImExImpl::isOriginEligible(const DataModel::Origin* origin) {
	return (this->*isOriginEligibleImpl)(origin);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool ImExImpl::isOriginEligibleExport(const DataModel::Origin* origin) {
	SEISCOMP_DEBUG("(Export) Checking if origin %s is eligible", origin->publicID().c_str());
	if ( origin->magnitudeCount() == 0 || origin->stationMagnitudeCount() == 0 ) {
		SEISCOMP_DEBUG("Origin is not eligible (no mags or stamags)");
		return false;
	}

	SEISCOMP_DEBUG("Checking if origin is preferred");
	for ( auto &&wrapper : _eventList ) {
		if ( wrapper.preferredOriginID() == origin->publicID() )
			return true;
	}

	SEISCOMP_DEBUG("Origin is not eligible (not preferred)");
	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
bool ImExImpl::isOriginEligibleImport(const DataModel::Origin* origin) {
	SEISCOMP_DEBUG("(Import) Checking if origin %s is eligible", origin->publicID().c_str());
	if ( origin->magnitudeCount() == 0 /*|| origin->stationMagnitudeCount() == 0*/ ) {
		SEISCOMP_DEBUG("Origin is not eligible");
		return false;
	}
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<





// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool ImExImpl::filterMagnitudeImport(const DataModel::Origin* origin) {
	for ( size_t i = 0; i < origin->magnitudeCount(); ++i ) {
		DataModel::Magnitude* magnitude = origin->magnitude(i);
		if ( !magnitude )
			continue;
		if ( _criterion->isInMagnitudeRange(magnitude->magnitude().value()) ) {
			SEISCOMP_DEBUG("Magnitude of type %s with valus %f matched",
			               magnitude->type().c_str(), magnitude->magnitude().value());
			return true;
		}
		else {
			SEISCOMP_DEBUG("= Magnitude mismatch =");
			SEISCOMP_DEBUG("%s", _criterion->what().c_str());
			_criterion->clearError();
		}

	}
	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool ImExImpl::filterMagnitudeExport(const DataModel::Origin* origin) {
	string preferredMagnitude;
	EventList::iterator eventIt = _eventList.begin();
	for ( ; eventIt != _eventList.end(); ++eventIt) {
		if ( eventIt->preferredOriginID() == origin->publicID() ) {
			preferredMagnitude = eventIt->preferredMagnitudeID();
			break;
		}
	}
	if ( preferredMagnitude.empty() )
		return false;
	SEISCOMP_DEBUG("Preferred magnitude ID has been found");
	DataModel::Magnitude* magnitude = origin->findMagnitude(preferredMagnitude);
	if ( !magnitude )
		return false;
	SEISCOMP_DEBUG("Preferred magnitude has been found");
	if ( !_criterion->isInMagnitudeRange(magnitude->magnitude().value()) ) {
		SEISCOMP_DEBUG("= Magnitude mismatch =");
		_criterion->clearError();
		return false;
	}
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool ImExImpl::filterMagnitude(const DataModel::Origin* origin) {
	return (this->*filterMagnitudeImpl)(origin);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<





// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool ImExImpl::filter(DataModel::Origin* origin) {
	if ( !_filter )
		return true;

	SEISCOMP_DEBUG("Filtering origin: %s", origin->publicID().c_str());
	SEISCOMP_DEBUG("Checking latitude/longitude");
	if ( !_criterion->isInLatLonRange(origin->latitude(), origin->longitude()) ) {
		SEISCOMP_DEBUG("= latitude/longitude mismatch =");
		SEISCOMP_DEBUG("%s", _criterion->what().c_str());
		_criterion->clearError();
		return false;
	}

	// Arrival count
	SEISCOMP_DEBUG("Checking arrival count");
	if ( !_criterion->checkArrivalCount(origin->arrivalCount()) ) {
		SEISCOMP_DEBUG("Number of arrivals %ld is below the minimum", (long int)origin->arrivalCount());
		SEISCOMP_DEBUG("%s", _criterion->what().c_str());
		_criterion->clearError();
		return false;
	}

	// Magnitude
	SEISCOMP_DEBUG("Checking magnitude");
	if ( !filterMagnitude(origin) )
		return false;

	SEISCOMP_DEBUG("Checking agencyID");
	if ( !_criterion->checkAgencyID(objectAgencyID(origin)) ) {
		SEISCOMP_DEBUG("Could not find agencyID: %s", objectAgencyID(origin).c_str());
		_criterion->clearError();
		return false;
	}

	SEISCOMP_DEBUG("Origin passed filter");
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void ImExImpl::serializeMessage(const string &destination, Core::Message *message) {
	if ( _messageQueue.size() >= _maxQueueSize ) {
		SEISCOMP_DEBUG("(%s) message queue exceeded maximum size of %ld. Skipping message.",
		               _sinkName.c_str(), (long int)_maxQueueSize);
		return;
	}

	if ( destination == "NULL" ) {
		SEISCOMP_DEBUG("Destination for notifierMessage is NULL. Skipping message!");
		return;
	}

	SEISCOMP_DEBUG("Sending NotifierMessage containing %d notifier", message->size());
	if ( message->empty() ) {
		SEISCOMP_DEBUG("sendMessage: empty message rejected");
		return;
	}

	messageInfo(message);

	Client::PacketPtr packet = new Client::Packet;

	if ( !_conversion.empty() ) {
		IO::ExporterPtr exporter = IO::ExporterFactory::Create(_conversion.c_str());
		if ( !exporter.get() ) {
			SEISCOMP_ERROR("ImExImpl: Could not create importer for type %s", _conversion.c_str());
			return;
		}

		{	// Needs a own block. Otherwise the filtered buffer will not be flushed
			boost::iostreams::stream_buffer<boost::iostreams::back_insert_device<string> > buf(packet->payload);
			boost::iostreams::filtering_ostreambuf filteredBuf;
			filteredBuf.push(boost::iostreams::zlib_compressor());
			filteredBuf.push(buf);

			if ( !exporter->write(&filteredBuf, message) ) {
				SEISCOMP_ERROR("ImExImpl: Could not export message for %s", _conversion.c_str());
				return;
			}
		}
	}
	else {
		Client::Protocol::encode(packet->payload, message,
		                         _contentEncoding, _contentType,
		                         _sink->schemaVersion().packed);
	}

	packet->target = destination;

	_messageQueue.push(packet);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void ImExImpl::sendOrigin(DataModel::Origin *origin, bool update) {
	string originID = origin->publicID();
	SEISCOMP_DEBUG("Sending origin with id: %s", originID.c_str());
	DataModel::Notifier::Enable();

	IMEXMessage imexMessage;

	// Send picks
	for ( size_t i = 0; i < origin->arrivalCount(); ++i ) {
		auto arrival = origin->arrival(i);
		auto pickIt = _imex->pickList().begin();
		for ( ; pickIt != _imex->pickList().end(); ++pickIt ) {
			if ( arrival->pickID() == (*pickIt)->publicID() ) {
				auto it = find_if(
					_sentPicks.begin(), _sentPicks.end(),
					[arrival](DataModel::Pick *pick) -> bool {
						return arrival->pickID() == pick->publicID();
					}
				);

				if ( it == _sentPicks.end() ) {
					_sentPicks.push_back((*pickIt).get());
					imexMessage.notifierMessage().attach(
						new DataModel::Notifier(
							DataModel::EventParameters::ClassName(),
							DataModel::OP_ADD, pickIt->get()
						)
					);
				}
				break;
			}
		}
	}
	SEISCOMP_DEBUG("Sending %d %s to %s in respect to origin %s",
	               imexMessage.size(), DataModel::Pick::ClassName(),
	               _routingTable[DataModel::Pick::ClassName()].c_str(),
	               originID.c_str());
	SEND_MSG(_routingTable[DataModel::Pick::ClassName()], &imexMessage);
	imexMessage.clear();

	// Attach origin
	if ( update ) {
		imexMessage.notifierMessage().attach(
			new DataModel::Notifier(
				DataModel::EventParameters::ClassName(), DataModel::OP_UPDATE, origin
			)
		);
	}
	else {
		imexMessage.notifierMessage().attach(
			new DataModel::Notifier(
				DataModel::EventParameters::ClassName(), DataModel::OP_ADD, origin
			)
		);
	}

	const string &originTarget = _routingTable[DataModel::Origin::ClassName()];
	const string &arrivalTarget = _routingTable[DataModel::Arrival::ClassName()];
	bool separateOrigin = originTarget != arrivalTarget;

	// Send origin
	if ( separateOrigin ) {
		SEISCOMP_DEBUG("Sending %sorigin %s to %s",
		               update?"update for ":"",
		               originID.c_str(), originTarget.c_str());
		SEND_MSG(originTarget, &imexMessage);
		imexMessage.clear();
	}

	// Attach arrivals
	if ( !update ) {
		for ( size_t i = 0; i < origin->arrivalCount(); ++i ) {
			imexMessage.notifierMessage().attach(
				new DataModel::Notifier(
					origin->publicID(), DataModel::OP_ADD, origin->arrival(i)));
		}

		if ( separateOrigin )
			SEISCOMP_DEBUG("Sending %d %s for origin %s to %s",
			               imexMessage.size(), DataModel::Arrival::ClassName(),
			               originID.c_str(), originTarget.c_str());
		else
			SEISCOMP_DEBUG("Sending %d %s and origin %s to %s",
			               imexMessage.size(), DataModel::Arrival::ClassName(),
			               originID.c_str(), originTarget.c_str());

		SEND_MSG(arrivalTarget, &imexMessage);
	}
	else {
		SEISCOMP_DEBUG("Sending update for origin %s to %s", originID.c_str(),
		               originTarget.c_str());
		SEND_MSG(originTarget, &imexMessage);
	}

	imexMessage.clear();

	if ( !update ) {
		// Send StationMagnitudes
		int amplitudeCount = 0;
		for ( size_t i = 0; i < origin->stationMagnitudeCount(); ++i ) {
			// Send Amplitudes first
			auto stationMagnitude = origin->stationMagnitude(i);
			auto saIt = _imex->amplitudeList().begin();
			for ( ; saIt != _imex->amplitudeList().end(); ++saIt) {
				if ( stationMagnitude->amplitudeID() == (*saIt)->publicID() ) {
					auto it = find_if(
						_sentAmplitudes.begin(),
						_sentAmplitudes.end(),
						[stationMagnitude](DataModel::Amplitude *amplitude) -> bool {
							return stationMagnitude->amplitudeID() == amplitude->publicID();
						}
					);
					if ( it == _sentAmplitudes.end() ) {
						++amplitudeCount;
						_sentAmplitudes.push_back((*saIt).get());
						imexMessage.notifierMessage().attach(
							new DataModel::Notifier(
								DataModel::EventParameters::ClassName(),
								DataModel::OP_ADD,
								saIt->get()
							)
						);
					}
					break;
				}
			}

			imexMessage.notifierMessage().attach(
				new DataModel::Notifier(
					origin->publicID(), DataModel::OP_ADD, stationMagnitude
				)
			);
		}

		SEISCOMP_DEBUG("Sending %ld %s and %d %s to %s in respect to origin %s",
		               (long int)origin->stationMagnitudeCount(),
		               DataModel::StationMagnitude::ClassName(),
		               amplitudeCount, DataModel::Amplitude::ClassName(),
		               _routingTable[DataModel::StationMagnitude::ClassName()].c_str(),
		               originID.c_str());

		SEND_MSG(_routingTable[DataModel::StationMagnitude::ClassName()], &imexMessage);
		imexMessage.clear();

		const string &staMagContribTarget = _routingTable[DataModel::StationMagnitudeContribution::ClassName()];
		bool sendStaMagContribs = staMagContribTarget != "NULL";

		// send NetworkMagnitudes
		size_t stationMagnitudeContributionCount = 0;
		for ( size_t i = 0; i < origin->magnitudeCount(); ++i ) {
			DataModel::Magnitude* magnitude = origin->magnitude(i);
			imexMessage.notifierMessage().attach(
					new DataModel::Notifier(
						origin->publicID(), DataModel::OP_ADD, magnitude
					)
			);

			if ( !sendStaMagContribs ) continue;

			stationMagnitudeContributionCount += magnitude->stationMagnitudeContributionCount();
			for ( size_t j = 0; j < magnitude->stationMagnitudeContributionCount(); ++j ) {
				imexMessage.notifierMessage().attach(
					new DataModel::Notifier(
						magnitude->publicID(),
						DataModel::OP_ADD,
						magnitude->stationMagnitudeContribution(j)
					)
				);
			}
		}

		SEISCOMP_DEBUG("Sending %ld %s and %ld %s to %s in respect to origin %s",
		               (long int)origin->magnitudeCount(),
		               DataModel::Magnitude::ClassName(),
		               (long int)stationMagnitudeContributionCount,
		               DataModel::StationMagnitudeContribution::ClassName(),
		               _routingTable[DataModel::Magnitude::ClassName()].c_str(),
		               originID.c_str()
		);

		SEND_MSG(_routingTable[DataModel::Magnitude::ClassName()], &imexMessage);
		imexMessage.clear();

		_sentOrigins.push_back(origin);
	}

	DataModel::Notifier::Disable();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void ImExImpl::sendEvent(EventWrapper& eventWrapper) {
	SEISCOMP_DEBUG("Sending %s to %s in respect to origin %s",
	               DataModel::Event::ClassName(),
	               _routingTable[DataModel::Event::ClassName()].c_str(),
	               eventWrapper.preferredOriginID().c_str());

	Core::DataMessage dataMessage;
	dataMessage.attach(eventWrapper.event());
	sendMessage(_routingTable[DataModel::Event::ClassName()], &dataMessage);
	eventWrapper.setHasBeenSent(true);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void ImExImpl::sendMessage(const string& destination, Core::Message *message) {
	(this->*sendMessageImpl)(destination, message);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void ImExImpl::sendMessageImport(const string& destination, Core::Message *message) {
	// The actual sending of the message happens in a separate thread
	serializeMessage(destination, &(reinterpret_cast<IMEXMessage*>(message)->notifierMessage()));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void ImExImpl::sendMessageExport(const string& destination, Core::Message *message) {
	// The actual sending of the message happens in a separate thread
	serializeMessage(destination, message);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void ImExImpl::sendMessageRaw() {
	while ( _isRunning ) {
		try {
			Client::PacketPtr packet = _messageQueue.pop();
			Client::Result ret;

			while ( (ret = _sink->protocol()->sendData(packet->target.c_str(),
			                                           packet->payload.c_str(),
			                                           packet->payload.size(),
			                                           Client::Protocol::Regular,
			                                           _contentEncoding,
			                                           _contentType)) != Client::OK ) {
				SEISCOMP_ERROR("(%s) Could not send message to %s on sink master %s due to error %d",
				               _sinkName.c_str(), packet->target.c_str(),
				               _sink->source().c_str(), ret.toInt());

				if ( !_isRunning ) break;

				if ( !_sink->isConnected() ) {
					SEISCOMP_ERROR("(%s) Trying to reconnect to %s", _sinkName.c_str(), _sink->source().c_str());
					_sink->reconnect();
				}

				Core::sleep(1);
			}

			SEISCOMP_DEBUG("(%s) Sent message to %s", _sinkName.c_str(), packet->target.c_str());
		}
		catch ( Core::GeneralException& ex ) {
			SEISCOMP_INFO("(%s) Exception: %s, returning", _sinkName.c_str(), ex.what());
			break;
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<





// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool ImExImpl::configGetRoutingTable(const string& prefix, const string& name, RoutingTable* routingTable) {
	try {
		vector<string> routingTableEntries = _imex->configGetStrings(prefix + "." + name);
		if ( !fillRoutingTable(routingTableEntries, *routingTable) )
			return false;
	}
	catch ( Config::Exception& e ) {
		SEISCOMP_DEBUG("(%s) %s ", e.what(), _sinkName.c_str());
		return false;
	}
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<





// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
} // namespace Applictions
} // namespace Seiscomp
