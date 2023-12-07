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

#include "config.h"
#include "quakelink.h"

#include <seiscomp/client/application.h>
#include <seiscomp/core/strings.h>
#include <seiscomp/datamodel/amplitude.h>
#include <seiscomp/datamodel/event.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/focalmechanism.h>
#include <seiscomp/datamodel/magnitude.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/reading.h>
#include <seiscomp/datamodel/stationmagnitude.h>
#include <seiscomp/logging/log.h>

#include <algorithm>
#include <iterator>


using namespace std;


namespace Seiscomp {
namespace QL2SC {
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Config::init() {
	Client::Application *app = SCCoreApp;
	if ( !app ) {
		return false;
	}

	SEISCOMP_INFO("reading configuration");

	// back log seconds
	try {
		int i = app->configGetInt("backLog");
		backLog = i < 0 ? 0 : (size_t)i;
	}
	catch ( ... ) { backLog = 1800; }

	// maximum number of notifiers per message
	try { batchSize = app->configGetInt("batchSize"); }
	catch ( ... ) { batchSize = 2000; }

	try { maxWaitForEventIDTimeout = app->configGetInt("eventAssociationTimeout"); }
	catch ( ... ) { maxWaitForEventIDTimeout = 10; }

	try { allowRemoval = app->configGetBool("allowRemoval"); }
	catch ( ... ) { allowRemoval = true; }

	try {
		auto publicIDWhitelist = app->configGetStrings("processing.whitelist.publicIDs");
		copy(
			publicIDWhitelist.begin(), publicIDWhitelist.end(),
			inserter(publicIDFilter.allow, publicIDFilter.allow.end())
		);
	}
	catch ( ... ) {}

	try {
		auto publicIDBlacklist = app->configGetStrings("processing.blacklist.publicIDs");
		copy(
			publicIDBlacklist.begin(), publicIDBlacklist.end(),
			inserter(publicIDFilter.deny, publicIDFilter.deny.end())
		);
	}
	catch ( ... ) {}

	// host configurations
	hosts.clear();
	vector<string> hostNames, routings;

	SEISCOMP_INFO("reading host configuration");
	try { hostNames = app->configGetStrings("hosts"); }
	catch ( ... ) {}
	if ( hostNames.empty() ) {
		SEISCOMP_ERROR("could not read host list");
		return false;
	}

	for ( vector<string>::const_iterator it = hostNames.begin();
	      it != hostNames.end(); ++it ) {
		HostConfig cfg;
		string prefix = "host." + *it + ".";

		// host
		cfg.host = *it;

		// URL
		try { cfg.url = app->configGetString(prefix + "url"); }
		catch ( ... ) { cfg.url = "ql://localhost:18010"; }

		// gzip
		try { cfg.gzip = app->configGetBool(prefix + "gzip"); }
		catch ( ... ) { cfg.gzip = false; }

		// native
		try { cfg.native = app->configGetBool(prefix + "native"); }
		catch ( ... ) { cfg.native = false; }

		try { cfg.syncEventAttributes = app->configGetBool(prefix + "syncEventAttributes"); }
		catch ( ... ) { cfg.syncEventAttributes = true; }

		try { cfg.syncPreferred = app->configGetBool(prefix + "syncPreferred"); }
		catch ( ... ) { cfg.syncPreferred = false; }

		try { cfg.syncEventDelay = app->configGetInt(prefix + "syncEventDelay"); }
		catch ( ... ) { cfg.syncEventDelay = 0; }

		// data options
		bool isSet;
		string dataPrefix = prefix + "data.";
		cfg.options = IO::QuakeLink::opIgnore;

		try { isSet = app->configGetBool(dataPrefix + "picks"); }
		catch ( ... ) { isSet = true; }
		if ( isSet ) cfg.options |= IO::QuakeLink::opDataPicks;

		try { isSet = app->configGetBool(dataPrefix + "amplitudes"); }
		catch ( ... ) { isSet = true; }
		if ( isSet ) cfg.options |= IO::QuakeLink::opDataAmplitudes;

		try { isSet = app->configGetBool(dataPrefix + "arrivals"); }
		catch ( ... ) { isSet = true; }
		if ( isSet ) cfg.options |= IO::QuakeLink::opDataArrivals;

		try { isSet = app->configGetBool(dataPrefix + "staMags"); }
		catch ( ... ) { isSet = true; }
		if ( isSet ) cfg.options |= IO::QuakeLink::opDataStaMags;

		try { isSet = app->configGetBool(dataPrefix + "staMts"); }
		catch ( ... ) { isSet = true; }
		if ( isSet ) cfg.options |= IO::QuakeLink::opDataStaMts;

		try { isSet = app->configGetBool(dataPrefix + "preferred"); }
		catch ( ... ) { isSet = true; }
		if ( isSet ) cfg.options |= IO::QuakeLink::opDataPreferred;

		// keep alive messages
		try { isSet = app->configGetBool(prefix + "keepAlive"); }
		catch ( ... ) { isSet = true; }
		if ( isSet ) cfg.options |= Seiscomp::IO::QuakeLink::opKeepAlive;

		// filter
		try { cfg.filter = app->configGetString(prefix + "filter"); }
		catch ( ... ) {}

		// routing table
		try {
			routings = app->configGetStrings(prefix + "routingTable");
			vector<string> toks;
			for ( vector<string>::iterator it = routings.begin();
			      it != routings.end(); ++it ) {
				Core::split(toks, it->c_str(), ":");
				if ( toks.size() != 2 ) {
					SEISCOMP_ERROR("Malformed routing table entry: %s", it->c_str());
					return false;
				}
				cfg.routingTable[toks[0]] = (toks[1] == "NULL" ? "" : toks[1]);
			}
		}
		catch ( ... ) {
			cfg.routingTable[DataModel::Pick::TypeInfo().className()] = Client::Protocol::IMPORT_GROUP;
			cfg.routingTable[DataModel::Amplitude::TypeInfo().className()] = Client::Protocol::IMPORT_GROUP;
			cfg.routingTable[DataModel::Origin::TypeInfo().className()] = "EVENT";
			cfg.routingTable[DataModel::FocalMechanism::TypeInfo().className()] = "EVENT";
		}

		// create explicit routing entries for top-level EventParameters
		// children in case a routing entry for EventParameters is found
		RoutingTable::const_iterator rit = cfg.routingTable.find(DataModel::EventParameters::TypeInfo().className());
		if ( rit != cfg.routingTable.end() && !rit->second.empty() ) {
			if ( cfg.routingTable[DataModel::Pick::TypeInfo().className()].empty() )
				cfg.routingTable[DataModel::Pick::TypeInfo().className()] = rit->second;
			if ( cfg.routingTable[DataModel::Amplitude::TypeInfo().className()].empty() )
				cfg.routingTable[DataModel::Amplitude::TypeInfo().className()] = rit->second;
			if ( cfg.routingTable[DataModel::Reading::TypeInfo().className()].empty() )
				cfg.routingTable[DataModel::Reading::TypeInfo().className()] = rit->second;
			if ( cfg.routingTable[DataModel::Origin::TypeInfo().className()].empty() )
				cfg.routingTable[DataModel::Origin::TypeInfo().className()] = rit->second;
			if ( cfg.routingTable[DataModel::FocalMechanism::TypeInfo().className()].empty() )
				cfg.routingTable[DataModel::FocalMechanism::TypeInfo().className()] = rit->second;
			if ( cfg.routingTable[DataModel::Event::TypeInfo().className()].empty() )
				cfg.routingTable[DataModel::Event::TypeInfo().className()] = rit->second;
		}

		hosts.push_back(cfg);

		stringstream ss;
		format(ss, cfg.routingTable);
		SEISCOMP_DEBUG("Read host configuration '%s':\n"
		               "  url         : %s\n"
		               "  gzip        : %s\n"
		               "  native      : %s\n"
		               "  data\n"
		               "    picks     : %s\n"
		               "    amplitudes: %s\n"
		               "    arrivals  : %s\n"
		               "    staMags   : %s\n"
		               "    staMts    : %s\n"
		               "    preferred : %s\n"
		               "  keepAlive   : %s\n"
		               "  filter      : %s\n"
		               "  routing     : %s\n",
		               it->c_str(),
		               cfg.url.c_str(),
		               cfg.gzip                                      ? "true" : "false",
		               cfg.native                                    ? "true" : "false",
		               cfg.options & IO::QuakeLink::opDataPicks      ? "true" : "false",
		               cfg.options & IO::QuakeLink::opDataAmplitudes ? "true" : "false",
		               cfg.options & IO::QuakeLink::opDataArrivals   ? "true" : "false",
		               cfg.options & IO::QuakeLink::opDataStaMags    ? "true" : "false",
		               cfg.options & IO::QuakeLink::opDataStaMts     ? "true" : "false",
		               cfg.options & IO::QuakeLink::opDataPreferred  ? "true" : "false",
		               cfg.options & IO::QuakeLink::opKeepAlive      ? "true" : "false",
		               cfg.filter.c_str(),
		               ss.str().c_str());
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Config::format(stringstream &ss, const RoutingTable &table) const {
	bool first = true;
	for ( RoutingTable::const_iterator it = table.begin();
	      it != table.end(); ++it, first = false) {
		if ( !first ) ss << ", ";
		ss << it->first << ":" << it->second;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
} // ns QL2SC
} // ns Seiscomp
