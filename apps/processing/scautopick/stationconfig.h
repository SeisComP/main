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


#ifndef APPS_PICKER_STATIONCONFIG_H
#define APPS_PICKER_STATIONCONFIG_H

#include <seiscomp/datamodel/configmodule.h>
#include <seiscomp/utils/keyvalues.h>

#include <string>
#include <map>


namespace Seiscomp {
namespace Applications {
namespace Picker {


struct StreamConfig {
	StreamConfig();
	StreamConfig(double on, double off, double tcorr,
	             const std::string &f);

	std::string  locationCode;
	std::string  channel;

	OPT(double)  triggerOn;
	OPT(double)  triggerOff;
	OPT(double)  timeCorrection;
	bool         sensitivityCorrection;
	bool         enabled;

	std::string  filter;

	bool         updatable;

	Util::KeyValuesPtr parameters;
};


class StationConfig {
	public:
		typedef std::pair<std::string, std::string> Key;
		typedef std::map<Key, StreamConfig> ConfigMap;
		typedef ConfigMap::const_iterator const_iterator;


	public:
		StationConfig();

		void setDefault(const StreamConfig &entry);

		const StreamConfig *read(const Seiscomp::Config::Config *config, const std::string &mod,
		                         DataModel::ParameterSet *params,
		                         const std::string &net, const std::string &sta);

		void read(const Seiscomp::Config::Config *config, const DataModel::ConfigModule *module,
		          const std::string &setup);

		const StreamConfig *get(const Seiscomp::Config::Config *config, const std::string &mod,
		                        const std::string &net, const std::string &sta);

		const_iterator begin() const;
		const_iterator end() const;

		void dump() const;


	private:
		StreamConfig _default;
		ConfigMap _stationConfigs;
};


}
}
}


#endif
