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

#include <seiscomp/autoloc/stationconfig.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <seiscomp/logging/log.h>


namespace Seiscomp {

namespace Autoloc {


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
StationConfigFile::StationConfigFile()
{
	std::string defaultkey = "* *";
	_entry[defaultkey] = StationConfig();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void StationConfigFile::setFilename(const std::string &filename) {
	this->filename = filename;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool StationConfigFile::read()
{
	std::string line;
	std::string defaultkey = "* *";
	_entry[defaultkey] = StationConfig();

	std::ifstream ifile(filename.c_str());
	if ( ! ifile.good()) {
		SEISCOMP_ERROR_S("Failed to open station config file "+filename);
		return false;
	}

	StationConfig entry;
	while( ! ifile.eof()) {
		std::getline(ifile, line);
		line.erase(0, line.find_first_not_of(" \n\r\t"));
		if (line[0] == '#')
			continue;
		char net[10], sta[10];
		int n = std::sscanf(line.c_str(), "%8s %8s %d %f", net, sta, &entry.usage, &entry.maxNucDist);
		if (n!=4) break;
		std::string key = std::string(net)
			+ std::string(" ")
			+ std::string(sta);
		_entry[key] = entry;
	}

	_mtime = mtime();

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
time_t StationConfigFile::mtime() const
{
	struct stat buf;

	if (stat(filename.c_str(), &buf) != 0) {
		// error!
		return 0; // FIXME
	}

	return buf.st_mtime;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool StationConfigFile::hasChanged() const
{
	return mtime() != _mtime;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const StationConfig&
StationConfigFile::get(const std::string &net, const std::string &sta) const
{
	std::vector<std::string> patterns;
	patterns.push_back(net + " " + sta);
	patterns.push_back(net + " *");
	patterns.push_back("* " + sta);
	patterns.push_back("* *");

	for (std::vector<std::string>::iterator
	     it = patterns.begin(); it != patterns.end(); ++it) {

		const std::string &pattern = *it;
		std::map<std::string, StationConfig>::const_iterator mit = _entry.find(pattern);
		if (mit == _entry.end())
			continue;

		const StationConfig &e = (*mit).second;
		SEISCOMP_DEBUG("Station %s %s  pattern %-8s config: usage=%d maxnucdist=%g",
		               net.c_str(), sta.c_str(), pattern.c_str(), e.usage, e.maxNucDist);

		return e;
	}

// This should never be executed:
//	string defaultkey = "* *";
//	return _entry[defaultkey];
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

}  // namespace Autoloc

}  // namespace Seiscomp
