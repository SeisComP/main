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




#define SEISCOMP_COMPONENT Autoloc

#include "stationconfig.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <seiscomp/logging/log.h>

namespace Seiscomp {

namespace AutolocInternal {

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// trim leading/trailing space from line
static void ltrim(std::string &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
		return !std::isspace(ch);
	}));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
StationConfig::StationConfig() {
	std::string defaultkey = "* *";
	_item[defaultkey] = StationConfigItem();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
StationConfig::StationConfig(const std::string &filename) {
	setFilename(filename);
	if ( !read() ) {
		throw std::runtime_error("Could not read station config from " + filename);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void StationConfig::setFilename(const std::string &filename) {
	_filename = filename;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const std::string &StationConfig::filename() const {
	return _filename;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool StationConfig::read() {
	std::string line;
	std::string defaultkey = "* *";
	_item[defaultkey] = StationConfigItem();

	std::ifstream ifile(_filename.c_str());
	if ( !ifile.good() ) {
		SEISCOMP_ERROR_S("Failed to open station config file " + _filename);
		return false;
	}

	StationConfigItem item;
	while ( !ifile.eof() ) {
		std::getline(ifile, line);
		ltrim(line);
//		line.erase(0, line.find_first_not_of(" \n\r\t"));
		if ( line.empty() )
			continue;
		if ( line[0] == '#' )
			continue;
		char net[10], sta[10];
		int n = std::sscanf(line.c_str(), "%8s %8s %d %f", net, sta,
				    &item.usage, &item.maxNucDist);
		if ( n != 4 ) break;
		std::string key = std::string(net) + std::string(" ") + std::string(sta);
		_item[key] = item;
	}

	_mtime = mtime();

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
time_t StationConfig::mtime() const {
	struct stat buf;

	if ( stat(_filename.c_str(), &buf) != 0 ) {
		// t.b.d. exception?
		throw std::runtime_error("Could not stat " + _filename);
	}

	return buf.st_mtime;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool StationConfig::hasChanged() const {
	return mtime() != _mtime;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const StationConfigItem&
StationConfig::get(const std::string &net, const std::string &sta) const {
	std::vector<std::string> patterns;
	patterns.push_back(net + " " + sta);
	patterns.push_back(net + " *");
	patterns.push_back("* " + sta);
	patterns.push_back("* *");

	for ( const std::string &pattern : patterns ) {
		std::map<std::string, StationConfigItem>::const_iterator
			mit = _item.find(pattern);
		if ( mit == _item.end() )
			continue;

		const StationConfigItem &e = (*mit).second;
		SEISCOMP_DEBUG("Station %s %s  pattern %-8s config: usage=%d maxnucdist=%g",
		               net.c_str(), sta.c_str(), pattern.c_str(), e.usage, e.maxNucDist);

		return e;
	}

	// This should never be executed
	return _item.begin()->second;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


}  // namespace AutolocInternal

}  // namespace Seiscomp
