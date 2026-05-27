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

namespace Seiscomp {

DataModel::Origin *convertToSC(const Autoloc::Origin* origin,  const std::string &author, const std::string &agencyID, bool allPhases=true);
// Origin *convertFromSC(const DataModel::Origin* scorigin);

}  // namespace Seiscomp
