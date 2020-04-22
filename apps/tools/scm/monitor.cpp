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


#define SEISCOMP_COMPONENT ScMonitor
#include <seiscomp/logging/log.h>

#include "monitor.h"

#include <sstream>
#include <iterator>

#include <seiscomp/core/strings.h>


namespace Seiscomp {
namespace Applications {
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
#define DEFINE_PLUGIN_ITERATOR(Interface) \
	PluginIterator<Interface, Interface##Ptr, Interface##Factory>

#define DEFINE_PLUGIN_ITERATOR_PARAM(Interface) \
	PluginIterator<Interface, Interface##Ptr, Interface##Factory>

#define DEFINE_PLUGIN_ITERATOR_ITERATOR(Interface) \
	PluginIterator<Interface, Interface##Ptr, Interface##Factory>::iterator

#define DEFINE_PLUGIN_ITERATOR_CITERATOR(Interface) \
	PluginIterator<Interface, Interface##Ptr, Interface##Factory>::const_iterator


template <typename Interface, typename InterfacePtr, typename InterfaceFactory>
class PluginIterator {
	public:
		typedef std::vector<Interface*> Plugins;
		typedef typename Plugins::iterator iterator;
		typedef typename Plugins::const_iterator const_iterator;


	public:
		PluginIterator(const char* name)
		 : _name(name) {
			_operational = init();
		}

		PluginIterator(const char* name, const std::vector<std::string>& pluginNames)
		 : _name(name) {
			_operational = init(pluginNames.begin(), pluginNames.end());
		}

		bool isOperational() { return _operational; }

		iterator begin() { return _plugins.begin(); }
		iterator end() { return _plugins.end(); }
		const_iterator begin() const { return _plugins.begin(); }
		const_iterator end() const { return _plugins.end(); }

		std::vector<std::string>::iterator namesBegin() { return _pluginNames.begin(); }
		std::vector<std::string>::iterator namesEnd() { return _pluginNames.end(); }
		std::vector<std::string>::const_iterator namesBegin() const { return _pluginNames.begin(); }
		std::vector<std::string>::const_iterator namesEnd() const { return _pluginNames.end(); }


	private:
		bool init() {
			std::unique_ptr<typename InterfaceFactory::ServiceNames> tmpPluginNames;
			tmpPluginNames = std::unique_ptr<typename InterfaceFactory::ServiceNames>(InterfaceFactory::Services());
			if ( !tmpPluginNames.get() ) {
				SEISCOMP_INFO("%s hosts no services", _name);
				return false;
			}
			return init(tmpPluginNames->begin(), tmpPluginNames->end());
		}

		template <typename NameIterator>
		bool init(NameIterator begin, NameIterator end) {
			for ( ; begin != end; ++begin ) {
				_pluginNames.push_back(*begin);

				Interface* obj = InterfaceFactory::Create((*begin).c_str());
				if ( !obj ) {
					SEISCOMP_ERROR("Could not create plugin: %s", (*begin).c_str());
					return false;
				}
				_plugins.push_back(obj);
			}
			return true;
		}


	private:
		 std::vector<std::string> _pluginNames;
		 Plugins _plugins;
		 const char* _name;
		 bool _operational;

};
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Monitor::Monitor(int argc, char** argv)
 : Client::Application(argc, argv),
   _refreshInterval(3) {

	setDatabaseEnabled(false, false);
	// addLoggingComponentSubscription("ScMonitor");
	setLoadConfigModuleEnabled(false);
	setMembershipMessagesEnabled(true);
	setMessagingUsername("");
	setPrimaryMessagingGroup(Client::Protocol::LISTENER_GROUP);
	addMessagingSubscription(Client::Protocol::STATUS_GROUP);
	addPluginPackagePath("monitor");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Monitor::~Monitor() {
	for ( size_t i = 0; i < _outPlugins.size(); ++i )
		_outPlugins[i]->deinitOut();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Monitor::createCommandLineDescription() {
	Application::createCommandLineDescription();
	System::CommandLine &cl = commandline();
	std::string clGroupName = "monitor";
	cl.addGroup(clGroupName.c_str());
	cl.addOption(clGroupName.c_str(), "clients,c", "Just monitor given clients", &_clientsToConsiderStr);
	cl.addOption(clGroupName.c_str(), "print-tags", "Print available keys for accessing client info data");
	cl.addOption(clGroupName.c_str(), "no-output-plugins", "Do not use output plugins");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Monitor::validateParameters() {
	if ( commandline().hasOption("print-tags") ) {
		printClientInfoTags();
		return false;
	}
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Monitor::printClientInfoTags() const {
	std::cout << "= Client info tags =" << std::endl;
	for ( size_t i = 0; i < Client::Status::ETagQuantity; ++i ) {
		std::cout << Client::Status::ETagNames::name(i) << std::endl;
	}
	std::cout << std::endl;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Monitor::init() {
	if ( !Application::init() ) return false;

	if ( !_clientsToConsiderStr.empty() ) {
		Core::split(_clientsToConsider, _clientsToConsiderStr.c_str(), ",");
		for ( auto && str : _clientsToConsider )
			Core::trim(str);
	}

	// Initialize output plugins
	if ( !commandline().hasOption("no-output-plugins") ) {
		DEFINE_PLUGIN_ITERATOR(MonitorOutPluginInterface) outPluginFactory("MonitorOutPluginInterfaceFactory");
		if ( outPluginFactory.isOperational() ) {
			for ( auto && interface : outPluginFactory )
				_outPlugins.push_back(interface);
		}
	}

	DEFINE_PLUGIN_ITERATOR(MonitorPluginInterface) pluginFactory("MonitorPluginInterfaceFactory");
	if ( pluginFactory.isOperational() ) {
		for ( auto && interface : pluginFactory ) {
			if ( interface->init(configuration()) )
				_plugins.push_back(interface);
			else
				SEISCOMP_ERROR("Plugin %s not operational, skipped", interface->name().c_str());
		}
	}

	for ( size_t i = 0; i < _outPlugins.size(); ++i )
		_outPlugins[i]->initOut(configuration());

	update();
	enableTimer(_refreshInterval);

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Monitor::handleNetworkMessage(const Client::Packet *msg) {
	if ( !_clientsToConsider.empty() ) {
		auto found = std::find(_clientsToConsider.begin(),
		                       _clientsToConsider.end(),
		                       msg->sender);
		if ( found == _clientsToConsider.end() ) return;
	}

	if ( msg->type == Client::Packet::Status ) {
		handleStatusMessage(msg);

		for ( size_t i = 0; i < _plugins.size(); ++i )
			_plugins[i]->process(_clientTable);

		for ( auto && client : _clientTable )
			client.processed = true;
	}
	else if ( msg->type == Client::Packet::Disconnected ) {
		auto it = _clientTable.begin();
		for ( ; it != _clientTable.end(); ++it ) {
			if ( it->info[Client::Status::Clientname] == msg->subject )
				break;
		}

		if ( it != _clientTable.end() ) {
			ResponseTimes::iterator responseTimesIt = _responseTimes.find(msg->subject);
			if ( responseTimesIt != _responseTimes.end() )
				_responseTimes.erase(responseTimesIt);
			_clientTable.erase(it);
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Monitor::handleDisconnect() {
	_clientTable.clear();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Monitor::handleStatusMessage(const Client::Packet *msg) {
	Client::Status status;

	if ( !status.parse(msg->payload) )
		return;

	status.values[Client::Status::Clientname] = msg->subject;

	_responseTimes[status.values[Client::Status::Clientname]] = Core::Time::GMT();

	for ( auto &&client : _clientTable ) {
		client.info[Client::Status::ResponseTime] = "0";
		if ( client.info[Client::Status::Clientname] == msg->subject ) {
			client = status;
			return;
		}
	}

	_clientTable.push_back(ClientTableEntry(status));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Monitor::handleTimeout() {
	update();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Monitor::update() {
	// Update response time
	for ( auto && client : _clientTable )
		client.info[Client::Status::ResponseTime] =
			Core::toString<int>((Core::Time::GMT() - _responseTimes[client.info[Client::Status::Clientname]]).seconds());

	for ( auto && plugin : _outPlugins )
		plugin->print(_clientTable);

	for ( auto && client : _clientTable )
		client.printed = true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<





} // namespace Applications
} // namespace Seiscomp

