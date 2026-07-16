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
#include <seiscomp/datamodel/catalog.h>
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


namespace Seiscomp::QL2SC {
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

	try { strictModificationTime = app->configGetBool("strictModificationTime"); }
	catch ( ... ) { strictModificationTime = false; }

	try {
		auto list = app->configGetStrings("processing.whitelist.publicIDs");
		std::vector<std::string> publicIDlist;
		publicIDlist.reserve(list.size());
		for ( const auto &item : list ) {
			publicIDlist.push_back(Seiscomp::Util::replace(item));
		}
		copy(
			publicIDlist.begin(), publicIDlist.end(),
			inserter(publicIDFilter.allow, publicIDFilter.allow.end())
		);
	}
	catch ( ... ) {}

	try {
		auto list =  app->configGetStrings("processing.blacklist.publicIDs");
		std::vector<std::string> publicIDlist;
		publicIDlist.reserve(list.size());
		for ( const auto &item : list ) {
			publicIDlist.push_back(Seiscomp::Util::replace(item));
		}
		copy(
			publicIDlist.begin(), publicIDlist.end(),
			inserter(publicIDFilter.deny, publicIDFilter.deny.end())
		);
	}
	catch ( ... ) {}

	// host configurations
	hosts.clear();
	vector<string> hostNames;
	vector<string> routings;

	SEISCOMP_INFO("reading host configuration");
	try { hostNames = app->configGetStrings("hosts"); }
	catch ( ... ) {}
	if ( hostNames.empty() ) {
		SEISCOMP_ERROR("could not read host list");
		return false;
	}

	for ( const auto& host : hostNames ) {
		HostConfig cfg;
		string prefix = "host." + host + ".";

		// host
		cfg.host = host;

		// URL
		try { cfg.url = app->configGetString(prefix + "url"); }
		catch ( ... ) { cfg.url = "ql://localhost:18010"; }

		// gzip
		try { cfg.gzip = app->configGetBool(prefix + "gzip"); }
		catch ( ... ) { cfg.gzip = false; }

		// native
		try { cfg.native = app->configGetBool(prefix + "native"); }
		catch ( ... ) { cfg.native = false; }

		try { cfg.delay = app->configGetInt(prefix + "delay"); }
		catch ( ... ) { cfg.delay = 0; }

		try { cfg.syncEventAttributes = app->configGetBool(prefix + "syncEventAttributes"); }
		catch ( ... ) { cfg.syncEventAttributes = true; }

		try { cfg.syncPreferred = app->configGetBool(prefix + "syncPreferred"); }
		catch ( ... ) { cfg.syncPreferred = false; }

		try { cfg.syncJournals = app->configGetBool(prefix + "syncJournals"); }
		catch ( ... ) { cfg.syncJournals = false; }

		try { cfg.syncEventDelay = app->configGetInt(prefix + "syncEventDelay"); }
		catch ( ... ) { cfg.syncEventDelay = 0; }

		// QuakeLink options
		cfg.options = IO::QuakeLink::opIgnore;

		auto setQLOption = [&] (const char *name, IO::QuakeLink::Options flag,
		                        bool enabled = true) {
			try {
				enabled = app->configGetBool(prefix + name);
			}
			catch ( ... ) {}

			if ( enabled ) {
				cfg.options |= flag;
			}
		};

		// data options
		setQLOption("data.picks", IO::QuakeLink::opDataPicks);
		setQLOption("data.amplitudes", IO::QuakeLink::opDataAmplitudes);
		setQLOption("data.arrivals", IO::QuakeLink::opDataArrivals);
		setQLOption("data.staMags", IO::QuakeLink::opDataStaMags);
		setQLOption("data.staMts", IO::QuakeLink::opDataStaMts);
		setQLOption("data.preferred", IO::QuakeLink::opDataPreferred);

		// keep alive messages
		setQLOption("keepAlive", Seiscomp::IO::QuakeLink::opKeepAlive);

		// filter
		try { cfg.filter = app->configGetString(prefix + "filter"); }
		catch ( ... ) {}

		// routing table
		try {
			routings = app->configGetStrings(prefix + "routingTable");
			vector<string> toks;
			for ( const auto &route : routings ) {
				Core::split(toks, route, ":");
				if ( toks.size() != 2 ) {
					SEISCOMP_ERROR("Malformed routing table entry: %s", route);
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
		const auto rit = cfg.routingTable.find(DataModel::EventParameters::TypeInfo().className());
		if ( rit != cfg.routingTable.end() && !rit->second.empty() ) {
			auto target = rit->second;
			for ( const auto &type : {
			          DataModel::Amplitude::TypeInfo(),
			          DataModel::Catalog::TypeInfo(),
			          DataModel::Event::TypeInfo(),
			          DataModel::FocalMechanism::TypeInfo(),
			          DataModel::Origin::TypeInfo(),
			          DataModel::Pick::TypeInfo(),
			          DataModel::Reading::TypeInfo()
			      }) {
				const auto *className = type.className();
				if ( cfg.routingTable.find(className) == cfg.routingTable.end() ) {
					cfg.routingTable[className] = target;
				}
			}
		}

		hosts[host] = cfg;

		stringstream ss;
		format(ss, cfg.routingTable);
		SEISCOMP_INFO("Read host configuration '%s':\n"
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
		              host,
		              cfg.url,
		              cfg.gzip                                      ? "true" : "false",
		              cfg.native                                    ? "true" : "false",
		              cfg.options & IO::QuakeLink::opDataPicks      ? "true" : "false",
		              cfg.options & IO::QuakeLink::opDataAmplitudes ? "true" : "false",
		              cfg.options & IO::QuakeLink::opDataArrivals   ? "true" : "false",
		              cfg.options & IO::QuakeLink::opDataStaMags    ? "true" : "false",
		              cfg.options & IO::QuakeLink::opDataStaMts     ? "true" : "false",
		              cfg.options & IO::QuakeLink::opDataPreferred  ? "true" : "false",
		              cfg.options & IO::QuakeLink::opKeepAlive      ? "true" : "false",
		              cfg.filter,
		              ss.str());
	}

	std::ostringstream deny;
	if ( publicIDFilter.deny.empty() ) {
		deny << "none";
	}
	else {
		for ( const auto &denyItem : publicIDFilter.deny ) {
			deny << denyItem << " ";
		}
	}

	std::ostringstream allow;
	if ( publicIDFilter.allow.empty() ) {
		allow << "none";
	}
	else {
		for ( const auto &allowItem : publicIDFilter.allow ) {
			allow << allowItem << " ";
		}
	}

	SEISCOMP_INFO("Processing configuration:");
	SEISCOMP_INFO("  allowed publicID prefixes: %s", allow.str());
	SEISCOMP_INFO("  blocked publicID prefixes: %s", deny.str());

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Config::format(stringstream &ss, const RoutingTable &table) {
	bool first = true;
	for ( const auto &[name, target] : table ) {
		if ( first ) {
			first = false;
		}
		else {
			ss << ", ";
		}
		ss << name << ":" << target;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
} // ns Seiscomp::QL2SC
