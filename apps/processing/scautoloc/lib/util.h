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

#include "lib/datamodel.h"

namespace Seiscomp {

namespace Autoloc {

double distance(const Autoloc::DataModel::Station* s1, const Autoloc::DataModel::Station* s2);
void delazi(double lat1, double lon1, double lat2, double lon2, double &delta, double &az1, double &az2);
void delazi(const Autoloc::DataModel::Hypocenter*, const Autoloc::DataModel::Station*, double &delta, double &az1, double &az2);

double originScore(const Autoloc::DataModel::Origin *origin, double maxRMS=3.5, double radius=0.);
bool determineAzimuthalGaps(const Autoloc::DataModel::Origin*, double *primary, double *secondary);

std::string printDetailed(const Autoloc::DataModel::Origin*);
std::string printOneliner(const Autoloc::DataModel::Origin*);
std::string printOrigin(const Autoloc::DataModel::Origin *origin, bool=false);


double meandev(const Autoloc::DataModel::Origin* origin);

double avgfn(double x);

typedef Seiscomp::TravelTime TravelTime;
bool travelTimeP (double lat1, double lon1, double dep1, double lat2, double lon2, double alt2, double delta, TravelTime&);

// 1st arrival P incl. Pdiff up to 130 deg, no PKP
//bool travelTimeP1(double lat1, double lon1, double dep1, double lat2, double lon2, double alt2, double delta, TravelTime&);

// 1st arrival PK* incl. PKP*, PKiKP
//bool travelTimePK(double lat1, double lon1, double dep1, double lat2, double lon2, double alt2, double delta, TravelTime&);


TravelTime travelTimePP(double lat1, double lon1, double dep1, double lat2, double lon2, double alt2, double delta);

std::string time2str(const Autoloc::DataModel::Time &t);
Seiscomp::Core::Time sctime(const Autoloc::DataModel::Time &time);

Autoloc::DataModel::PickVector readPickFile();
Autoloc::DataModel::Pick* readPickLine();

// FIXME: mixed bag!
Autoloc::DataModel::Pick::Status status(const Seiscomp::DataModel::Pick *pick);

} // namespace Autoloc


namespace Math {
namespace Statistics {

double rms(const std::vector<double> &v, double offset = 0);

} // namespace Statistics
} // namespace Math

} // namespace Seiscomp
