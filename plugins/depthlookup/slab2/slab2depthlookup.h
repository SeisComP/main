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


#ifndef SEISCOMP_PLUGIN_SLAB2DEPTHLOOKUP_H
#define SEISCOMP_PLUGIN_SLAB2DEPTHLOOKUP_H


#include <seiscomp/seismology/depthlookup.h>
#include <seiscomp/geo/featureset.h>

#include <string>
#include <vector>


class Slab2DepthLookup : public Seiscomp::Seismology::DepthLookup {
	public:
		bool init(const Seiscomp::Config::Config &config) override;

		double getDefaultDepth(double lat, double lon,
		                       double fallback) const override;

		double getMaxDepth(double lat, double lon,
		                   double fallback) const override;

	private:
		struct SlabLevel {
			int                                              depthKm{0};
			std::vector<const Seiscomp::Geo::GeoFeature *>  features;
		};

		struct SlabZone {
			std::string              name;
			std::vector<SlabLevel>   levels;   // sorted ascending by depthKm
			int                      maxDepthKm{0};
		};

		int _slabDepthAt(double lat, double lon) const;

		std::vector<SlabZone>        _slabZones;
		Seiscomp::Geo::GeoFeatureSet _slabFeatures;  // local — NOT the global singleton
};


#endif  // SEISCOMP_PLUGIN_SLAB2DEPTHLOOKUP_H
