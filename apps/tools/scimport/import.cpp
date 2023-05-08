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


#define SEISCOMP_COMPONENT ScImport
#include "import.h"

#include <functional>
#include <vector>

#include <seiscomp/logging/log.h>
#include <seiscomp/core/strings.h>
#include <seiscomp/core/system.h>
#include <seiscomp/system/environment.h>
#include <seiscomp/core/datamessage.h>
#include <seiscomp/utils/timer.h>
#include <seiscomp/core/datetime.h>
#include <seiscomp/core/system.h>
#include <seiscomp/utils/files.h>

#include "filter.h"


using namespace std;


namespace Seiscomp {
namespace Applications {
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Import::Import(int argc, char* argv[])
: Client::Application(argc, argv)
, _sinkMessageThread(NULL)
, _filter(true)
, _routeUnknownGroup(false)
, _mode(RELAY)
, _useSpecifiedGroups(true)
, _test(false) {
	setRecordStreamEnabled(false);
	setDatabaseEnabled(false, false);
	setPrimaryMessagingGroup(Client::Protocol::LISTENER_GROUP);
	setMessagingUsername(Util::basename(argv[0]));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Import::~Import() {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<





// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Import::init() {
	if ( !Application::init() ) return false;

	DataModel::PublicObject::SetRegistrationEnabled(false);

	if ( !Filter::Init(*this) ) return false;

	if ( commandline().hasOption("test") )
		_test = true;

	if ( commandline().hasOption("no-filter") )
		_filter = false;

	if ( commandline().hasOption("import") )
		_mode = IMPORT;

	if ( commandline().hasOption("routeunknowngroup") )
		_routeUnknownGroup = true;

	if ( commandline().hasOption("ignore-groups") )
		_useSpecifiedGroups = false;

	try {
		configGetString("routingtable");
		_mode = IMPORT;
	}
	catch ( ... ) {}

	string sinkName = "localhost";
	if ( commandline().hasOption("sink") ) {
		sinkName = commandline().option<string>("sink");
	}
	else {
		try {
			sinkName = configGetString("sink");
		}
		catch ( Config::Exception & ) {}
	}

	if ( connectToSink(sinkName) != Client::OK )
		return false;

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Import::initConfiguration() {
	if (!Application::initConfiguration())
		return false;

	try {
		_filter = configGetBool("useFilter");
	}
	catch ( ... ) {}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Import::done() {
	if ( _sink ) _sink->disconnect();

	Client::Application::done();

	if ( _sinkMessageThread ) {
		SEISCOMP_DEBUG("Waiting for sink message thread");
		_sinkMessageThread->join();
		delete _sinkMessageThread;
		_sinkMessageThread = NULL;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Import::createCommandLineDescription() {
	Application::createCommandLineDescription();
	System::CommandLine &cl = commandline();
	string clGroupName = "scimport";
	cl.addGroup(clGroupName.c_str());
	cl.addOption(clGroupName.c_str(), "sink,o", "Sink master", static_cast<string*>(NULL));
	cl.addOption(clGroupName.c_str(), "import,i", "Switch to import mode (default is relay)\n"
	                                  "import mode: You have your own routing table specified\n"
	                                  "relay mode: The routing table will be calculated automatically");
	cl.addOption(clGroupName.c_str(), "no-filter", "Don't filter messages");
	cl.addOption(clGroupName.c_str(), "routeunknowngroup", "Route unknown groups to the default group IMPORT_GROUP");
	cl.addOption(clGroupName.c_str(), "ignore-groups", "Ignore user specified groups");
	cl.addOption(clGroupName.c_str(), "test", "Do not send any messages");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Client::Result Import::connectToSink(const string &sink) {
	Client::Result r;

	_sink = new Client::Connection();
	r = _sink->setSource(sink);
	if ( r != Client::OK ) {
		SEISCOMP_ERROR("Invalid sink URL: %s", sink.c_str());
		return r;
	}

	// Connect to the sink master and use a default name
	bool first = true;
	while ( (r = _sink->connect("", Client::Protocol::IMPORT_GROUP)) !=
			Client::OK && !_exitRequested ) {
		if ( first ) {
			SEISCOMP_WARNING("Could not connect to the sink master %s : %s, trying again every 2s",
			                 sink.c_str(), r.toString());
			first = false;
		}

		Core::sleep(2);
	}

	if ( r != Client::OK )
		return r;

	// Get rid of data messages and read commands that are may send.
	SEISCOMP_INFO("Successfully connected to sink master: %s", sink.c_str());

	// Build routing table
	if ( _mode == RELAY )
		buildRelayRoutingtable(_routeUnknownGroup);
	else if ( _mode == IMPORT ) {
		if ( !buildImportRoutingtable() ) {
			SEISCOMP_ERROR("Could not built routing table for IMPORT mode.\nThere are no routing entries specified in the configuration file");
			return Client::Error;
		}
	}
	else {
		SEISCOMP_ERROR("Unknown import mode: %i", static_cast<int>(_mode));
		return Client::Error;
	}

	_sinkMessageThread = new thread(bind(&Import::readSinkMessages, this));

	// Print routing table
	for ( auto it : _routingTable )
		SEISCOMP_INFO("%s@%s -> %s@%s",
		              it.first.c_str(),
		              connection()->source().c_str(),
		              it.second.c_str(), sink.c_str());

	// Subscribe to source message groups
	for ( auto it :  _routingTable ) {
		if ( connection()->subscribe(it.first) != Client::OK )
			SEISCOMP_INFO("Could subscribe to group: %s", it.first.c_str());
	}

	return Client::OK;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Import::handleNetworkMessage(const Client::Packet *packet) {
	// Store the last packet as its meta data are needed
	_lastPacket = packet;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Import::handleMessage(Core::Message *msg) {
	assert(_lastPacket);
	auto it = _routingTable.find(_lastPacket->target);
	if ( it == _routingTable.end() ) return;

	if ( _filter ) {
		if ( Core::DataMessage* dataMessage = Core::DataMessage::Cast(msg) ) {
			Core::DataMessage::iterator it = dataMessage->begin();
			while ( it != dataMessage->end() ) {
				if ( filterObject((*it).get()) )
					it = dataMessage->detach(it);
				else
					++it;
			}

		}
		else if ( DataModel::NotifierMessage* notifierMessage = DataModel::NotifierMessage::Cast(msg) ) {
			// Process Notifier Message
			DataModel::NotifierMessage::iterator it = notifierMessage->begin();
			while ( it != notifierMessage->end() ) {
				if ( filterObject((*it)->object()) )
					it = notifierMessage->detach(it);
				else
					++it;
			}
		}
		else {
			// Unknown message
			return;
		}

		if ( msg->empty() ) return;
	}

	/*
	SEISCOMP_INFO(
		"Message (type: %d) from source %s with destination %s relayed to group %s on sink %s",
		newNetworkMessagePtr->type(), networkMessage->clientName().c_str(), networkMessage->destination().c_str(),
		it->second.c_str(), _sink->masterAddress().c_str());
	*/

	if ( !_test ) {
		bool first = true;
		Client::Result r;
		while ( (r = _sink->sendMessage(it->second, msg)) !=
				Client::OK && !_exitRequested ) {
			switch ( r.code() ) {
				case Client::GroupDoesNotExist:
					SEISCOMP_WARNING("Sink group %s does not exist, message ignored",
					                 it->second.c_str());
					return;

				case Client::MessageTooLarge:
					SEISCOMP_WARNING("Sink says: message is too large, message ignored");
					return;
				default:
					break;
			}

			if ( first ) {
				SEISCOMP_WARNING("Sending message to %s failed, waiting for reconnect",
				                 _sink->source().c_str());
				first = false;
			}

			Core::sleep(2);
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Import::filterObject(Core::BaseObject* obj) {
	Filter* filter = Filter::GetFilter(obj->className());
	if ( filter ) {
		if ( !filter->filter(obj) ) return true;
	}
	else
		SEISCOMP_DEBUG("Filter for class: %s not available", obj->className());

	return false;
}
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Import::buildImportRoutingtable() {
	// Build routing table
	SEISCOMP_INFO("Calculating routing table ...");
	try {
		vector<string> tmpRoutingTable = configGetStrings("routingtable");

		for ( const auto &item : tmpRoutingTable ) {
			vector<string> tokens;
			Core::split(tokens, item.c_str(), ":");
			if ( tokens.size() != 2 )
				SEISCOMP_INFO("Malformed routing table entry: %s", item.c_str());
			else
				_routingTable[tokens[0]] = tokens[1];
		}
	}
	catch ( Config::Exception& e ) {
		SEISCOMP_ERROR("%s", e.what());
		return false;
	}

	set<string> sourceGroups;
	if ( _useSpecifiedGroups ) {
		try {
			vector<string> v = configGetStrings("msggroups");
			sourceGroups.insert(v.begin(), v.end());
		}
		catch ( Config::Exception& ) {
			SEISCOMP_ERROR("No message groups defined, see @msggroups");
			exit(0);
		}
	}
	else {
		Client::Connection *con = connection();
		if ( con )
			sourceGroups = con->protocol()->groups();
	}

	for ( auto it = _routingTable.begin(); it != _routingTable.end(); ) {
		if ( find(sourceGroups.begin(), sourceGroups.end(), it->first) == sourceGroups.end() ) {
			SEISCOMP_ERROR("Group %s is not a member of available source groups. Deleting routing entry", it->first.c_str());
			_routingTable.erase(it++);
		}
		else
			++it;
	}

	/*
	vector<string> sinkGroups = _sink->groups();
	for ( it = _routingTable.begin(); it != _routingTable.end(); ) {
		if ( find(sinkGroups.begin(), sinkGroups.end(), it->second) == sinkGroups.end() ) {
			SEISCOMP_ERROR("Group %s is not a member of available sink groups. Deleting routing entry", it->second.c_str());
			tmp = it;
			++it;
			_routingTable.erase(tmp);
		}
		else
			++it;
	}
	*/

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<





// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Import::buildRelayRoutingtable(bool routeUnknownGroup) {
	SEISCOMP_INFO("Calculating default routing table ...");
	set<string> sourceGroups;

	if ( _useSpecifiedGroups ) {
		try {
			vector<string> v = configGetStrings("msggroups");
			sourceGroups.insert(v.begin(), v.end());
		}
		catch ( Config::Exception& ) {
			SEISCOMP_ERROR("No message groups defined, see @msggroups");
			exit(0);
		}
	}
	else {
		Client::Connection* con = connection();
		if ( con )
			sourceGroups = connection()->protocol()->groups();
	}

	set<string> sinkGroups = _sink->protocol()->groups();

	for( const auto &item : sourceGroups ) {
		auto found = find(sinkGroups.begin(), sinkGroups.end(), item);
		if ( found != sinkGroups.end() )
			_routingTable[item] = *found;
		else if( routeUnknownGroup )
			_routingTable[item] = Client::Protocol::IMPORT_GROUP;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Import::readSinkMessages() {
	while ( !_exitRequested ) {
		Core::MessagePtr msg;
		msg = _sink->recv();
		if ( _sink->lastError() != Client::OK ) {
			bool first = true;

			while ( !_exitRequested ) {
				if ( first )
					SEISCOMP_WARNING("Connection lost to sink, trying to reconnect");

				_sink->reconnect();
				if ( _sink->isConnected() ) {
					SEISCOMP_INFO("Reconnected successfully to sink");
					break;
				}
				else {
					if ( first ) {
						first = false;
						SEISCOMP_INFO("Reconnecting to sink failed, trying again every 2 seconds");
					}
					Core::sleep(2);
				}
			}
		}
	}

	SEISCOMP_INFO("Leaving sink message thread");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
} // namespace Applictions
} // namespace Seiscomp
