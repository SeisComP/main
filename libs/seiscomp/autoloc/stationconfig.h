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


#ifndef SEISCOMP_LIBAUTOLOC_STATIONCONFIG_H_INCLUDED
#define SEISCOMP_LIBAUTOLOC_STATIONCONFIG_H_INCLUDED


#include <string>
#include <map>
#include <seiscomp/core/datetime.h>

namespace Seiscomp {

namespace Autoloc {

class StationConfigItem {
	public:
		float maxNucDist, maxLocDist;
		int usage;

	StationConfigItem() {
		maxNucDist = maxLocDist = 0;
		usage = 0;
	}
};


class StationConfig {
	public:
		StationConfig();

		void setFilename(const std::string &filename);
		const std::string &filename() const;


		// Has the file modification time changed since last read()?
		bool hasChanged() const;

		// Read the file. If successful return true, otherwise false.
		bool read();

		// Get the config item for net,sta
		const StationConfigItem& get(
			const std::string &net,
			const std::string &sta) const;

	private:
		time_t mtime() const;

	private:
		time_t _mtime;
		std::string _filename;
		std::map<std::string, StationConfigItem> _item;
};

}  // namespace Autoloc

}  // namespace Seiscomp

#endif
