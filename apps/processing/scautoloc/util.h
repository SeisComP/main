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

double distance(const Station* s1, const Station* s2);
std::string printDetailed(const Origin*);
std::string printOneliner(const Origin*);
bool automatic(const Pick*);
bool ignored(const Pick*);
bool manual(const Pick*);
char statusFlag(const Pick*);
bool hasAmplitude(const Pick*);


double meandev(const Origin* origin);

double avgfn(double x);

std::string printOrigin(const Origin *origin, bool=false);
int numberOfDefiningPhases(const Origin &origin);

typedef Seiscomp::TravelTime TravelTime;
bool travelTimeP (double lat1, double lon1, double dep1, double lat2, double lon2, double alt2, double delta, TravelTime&);

// 1st arrival P incl. Pdiff up to 130 deg, no PKP
bool travelTimeP1(double lat1, double lon1, double dep1, double lat2, double lon2, double alt2, double delta, TravelTime&);

// 1st arrival PK* incl. PKP*, PKiKP
bool travelTimePK(double lat1, double lon1, double dep1, double lat2, double lon2, double alt2, double delta, TravelTime&);


TravelTime travelTimePP(double lat1, double lon1, double dep1, double lat2, double lon2, double alt2, double delta);

std::string time2str(const Time &t);

namespace Utils {

StationMap *readStationLocations(const std::string &fname);
Seiscomp::DataModel::Inventory* inventoryFromStationLocationFile(const std::string &_stationLocationFile);

//bool readStationConfig(StationMap *stations, const std::string &fname);
PickVector readPickFile();
Pick*      readPickLine();
Pick::Status status(const Seiscomp::DataModel::Pick *pick);

}

}






namespace Seiscomp {
namespace Math {
namespace Statistics {

double rms(const std::vector<double> &v, double offset = 0);

} // namespace Statistics
} // namespace Math
} // namespace Seiscomp

