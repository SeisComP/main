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

#include "monitorplugininterface.h"
#include "monitorfilter.h"

#include <iostream>
#include <boost/lexical_cast.hpp>

#include <seiscomp/core/interfacefactory.ipp>


IMPLEMENT_INTERFACE_FACTORY(Seiscomp::Applications::MonitorPluginInterface, SC_MPLUGIN_API);


namespace Seiscomp {
namespace Applications {

IMPLEMENT_SC_ABSTRACT_CLASS(MonitorPluginInterface, "MonitorPluginInterface");


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool findName(ClientInfoData clientData, std::string name) {
	auto it = clientData.find(Client::Status::Clientname);
	if ( it == clientData.end() ) {
		return false;
	}
	if ( name != it->second ) {
		return false;
	}
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
MonitorPluginInterface::MonitorPluginInterface(const std::string& name)
	: _filterMeanInterval(10*60),
	  _filterMeanTimeMark(Core::Time::GMT()),
	  _name(name),
	  _operational(false),
	  _isFilteringEnabled(false),
	  _mFilterParser(NULL),
	  _filter(NULL) {
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
MonitorPluginInterface::~MonitorPluginInterface() {
	if ( _mFilterParser ) delete _mFilterParser;
	if ( _filter ) delete _filter;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
MonitorPluginInterface* MonitorPluginInterface::Create(const std::string& service) {
	return MonitorPluginInterfaceFactory::Create(service.c_str());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool MonitorPluginInterface::init(const Config::Config &cfg) {
	setOperational(initFilter(cfg));
	return operational();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool MonitorPluginInterface::initFilter(const Config::Config &cfg) {
	try {
		_filterStr = cfg.getString(_name + ".filter");
		SEISCOMP_DEBUG("Filter expression: %s", _filterStr.c_str());
		_mFilterParser = new MFilterParser;
		bs::tree_parse_info<> info = bs::ast_parse(_filterStr.c_str(), *_mFilterParser, bs::space_p);
		if ( info.full ) {
			SEISCOMP_DEBUG("Parsing filter expression succeed");
			MOperatorFactory factory;
			_filter = evalParseTree(info.trees.begin(), factory);
		}
		else
			SEISCOMP_ERROR("Parsing filter expression: %s failed at token: %c", _filterStr.c_str(), *info.stop);

		if ( !_filter ) {
			SEISCOMP_ERROR("Message Filter could not be instantiated.");
			return false;
		}
		_isFilteringEnabled = true;
	}
	catch ( Config::OptionNotFoundException& ) {
		SEISCOMP_DEBUG("Filter option for %s not set. Message filtering disabled.", _name.c_str());
	}
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool MonitorPluginInterface::operational() const {
	return _operational;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool MonitorPluginInterface::isFilteringEnabled() const {
	return _isFilteringEnabled;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MonitorPluginInterface::setOperational(bool val) {
	_operational = val;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const std::string& MonitorPluginInterface::filterString() const {
	return _filterStr;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const ClientTable* MonitorPluginInterface::filter(const ClientTable& clientTable) {
	if ( !_filter )
		return NULL;

	_match.clear();
	for ( ClientTable::const_iterator it = clientTable.begin(); it != clientTable.end(); ++it ) {
		if ( _filter->eval(*it) )
			_match.push_back(*it);
	}
	return match();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const ClientTable* MonitorPluginInterface::filterMean(const ClientTable &clientTable) {
	// Delete already disconnected clients
	ClientTable::iterator fmIt = _filterMeanClientTable.begin();
	while ( fmIt != _filterMeanClientTable.end() ) {
		auto found = std::find_if(
				clientTable.begin(),
				clientTable.end(),
				[&fmIt](const ClientTableEntry &entry) -> bool {
					return findName(entry, fmIt->info[Client::Status::Clientname]);
				}
		);
		if ( found == clientTable.end() ) {
			_filterMeanMessageCount.erase(fmIt->info[Client::Status::Clientname]);
			fmIt = _filterMeanClientTable.erase(fmIt);
		}
		else {
			++fmIt;
		}
	}

	// Add newly connected clients
	ClientTable::const_iterator ctIt = clientTable.begin();
	for ( ; ctIt != clientTable.end(); ++ctIt ) {
		auto &name = ctIt->info.find(Client::Status::Clientname)->second;
		auto found = std::find_if(
				_filterMeanClientTable.begin(),
				_filterMeanClientTable.end(),
				[name](const ClientTableEntry &entry) -> bool {
					return findName(entry, name);
				}
		);

		if ( found != _filterMeanClientTable.end() ) {
			// Update data
			sumData<Client::Status::TotalMemory>(*found, *ctIt);
			sumData<Client::Status::ClientMemoryUsage>(*found, *ctIt);
			sumData<Client::Status::CPUUsage>(*found, *ctIt);
			sumData<Client::Status::MessageQueueSize>(*found, *ctIt);

			auto mIt = ctIt->info.find(Client::Status::Uptime);
			if ( mIt != ctIt->info.end() )
				found->info[Client::Status::Uptime] = mIt->second;
			else
				found->info[Client::Status::Uptime] = "-1";

			mIt = ctIt->info.find(Client::Status::ResponseTime);
			if ( mIt != ctIt->info.end() )
				found->info[Client::Status::ResponseTime] = mIt->second;
			else
				found->info[Client::Status::ResponseTime] = "-1";

			++(_filterMeanMessageCount[ctIt->info.find(Client::Status::Clientname)->second]);
		}
		else {
			_filterMeanClientTable.push_back(*ctIt);
			_filterMeanMessageCount[ctIt->info.find(Client::Status::Clientname)->second] = 1;
		}
	}

	if ( Core::Time::GMT() - _filterMeanTimeMark >= _filterMeanInterval ) {
		_filterMeanTimeMark = Core::Time::GMT();
		// Calculate mean
		for ( ClientTable::iterator it = _filterMeanClientTable.begin();
			 it != _filterMeanClientTable.end(); ++it ) {
			size_t count = _filterMeanMessageCount[it->info[Client::Status::Clientname]];
			calculateMean<Client::Status::TotalMemory>(*it, count);
			calculateMean<Client::Status::ClientMemoryUsage>(*it, count);
			calculateMean<Client::Status::CPUUsage>(*it, count);
			calculateMean<Client::Status::MessageQueueSize>(*it, count);
		}
		return filter(_filterMeanClientTable);
	}
	return NULL;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MonitorPluginInterface::setFilterMeanInterval(double interval) {
	_filterMeanInterval = interval * 60;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const ClientTable* MonitorPluginInterface::match() const {
	return _match.size() > 0 ? &_match : NULL;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const std::string& MonitorPluginInterface::name() const {
	return _name;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
template <Client::Status::ETag tag>
void MonitorPluginInterface::sumData(ClientInfoData &lhs, const ClientInfoData &rhs) {
	typedef typename Client::StatusT<tag>::Type Type;
	ClientInfoData::const_iterator found = rhs.find(tag);
	if ( found == rhs.end() ) {
		SEISCOMP_ERROR("Incompatible data found. Tag %s could not be found in ClientInfoData",
				Client::Status::Tag(tag).toString());
	}

	try {
		Type result = boost::lexical_cast<Type>(lhs[tag]) +
			          boost::lexical_cast<Type>(found->second);
		lhs[tag] = boost::lexical_cast<std::string>(result);
	}
	catch ( boost::bad_lexical_cast& e ) {
		SEISCOMP_ERROR("%s", e.what());
		SEISCOMP_ERROR(
				"[%s, %s@%s] Datatypes could not be converted",
				rhs.find(Client::Status::Clientname)->second.c_str(),
				rhs.find(Client::Status::Programname)->second.c_str(),
				rhs.find(Client::Status::Hostname)->second.c_str()
		);
		SEISCOMP_ERROR(
				"tag = %s, lhs[tag] = %s, found->second = %s",
				Client::Status::Tag(tag).toString(),
				lhs[tag].c_str(),
				found->second.c_str()
		);
		// throw e;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
template <Client::Status::ETag tag>
void MonitorPluginInterface::calculateMean(ClientInfoData& lhs, size_t count) {
	typedef typename Client::StatusT<tag>::Type Type;
	try {
		Type result = boost::lexical_cast<Type>(lhs[tag]) / count;
		lhs[tag] = boost::lexical_cast<std::string>(result);
	}
	catch ( boost::bad_lexical_cast& e ) {
		SEISCOMP_ERROR("%s", e.what());
		SEISCOMP_ERROR(
				"[%s, %s@%s] Datatypes could not be converted",
				lhs.find(Client::Status::Clientname)->second.c_str(),
				lhs.find(Client::Status::Programname)->second.c_str(),
				lhs.find(Client::Status::Hostname)->second.c_str()
		);
		SEISCOMP_ERROR(
				"tag = %s, lhs[tag] = %s",
				Client::Status::Tag(tag).toString(),
				lhs[tag].c_str()
		);
		// throw e;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


} // namespace Applications
} // namespace Seiscomp
