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




# define SEISCOMP_COMPONENT Autoloc
#include <seiscomp/logging/log.h>

#include "lib/stationlocationfile.h"

#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <string>

namespace Seiscomp {

namespace Autoloc {

namespace Util {


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Seiscomp::DataModel::Inventory* inventoryFromStationLocationFile(const std::string &filename)
{
	Seiscomp::DataModel::Inventory *inventory = new Seiscomp::DataModel::Inventory;
	std::ifstream ifile(filename.c_str());

	std::string sta, net;
	double lat, lon, alt;

	while (ifile >> net >> sta >> lat >> lon >> alt) {
		std::string netId = "Network/"+net;
		Seiscomp::DataModel::Network *network = inventory->findNetwork(netId);
		if ( ! network) {
			network = new Seiscomp::DataModel::Network(netId);
			network->setCode(net);
			inventory->add(network);
		}

		std::string staId = "Station/"+net+"/"+sta;
		Seiscomp::DataModel::Station *station = new Seiscomp::DataModel::Station(staId);
		station->setCode(sta);
		station->setLatitude(lat);
		station->setLongitude(lon);
		station->setElevation(alt);
		network->add(station);
	}

	return inventory;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


} // namespace Util

} // namespace Autoloc

} // namespace Seiscomp
