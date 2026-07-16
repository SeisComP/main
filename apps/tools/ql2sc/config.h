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


#ifndef SEISCOMP_QL2SC_CONFIG_H__
#define SEISCOMP_QL2SC_CONFIG_H__


#include <seiscomp/utils/stringfirewall.h>

#include <map>
#include <string>
#include <sstream>
#include <map>


namespace Seiscomp::QL2SC {


typedef std::map<std::string, std::string> RoutingTable;

struct HostConfig {
	std::string  host;
	std::string  url;
	bool         gzip;
	bool         native;
	int          options;
	int          delay{0};
	std::string  filter;
	RoutingTable routingTable;
	bool         syncEventAttributes{true};
	bool         syncPreferred{false};
	bool         syncJournals{false};
	int          syncEventDelay{0};
};

using HostConfigs = std::map<std::string, HostConfig>;


struct PrefixFirewall : Util::StringFirewall {
	bool isAllowed(const std::string &s) const override {
		// Check prefix only
		return (allow.empty() ? true : matches(allow, s))
		    && (deny.empty() ? true : !matches(deny, s));
	}

	bool matches(const StringSet &set, const std::string &s) const {
		for ( const auto &prefix : set ) {
			if ( !s.compare(0, prefix.size(), prefix) ) {
				return true;
			}
		}

		return false;
	}
};


struct Config {
	bool init();
	static void format(std::stringstream &ss, const RoutingTable &table);

	int            batchSize;
	size_t         backLog;
	int            maxWaitForEventIDTimeout;
	PrefixFirewall publicIDFilter;
	bool           allowRemoval;
	bool           strictModificationTime;
	HostConfigs    hosts;
};


} // ns Seiscomp::QL2SC


#endif // SEISCOMP_QL2SC_CONFIG_H__
