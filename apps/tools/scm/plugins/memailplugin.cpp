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

#include "memailplugin.h"

#include <iostream>
#include <algorithm>

#include <seiscomp/core/plugin.h>
#include <seiscomp/core/system.h>
#include <seiscomp/system/hostinfo.h>
#include <seiscomp/client/application.h>


using namespace std;


namespace Seiscomp {
namespace Applications {




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EmailMessage::EmailMessage() : _hasChanged(false) { }
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EmailMessage::setHeader(const string& header)
{
	_header.assign(header);
	_hasChanged = true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EmailMessage::setHeaderFilteredClients(const string& header)
{
	_headerFilteredClients.assign(header);
	_hasChanged = true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EmailMessage::setHeaderRequiredClients(const string& header)
{
	_headerRequiredClients.assign(header);
	_hasChanged = true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EmailMessage::setHeaderSilentClients(const string& header)
{
	_headerSilentClients.assign(header);
	_hasChanged = true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EmailMessage::setFilteredClients(const vector<string>& clients)
{
	_hasChanged = true;
	if ( !_filteredClients.empty() )
		_filteredClients.clear();
	for ( size_t i = 0; i < clients.size(); ++i ) {
		_filteredClients.append(clients[i] + "\n--\n");
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EmailMessage::setSilentClients(
		const vector<string>& silentClients,
		const vector<string>& recoveredClients)
{
	_hasChanged = true;
	if ( !_silentClients.empty() )
		_silentClients.clear();
	for ( size_t i = 0; i < silentClients.size(); ++i )
		_silentClients.append(silentClients[i] + "\n");
	for ( size_t i = 0; i < recoveredClients.size(); ++i ) {
		_silentClients.append(recoveredClients[i] + "\n");
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EmailMessage::setRequiredClients(
		const vector<string>& missing,
		const vector<string>& reappeared)
{
	_hasChanged = true;
	if ( !_requiredClients.empty() )
		_requiredClients.clear();
	for ( size_t i = 0; i < missing.size(); ++i ) {
		_requiredClients.append(missing[i] + "\n");
	}
	for ( size_t i = 0; i < reappeared.size(); ++i ) {
		_requiredClients.append(reappeared[i] + "\n");
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EmailMessage::setUserData(const string& data)
{
	_hasChanged = true;
	_userData.assign(data);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const string& EmailMessage::message()
{
	if ( _hasChanged ) {
		_hasChanged = !_hasChanged;
		_message.clear();
		string doubleSpacer = "\n\n";
		string singleSpacer = "\n";
		_message.append(_header);
		_message.append(Core::Time::GMT().toString("%a, %d %b %Y %H:%M:%S"));
		_message.append(doubleSpacer);

		if ( !_filteredClients.empty() ) {
			_message.append(_headerFilteredClients + doubleSpacer);
			_message.append(_filteredClients + singleSpacer);
		}
		if ( !_requiredClients.empty() ) {
			_message.append(_headerRequiredClients + doubleSpacer);
			_message.append(_requiredClients + singleSpacer);
		}
		if ( !_silentClients.empty() ) {
			_message.append(_headerSilentClients + doubleSpacer);
			_message.append(_silentClients + singleSpacer);
		}
		if ( !_userData.empty() )
			_message.append(_userData);
	}
	return _message;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool EmailMessage::empty()
{
	bool empty =
		_filteredClients.empty() &&
		_silentClients.empty() &&
		_requiredClients.empty() &&
		_message.empty();
	return empty;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void EmailMessage::clear()
{
	if ( empty() ) return;

	_message.clear();
	_filteredClients.clear();
	_silentClients.clear();
	_requiredClients.clear();

	_hasChanged = true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EmailSender* EmailSender::Create()
{
	EmailSender* sender = new EmailSender;
	return sender;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool EmailSender::sendEmail(const string& text, const string& recipient)
{
	SEISCOMP_DEBUG("Sending email to: %s", recipient.c_str());
	ostringstream command;
	command << "echo \'" << text << "\'  | mailx -s \'scm notification\' " << recipient << endl;
	SEISCOMP_DEBUG("%s", command.str().c_str());
	return Core::system(command.str());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<





// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
template <Client::Status::ETag tag>
inline const string printFilterMatch(const ClientInfoData& data)
{
	ClientInfoData::const_iterator clientIt = data.find(tag);
	string text;
	if ( clientIt != data.end() ) {
		text = string(Client::Status::Tag(tag).toString())
		+ " = " + clientIt->second;
		SEISCOMP_DEBUG("%s", text.c_str());
	}
	return text;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
IMPLEMENT_SC_CLASS_DERIVED(MEmailPlugin, MonitorPluginInterface, "MEmailPlugin");
REGISTER_MONITOR_PLUGIN_INTERFACE(MEmailPlugin, "memailplugin");

ADD_SC_PLUGIN(
	"monitor plugin for email notifications",
	"GFZ Potsdam <seiscomp-devel@gfz-potsdam.de>", 1, 0, 0
)
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
MEmailPlugin::MEmailPlugin() :
	MonitorPluginInterface("memailplugin"),
	_filterResponseInterval(60),
	_requiredClientsTimeMarker(Core::Time::GMT()),
	_reportRequiredClientsTimeSpan(5 * 60),
	_reportSilentClients(true),
	_reportSilentClientsTimeSpan(60), // 60 sec.
	_sendEmail(false)
{}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool MEmailPlugin::init(const Config::Config &cfg) {
	if ( !MonitorPluginInterface::init(cfg) ) return false;
	try {
		vector<string> recipients = cfg.getStrings(name() + ".recipients");
		addRecipients(recipients);
	}
	catch( Config::Exception& e ) {
		SEISCOMP_ERROR("MEmailPlugin could not be initialized due to missing recipients list: %s", e.what());
		setOperational(false);
	}

	try {
		_template = cfg.getString(name() + ".template");
	}
	catch ( Config::Exception& e ) {
		SEISCOMP_DEBUG("%s", e.what());
	}

	try {
		vector<string> requiredClients = cfg.getStrings(name() + ".requiredClients");
		for (size_t i = 0; i < requiredClients.size(); ++i) {
			_requiredClients.insert(make_pair(requiredClients[i], false));
		}
	}
	catch ( Config::Exception& e ) {
		SEISCOMP_DEBUG("%s", e.what());
	}

	try {
		_reportSilentClients = cfg.getBool(name() + ".reportSilentClients");
	}
	catch ( Config::Exception& e ) {
		SEISCOMP_DEBUG("%s", e.what());
	}

	try {
		_reportSilentClientsTimeSpan =
			Core::TimeSpan(cfg.getDouble(name() + ".reportSilentClientsTimeSpan") * 60);
	}
	catch ( Config::Exception& e ) {
		SEISCOMP_DEBUG("%s", e.what());
	}

	try {
		double filterMeanInterval = cfg.getDouble(name() + ".filterMeanInterval");
		setFilterMeanInterval(filterMeanInterval);
	}
	catch (Config::Exception& e) {
		SEISCOMP_DEBUG("%s", e.what());
	}

	try {
		_reportRequiredClientsTimeSpan = cfg.getDouble(name() + ".reportRequiredClients") * 60;
	}
	catch ( Config::Exception& e ) {
		SEISCOMP_DEBUG("%s", e.what());
	}

	try {
		_sendEmail = cfg.getBool(name() + ".sendEmail");
	}
	catch ( Config::Exception& e ) {
		SEISCOMP_DEBUG("%s", e.what());
	}

	_sender= unique_ptr<EmailSender>(EmailSender::Create());
	if ( !_sender.get() ) {
		SEISCOMP_ERROR("MEmailPlugin could not be initialized. Email service not available!");
		setOperational(false);
	}

	stringstream ss;
	ss << "This message has been automatically generated by scm on host: "
	<< System::HostInfo().name() << " for master: master@" << SCCoreApp->connection()->source() << endl;

	_message.setHeader(ss.str());
	ss.str(string());

	ss << "The following clients match the given filter condition:" << endl;
	ss << filterString();
	_message.setHeaderFilteredClients(ss.str());
	ss.str(string());

	ss << "Some of the connected have been silent for more than "
	   << _reportRequiredClientsTimeSpan << " seconds" << endl;
	ss << "'-' denotes a silent and '+' a recovered client.";
	_message.setHeaderSilentClients(ss.str());
	ss.str(string());

	ss << "Some required clients are disconnected (-) or reconnected (+)" << endl;
	ss << "Required clients: ";
	RequiredClients::iterator it = _requiredClients.begin();
	for ( ; it != _requiredClients.end(); ++it ) {
		if ( it != _requiredClients.begin() )
			ss << ", ";
		ss << it->first;
	}
	_message.setHeaderRequiredClients(ss.str());
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MEmailPlugin::process(const ClientTable &clientTable) {
	if ( !operational() )
		return;

	_message.clear();

	if ( isFilteringEnabled() ) {
		SEISCOMP_DEBUG("Processing client table with %d clients", int(clientTable.size()));
		for ( auto &&item : clientTable ) {
			ClientInfoData::const_iterator clientIt = item.info.find(Client::Status::Clientname);
			if ( clientIt != item.info.end() ) {
				SEISCOMP_DEBUG("Applying filter on client: %s ", clientIt->second.c_str());
			}
		}

		const ClientTable* match = filterMean(clientTable);
		if ( match ) {
			SEISCOMP_DEBUG("Number of filter matches: %d", int(match->size()));
			vector<string> data;
			stringstream ss;
			for ( ClientTable::const_iterator it = match->begin(); it != match->end(); ++it ) {
				ss << printFilterMatch<Client::Status::Programname>(*it) << endl;
				ss << printFilterMatch<Client::Status::Clientname>(*it) << endl;
				ss << printFilterMatch<Client::Status::Hostname>(*it) << endl;
				ss << printFilterMatch<Client::Status::TotalMemory>(*it) << endl;
				ss << printFilterMatch<Client::Status::ClientMemoryUsage>(*it) << endl;
				ss << printFilterMatch<Client::Status::CPUUsage>(*it) << endl;
				ss << printFilterMatch<Client::Status::MessageQueueSize>(*it) << endl;
				ss << printFilterMatch<Client::Status::Uptime>(*it) << endl;
				ss << printFilterMatch<Client::Status::ResponseTime>(*it);
				data.push_back(ss.str());
				ss.str(string());
				SEISCOMP_DEBUG("--");
			}
			if ( data.size() > 0 )
				_message.setFilteredClients(data);

			if ( !_template.empty() )
				_message.setUserData(_template);
		}
	}

	if ( _reportSilentClients ) {
		vector<string> silentClients;
		vector<string> recoveredClients;

		ClientTable::const_iterator clientIt = clientTable.begin();
		for ( ; clientIt != clientTable.end(); ++clientIt ) {
			const ClientInfoData& clientInfoData = clientIt->info;

			// Check if the client is already in the list
			ClientInfoData::const_iterator clientNameIt =
				clientInfoData.find(Client::Status::Clientname);

			if ( clientNameIt == clientInfoData.end() ) {
				SEISCOMP_DEBUG("Could not find clientnametag in clientinfodata");
				continue;
			}

			// Ignore clients which are not listed in the required clients.
			if ( _requiredClients.find(clientNameIt->second) == _requiredClients.end() )
				continue;

			SilentClients::iterator silentClientIt = find(
					_silentClients.begin(),
					_silentClients.end(),
					clientNameIt->second
			);

			ClientInfoData::const_iterator responseTimeIt = clientInfoData.find(Client::Status::ResponseTime);
			if ( responseTimeIt == clientInfoData.end() )
				continue;

			// Get the hostname
			ClientInfoData::const_iterator hostNameIt =	clientInfoData.find(Client::Status::Hostname);
			if ( hostNameIt == clientInfoData.end() ){
				SEISCOMP_DEBUG("Could not find HOSTNAME_TAG");
				continue;
			}
			string hostNameStr = " on " + hostNameIt->second;

			int responseTime = 0;
			Core::fromString(responseTime, responseTimeIt->second);
			if ( responseTime > _reportSilentClientsTimeSpan ) {
				if ( silentClientIt == _silentClients.end() ) {
					silentClients.push_back(string("- ") + clientNameIt->second + hostNameStr);
					_silentClients.push_back(clientNameIt->second);
				}
			}
			else {
				if ( silentClientIt != _silentClients.end() ) {
					recoveredClients.push_back("+ " + *silentClientIt + hostNameStr);
					_silentClients.erase(silentClientIt);
				}
			}
		}
		if ( silentClients.size() > 0 || recoveredClients.size() > 0 )
			_message.setSilentClients(silentClients, recoveredClients);
	}

	if ( Core::Time::GMT() - _requiredClientsTimeMarker > _reportRequiredClientsTimeSpan ) {
		_requiredClientsTimeMarker = Core::Time::GMT();
		vector<string> missingClients;
		vector<string> reconnectedClients;
		RequiredClients::iterator rcIt = _requiredClients.begin();
		for ( ; rcIt != _requiredClients.end(); ++rcIt ) {
			ClientTable::const_iterator found =
				find_if(
					clientTable.begin(),
					clientTable.end(),
					bind(findName, placeholders::_1, rcIt->first)
				);

			if ( found == clientTable.end() && !rcIt->second ) {
				missingClients.push_back(string("- ") + rcIt->first);
				rcIt->second = true;
			}
			else if ( found != clientTable.end() && rcIt->second ) {
				rcIt->second = false;
				ClientInfoData::const_iterator hostNameIt =	found->info.find(Client::Status::Hostname);
				if ( hostNameIt == found->info.end() ) {
					SEISCOMP_DEBUG("Could not find HOSTNAME_TAG");
					continue;
				}
				string hostNameStr = " on " + hostNameIt->second;
				reconnectedClients.push_back(string("+ ") + rcIt->first + hostNameStr);
			}
		}
		if ( missingClients.size() > 0 || reconnectedClients.size() > 0 )
			_message.setRequiredClients(missingClients, reconnectedClients);
	}

	// Send email
	if ( !_message.empty() && _sendEmail ) {
		for ( size_t i = 0; i < _recipients.size(); ++i ) {
			if ( !_sender->sendEmail(_message.message(), _recipients[i]) )
				SEISCOMP_ERROR("MEmailPlugin: Sending notification to %s failed", _recipients[i].c_str());
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MEmailPlugin::addRecipient(const string &recipient) {
	_recipients.push_back(recipient);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MEmailPlugin::addRecipients(const vector<string> &recipients) {
	_recipients.assign(recipients.begin(), recipients.end());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const vector<string> &MEmailPlugin::recipients() const {
	return _recipients;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
} // namespace Applications
} // namespace Seiscomp
