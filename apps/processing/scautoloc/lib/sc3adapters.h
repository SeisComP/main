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




// Seiscomp::DataModel
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/pick.h>

// Seiscomp::Autoloc::DataModel
#include "datamodel.h"

namespace Seiscomp {

namespace Autoloc {

// export an Autoloc::DataModel::Origin to a Seiscomp::DataModel::Origin
Seiscomp::DataModel::Origin *exportToSC(const Autoloc::DataModel::Origin*, bool allPhases=true);

} // namespace Autoloc

} // namespace Seiscomp
