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

#include <optional>
#include <string>
#include <vector>


class Slab2DepthLookup : public Seiscomp::Seismology::DepthLookup {
	public:
		bool init(const Seiscomp::Config::Config &config) override;

		double fetch(double lat, double lon) const override;

		double fetchMaxDepth(double lat, double lon) const override;

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

		// Supplementary polygon regions consulted when a point falls outside
		// all slab zones. Loaded from dlslab2.polygon.regions.
		struct PolyRegion {
			const Seiscomp::Geo::GeoFeature *feature{nullptr};
			double                           defaultDepth{0.0};
			std::optional<double>            maxDepth;
		};

		int _slabDepthAt(double lat, double lon) const;

		double                       _fallback{10.0};
		std::vector<SlabZone>        _slabZones;
		Seiscomp::Geo::GeoFeatureSet _slabFeatures;  // local — NOT the global singleton
		std::vector<PolyRegion>      _polyRegions;   // from global GeoFeatureSet singleton
};


#endif  // SEISCOMP_PLUGIN_SLAB2DEPTHLOOKUP_H
