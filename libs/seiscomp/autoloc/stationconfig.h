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


#ifndef SEISCOMP_AUTOLOC_STATIONCONFIG_H
#define SEISCOMP_AUTOLOC_STATIONCONFIG_H


#include <string>
#include <map>
#include <seiscomp/core/datetime.h>

namespace Seiscomp {

namespace Autoloc {

class StationConfig {
	public:
		float maxNucDist, maxLocDist;
		int usage;

	StationConfig() {
		maxNucDist = maxLocDist = 0;
		usage = 0;
	}
};


class StationConfigFile {
	public:
		StationConfigFile();
		void setFilename(const std::string &filename);

		// Has the file modification time changed since last read()?
		bool hasChanged() const;

		// Read the file. If successful return true, otherwise false.
		bool read();

		// Get the config entry for net,sta
		const StationConfig& get(
			const std::string &net,
			const std::string &sta) const;

	private:
		time_t mtime() const;

	private:
		time_t _mtime;
		std::string filename;
		std::map<std::string, StationConfig> _entry;
};

}  // namespace Autoloc

}  // namespace Seiscomp

#endif
