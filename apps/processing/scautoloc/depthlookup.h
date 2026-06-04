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


#ifndef SEISCOMP_AUTOLOC_DEPTHLOOKUP_H
#define SEISCOMP_AUTOLOC_DEPTHLOOKUP_H


#include <memory>
#include <string>
#include <vector>


namespace Seiscomp {

namespace Processing {


/**
 * Abstract interface for location-dependent default/maximum depth lookup.
 *
 * Compiled directly into scautoloc — no plugin loader, no Client::Application
 * dependency. Safe to use from standalone code and Python scripts.
 *
 * Backends:
 *   Constant — fixed values from config (default; existing behaviour)
 *   Slab2    — USGS Slab2.0 depth-footprint contours (geographic)
 */
class DepthLookup {
	public:
		virtual ~DepthLookup() = default;

		// Returns the default depth in km for the given location.
		virtual double fetch(double lat, double lon) const = 0;

		// Returns the maximum allowed depth in km for the given location.
		virtual double fetchMaxDepth(double lat, double lon) const = 0;
};

using DepthLookupPtr = std::unique_ptr<DepthLookup>;


// ---------------------------------------------------------------------------
// Constant backend
// Returns fixed values taken from AutolocConfig.defaultDepth / maxDepth.
// This is the default and preserves existing behaviour exactly.
// ---------------------------------------------------------------------------
class ConstantDepthLookup : public DepthLookup {
	public:
		ConstantDepthLookup(double defaultDepth, double maxDepth);

		double fetch(double lat, double lon) const override;
		double fetchMaxDepth(double lat, double lon) const override;

	private:
		double _defaultDepth;
		double _maxDepth;
};


// ---------------------------------------------------------------------------
// Slab2 backend
// Looks up depth from USGS Slab2.0 depth-footprint contour BNA files.
// Falls back to fallbackDepth / fallbackMaxDepth when outside all slab zones.
// ---------------------------------------------------------------------------
class Slab2DepthLookup : public DepthLookup {
	public:
		// slabDir:        path to directory containing subdirs 000/, 025/, ...
		// fallbackDepth:  depth returned outside all slab zones
		// fallbackMaxDepth: maxDepth returned outside all slab zones
		Slab2DepthLookup(const std::string &slabDir,
		                 double fallbackDepth,
		                 double fallbackMaxDepth);
		~Slab2DepthLookup() override;

		// Returns false if slabDir is empty or contains no valid BNA files.
		bool init();

		double fetch(double lat, double lon) const override;
		double fetchMaxDepth(double lat, double lon) const override;

	private:
		struct SlabLevel {
			int         depthKm;
			std::string featureName;
		};
		struct SlabZone {
			std::string            name;
			std::vector<SlabLevel> levels;  // sorted descending by depthKm
			int                    maxDepthKm{0};
		};

		double _lookupDepth(double lat, double lon) const;

		std::string            _slabDir;
		double                 _fallbackDepth;
		double                 _fallbackMaxDepth;
		std::vector<SlabZone>  _zones;
		void                  *_features{nullptr};  // opaque GeoFeatureSet*
};


// ---------------------------------------------------------------------------
// Factory — instantiates the right backend from AutolocConfig fields.
// depthLookupType:  "Constant" (default) or "Slab2"
// defaultDepth:     used by Constant backend and as Slab2 fallback
// maxDepth:         used by Constant backend and as Slab2 fallback
// slabDir:          directory for Slab2 BNA files (ignored for Constant)
// ---------------------------------------------------------------------------
DepthLookupPtr makeDepthLookup(const std::string &type,
                               double defaultDepth,
                               double maxDepth,
                               const std::string &slabDir = "");


}  // namespace Processing

}  // namespace Seiscomp

#endif
