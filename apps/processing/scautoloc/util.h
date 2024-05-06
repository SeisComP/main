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

namespace Autoloc {

void delazi(double lat1, double lon1, double lat2, double lon2, double &delta, double &az1, double &az2);
void delazi(const Hypocenter *hypo, const Station *station, double &delta, double &az1, double &az2);
double distance(const Station* s1, const Station* s2);
std::string printDetailed(const Origin*);
std::string printOneliner(const Origin*);
bool automatic(const Pick*);
bool ignored(const Pick*);
bool manual(const Pick*);
char modeFlag(const Pick*);
bool hasAmplitude(const Pick*);


double meandev(const Origin* origin);

double avgfn(double x);

std::string printOrigin(const Origin *origin, bool=false);
int numberOfDefiningPhases(const Origin &origin);

typedef Seiscomp::TravelTime TravelTime;
bool travelTimeP (double lat1, double lon1, double dep1, double lat2, double lon2, double alt2, double delta, TravelTime&);

std::string time2str(const Time &t);

namespace Utils {

StationMap *readStationLocations(const std::string &fname);
Seiscomp::DataModel::Inventory* inventoryFromStationLocationFile(const std::string &_stationLocationFile);

//bool readStationConfig(StationMap *stations, const std::string &fname);
PickVector readPickFile();
Pick*      readPickLine();
Pick::Mode mode(const Seiscomp::DataModel::Pick *pick);

}

}






namespace Seiscomp {
namespace Math {
namespace Statistics {

double rms(const std::vector<double> &v, double offset = 0);

} // namespace Statistics
} // namespace Math
} // namespace Seiscomp

