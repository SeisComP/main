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


#define SEISCOMP_COMPONENT Autoloc

#include "depthlookup.h"

#include <seiscomp/logging/log.h>
#include <seiscomp/geo/featureset.h>

#include <algorithm>
#include <filesystem>
#include <regex>


namespace fs = std::filesystem;

namespace Seiscomp {
namespace Processing {


// ---------------------------------------------------------------------------
// ConstantDepthLookup
// ---------------------------------------------------------------------------

ConstantDepthLookup::ConstantDepthLookup(double defaultDepth, double maxDepth)
: _defaultDepth(defaultDepth), _maxDepth(maxDepth) {}

double ConstantDepthLookup::fetch(double /*lat*/, double /*lon*/) const {
	return _defaultDepth;
}

double ConstantDepthLookup::fetchMaxDepth(double /*lat*/, double /*lon*/) const {
	return _maxDepth;
}


// ---------------------------------------------------------------------------
// Slab2DepthLookup
// ---------------------------------------------------------------------------

Slab2DepthLookup::Slab2DepthLookup(const std::string &slabDir,
                                   double fallbackDepth,
                                   double fallbackMaxDepth)
: _slabDir(slabDir)
, _fallbackDepth(fallbackDepth)
, _fallbackMaxDepth(fallbackMaxDepth)
, _features(new Geo::GeoFeatureSet()) {}

Slab2DepthLookup::~Slab2DepthLookup() {
	delete static_cast<Geo::GeoFeatureSet *>(_features);
}

bool Slab2DepthLookup::init() {
	if ( _slabDir.empty() ) {
		SEISCOMP_ERROR("DepthLookup/Slab2: slabDir is not configured");
		return false;
	}

	if ( !fs::is_directory(_slabDir) ) {
		SEISCOMP_ERROR("DepthLookup/Slab2: directory not found: %s",
		               _slabDir.c_str());
		return false;
	}

	auto *features = static_cast<Geo::GeoFeatureSet *>(_features);
	std::map<std::string, SlabZone> zoneMap;

	// Iterate depth-level subdirectories: 000, 025, 050, ...
	std::vector<fs::path> depthDirs;
	for ( const auto &entry : fs::directory_iterator(_slabDir) ) {
		if ( entry.is_directory() )
			depthDirs.push_back(entry.path());
	}
	std::sort(depthDirs.begin(), depthDirs.end());

	const std::regex bnaPattern(R"(^(.+)\.bna$)");
	size_t totalLoaded = 0;

	for ( const auto &depthDir : depthDirs ) {
		const std::string dirName = depthDir.filename().string();
		int depthKm = 0;
		try { depthKm = std::stoi(dirName); }
		catch (...) { continue; }

		for ( const auto &entry : fs::directory_iterator(depthDir) ) {
			if ( !entry.is_regular_file() ) continue;

			std::string fname = entry.path().filename().string();
			std::smatch m;
			if ( !std::regex_match(fname, m, bnaPattern) ) continue;

			const std::string zoneName = m[1].str();
			const std::string featureName = zoneName + " " + dirName + " km";

			if ( features->readFile(entry.path().string(), nullptr) >= 0 ) {
				zoneMap[zoneName].name = zoneName;
				zoneMap[zoneName].levels.push_back({depthKm, featureName});
				if ( depthKm > zoneMap[zoneName].maxDepthKm )
					zoneMap[zoneName].maxDepthKm = depthKm;
				++totalLoaded;
			}
		}
	}

	if ( totalLoaded == 0 ) {
		SEISCOMP_ERROR("DepthLookup/Slab2: no BNA files loaded from %s",
		               _slabDir.c_str());
		return false;
	}

	// Sort each zone's levels descending by depth (deepest first for lookup)
	for ( auto &[name, zone] : zoneMap ) {
		std::sort(zone.levels.begin(), zone.levels.end(),
		          [](const SlabLevel &a, const SlabLevel &b) {
		              return a.depthKm > b.depthKm;
		          });
		_zones.push_back(std::move(zone));
	}

	SEISCOMP_INFO("DepthLookup/Slab2: loaded %zu BNA files across %zu zones from %s",
	              totalLoaded, _zones.size(), _slabDir.c_str());
	return true;
}


double Slab2DepthLookup::_lookupDepth(double lat, double lon) const {
	const auto *features = static_cast<const Geo::GeoFeatureSet *>(_features);
	const Geo::GeoCoordinate coord(lat, lon);

	for ( const auto &zone : _zones ) {
		// Scan deepest → shallowest; return depth of deepest level containing point
		for ( const auto &level : zone.levels ) {
			const Geo::GeoFeature *feature = nullptr;
			for ( const auto *f : features->features() ) {
				if ( f && f->name() == level.featureName ) {
					feature = f;
					break;
				}
			}
			if ( feature && feature->contains(coord) )
				return static_cast<double>(level.depthKm);
		}
	}

	return -1.0;  // not in any slab zone
}


double Slab2DepthLookup::fetch(double lat, double lon) const {
	double d = _lookupDepth(lat, lon);
	return d >= 0.0 ? d : _fallbackDepth;
}


double Slab2DepthLookup::fetchMaxDepth(double lat, double lon) const {
	// Inside a slab zone: max depth is the zone's deepest contour level
	const auto *features = static_cast<const Geo::GeoFeatureSet *>(_features);
	const Geo::GeoCoordinate coord(lat, lon);

	for ( const auto &zone : _zones ) {
		// Check if point is inside the shallowest (outermost) level
		if ( zone.levels.empty() ) continue;
		const auto &shallowest = zone.levels.back();
		const Geo::GeoFeature *feature = nullptr;
		for ( const auto *f : features->features() ) {
			if ( f && f->name() == shallowest.featureName ) {
				feature = f;
				break;
			}
		}
		if ( feature && feature->contains(coord) )
			return static_cast<double>(zone.maxDepthKm);
	}

	return _fallbackMaxDepth;
}


// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

DepthLookupPtr makeDepthLookup(const std::string &type,
                               double defaultDepth,
                               double maxDepth,
                               const std::string &slabDir) {
	if ( type == "Slab2" ) {
		auto backend = std::make_unique<Slab2DepthLookup>(
		    slabDir, defaultDepth, maxDepth);
		if ( !backend->init() ) {
			SEISCOMP_ERROR("DepthLookup: Slab2 backend failed to initialize "
			               "— falling back to Constant");
			return std::make_unique<ConstantDepthLookup>(defaultDepth, maxDepth);
		}
		return backend;
	}

	if ( type != "Constant" ) {
		SEISCOMP_WARNING("DepthLookup: unknown type '%s' — using Constant",
		                 type.c_str());
	}

	return std::make_unique<ConstantDepthLookup>(defaultDepth, maxDepth);
}


}  // namespace Autoloc
}  // namespace Seiscomp
