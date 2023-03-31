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


#define SEISCOMP_COMPONENT MN

#include <seiscomp/logging/log.h>
#include <seiscomp/system/environment.h>
#include <seiscomp/math/geo.h>

#include <boost/thread/mutex.hpp>

#include "./regions.h"

//#define TEST_WITHOUT_REGIONS


namespace Seiscomp {
namespace Magnitudes {
namespace MN {


namespace {


#ifndef TEST_WITHOUT_REGIONS
bool validRegionInitialized = false;
Seiscomp::Geo::GeoFeatureSet validRegion;
boost::mutex regionMutex;
#endif


}


bool initialize(const Config::Config *config) {
#ifndef TEST_WITHOUT_REGIONS
	boost::mutex::scoped_lock l(regionMutex);

	if ( !validRegionInitialized ) {
		// Don't try again
		validRegionInitialized = true;

		std::string filename;

		try {
			filename = config->getString("magnitudes.MN.region");
		}
		catch ( ... ) {
			filename = "@DATADIR@/magnitudes/MN/MN.bna";
		}

		filename = Seiscomp::Environment::Instance()->absolutePath(filename);

		if ( !validRegion.readFile(filename, nullptr) ) {
			SEISCOMP_ERROR("Failed to read/parse MN region file: %s",
			               filename.c_str());
			return false;
		}
	}
	else if ( validRegion.features().empty() ) {
		// No region defined, nothing to do
		SEISCOMP_ERROR("No regions defined in amplitudes.MN.region file");
		return false;
	}

#endif
	return true;
}


bool isInsideRegion(double lat0, double lon0,
                    double lat1, double lon1) {
#ifndef TEST_WITHOUT_REGIONS
	double dist, az, baz;

	Math::Geo::delazi_wgs84(lat0, lon0, lat1, lon1, &dist, &az, &baz);

	// Convert to km
	dist = Math::Geo::deg2km(dist);

	// Check path each 10 km
	int steps = dist / 10;

	for ( int i = 1; i < steps; ++i ) {
		Math::Geo::delandaz2coord(Math::Geo::km2deg(dist*i/steps), az, lat0, lon0,
		                          &lat1, &lon1);
		if ( !isInsideRegion(lat1, lon1) )
			return false;
	}
#endif
	return true;
}


bool isInsideRegion(double lat, double lon) {
#ifndef TEST_WITHOUT_REGIONS
	boost::mutex::scoped_lock l(regionMutex);
	size_t numFeatures = validRegion.features().size();
	for ( size_t i = 0; i < numFeatures; ++i ) {
		Seiscomp::Geo::GeoFeature *feature = validRegion.features()[i];
		if ( feature->contains(Seiscomp::Geo::Vertex(lat, lon)) )
			return true;
	}

	return false;
#else
	return true;
#endif
}


}
}
}
