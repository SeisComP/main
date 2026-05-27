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




#include <string>
#include <vector>
#include <seiscomp/core/datetime.h>
#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/inventory.h>
#include <seiscomp/seismology/ttt.h>

#include "datamodel.h"

namespace Seiscomp {

namespace Autoloc {

// Compute the distance in degrees between two stations on a sphere
double distance(
	const Autoloc::Station* s1,
	const Autoloc::Station* s2);

// Compute the distance and azimuths in degrees between two points on a sphere
void delazi(
	double lat1, double lon1,
	double lat2, double lon2,
	double &delta, double &az1, double &az2);

// Compute the distance and azimuths in degrees between a hypocenter and a
// station on a sphere.
void delazi(
	const Autoloc::Hypocenter*,
	const Autoloc::Station*,
	double &delta, double &az1, double &az2);


// Various formatters to generate debug output for scautoloc
std::string printDetailed(const Autoloc::Origin*);
std::string printOneliner(const Autoloc::Origin*);
std::string printOrigin(const Autoloc::Origin *origin, bool=false);

double meandev(const Autoloc::Origin* origin);

double avgfn(double x);

// Compute the P travel time between two points on a spherical Earth.
typedef Seiscomp::TravelTime TravelTime;

bool travelTimeP (
	double lat1, double lon1, double dep1,
	double lat2, double lon2, double alt2,
	double delta, TravelTime&);


// Format an Autoloc::DataModel::Time time as time stamp.
std::string time2str(const Autoloc::Time &t);

// Convert an Autoloc::DataModel::Time time to a Seiscomp::Core::Time
Seiscomp::Core::Time sctime(const Autoloc::Time &time);


bool automatic(const Pick*);
bool ignored(const Pick*);
bool manual(const Pick*);
char modeFlag(const Pick*);
bool hasAmplitude(const Pick*);

double meandev(const Origin* origin);

int numberOfDefiningPhases(const Origin &origin);


namespace Utils {

StationMap *readStationLocations(const std::string &fname);
Seiscomp::DataModel::Inventory* inventoryFromStationLocationFile(const std::string &_stationLocationFile);

//bool readStationConfig(StationMap *stations, const std::string &fname);
PickVector readPickFile();
Pick*      readPickLine();
Pick::Mode mode(const Seiscomp::DataModel::Pick *pick);

void minimizeInventory(Seiscomp::DataModel::Inventory *inventory);

}  // namespace Utils

}  // namespace Autoloc



namespace Math {

namespace Statistics {

double rms(const std::vector<double> &v, double offset = 0);

} // namespace Statistics

} // namespace Math

} // namespace Seiscomp
