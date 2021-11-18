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


#ifndef SEISCOMP_AUTOLOC_STATIONLOCATIONFILE_H_INCLUDED
#define SEISCOMP_AUTOLOC_STATIONLOCATIONFILE_H_INCLUDED

#include <seiscomp/datamodel/inventory.h>
#include <string>


namespace Seiscomp {

namespace Autoloc {

namespace Util {

// Read an inventory from a text file consisting of lines of
// network code, station code, latitude, longitude, elevation

Seiscomp::DataModel::Inventory* inventoryFromStationLocationFile(const std::string &filename);

} // namespace Util

} // namespace Autoloc

} // namespace Seiscomp

#endif
