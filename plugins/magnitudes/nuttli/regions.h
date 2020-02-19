/***************************************************************************
 * Copyright (C) gempa GmbH                                                *
 * All rights reserved.                                                    *
 * Contact: gempa GmbH (seiscomp-dev@gempa.de)                             *
 *                                                                         *
 * Author: Jan Becker                                                      *
 * Email: jabe@gempa.de                                                    *
 *                                                                         *
 * GNU Affero General Public License Usage                                 *
 * This file may be used under the terms of the GNU Affero                 *
 * Public License version 3.0 as published by the Free Software Foundation *
 * and appearing in the file LICENSE included in the packaging of this     *
 * file. Please review the following information to ensure the GNU Affero  *
 * Public License version 3.0 requirements will be met:                    *
 * https://www.gnu.org/licenses/agpl-3.0.html.                             *
 *                                                                         *
 * Other Usage                                                             *
 * Alternatively, this file may be used in accordance with the terms and   *
 * conditions contained in a signed written agreement between you and      *
 * gempa GmbH.                                                             *
 ***************************************************************************/


#ifndef SEISCOMP_CUSTOM_REGIONS_NUTTLI_H__
#define SEISCOMP_CUSTOM_REGIONS_NUTTLI_H__


#include <seiscomp/core/version.h>
#include <seiscomp/config/config.h>
#include <seiscomp/geo/featureset.h>


namespace Seiscomp {
namespace Magnitudes {
namespace MN {


bool initialize(const Config::Config *config);

bool isInsideRegion(double lat0, double lon0,
                    double lat1, double lon1);

bool isInsideRegion(double lat, double lon);


}
}
}


#endif
