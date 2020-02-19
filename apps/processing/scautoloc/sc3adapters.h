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
#include <seiscomp/datamodel/origin.h>

#include "datamodel.h"

namespace Autoloc {

void delazi(double lat1, double lon1, double lat2, double lon2, double &delta, double &az1, double &az2);
void delazi(const Hypocenter*, const Station*, double &delta, double &az1, double &az2);

Seiscomp::Core::Time sc3time(const Time &time);

Seiscomp::DataModel::Origin *convertToSC3(const Origin* origin, bool allPhases=true);
// Origin *convertFromSC3(const Seiscomp::DataModel::Origin* sc3origin);

// Pick *convertFromSC3(const Seiscomp::DataModel::Pick *sc3pick);

} // namespace Autoloc

