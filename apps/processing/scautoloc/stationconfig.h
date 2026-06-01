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

namespace AutolocInternal {

class StationConfigItem {
	public:
		StationConfigItem() = default;

		StationConfigItem(const StationConfigItem &other) {
			maxNucDist = other.maxNucDist;
			maxLocDist = other.maxLocDist;
			usage = other.usage;
		}

		float maxNucDist {0};
		float maxLocDist {0};
		int usage {0};

};


class StationConfig {
	public:
		// Create an empty station config
		StationConfig();

		// Create station config and read the contents of the specified file
		StationConfig(const std::string &filename);

		// Set/get the station config file name
		void setFilename(const std::string &filename);
		const std::string &filename() const;

		// Has the file modification time changed since last read()?
		bool hasChanged() const;

		// Read the file. If successful return true, otherwise false.
		bool read();

		// Get the config item for net,sta
		const StationConfigItem& get(const std::string &net, const std::string &sta) const;

	private:
		time_t mtime() const;

	private:
		time_t _mtime {0};
		std::string _filename;
		std::map<std::string, StationConfigItem> _item;
};

}  // namespace AutolocInternal

}  // namespace Seiscomp

#endif
