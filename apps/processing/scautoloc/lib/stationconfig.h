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

namespace Seiscomp {

namespace Autoloc {


class StationConfig {
	public:
		class Entry {
			public:
				float maxNucDist, maxLocDist;
				int usage;

			Entry() {
				maxNucDist = maxLocDist = 0;
				usage = 0;
			}
		};

		StationConfig();
		bool read(const std::string &filename);

		const Entry& get(const std::string &net, const std::string &sta) const;

	private:
		std::map<std::string, Entry> _entry;
};

}  // namespace Autoloc

}  // namespace Seiscomp

#endif
