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


#define SEISCOMP_COMPONENT Slab2DepthLookup

#include "slab2depthlookup.h"

#include <seiscomp/core/strings.h>
#include <seiscomp/geo/feature.h>
#include <seiscomp/geo/featureset.h>
#include <seiscomp/logging/log.h>
#include <seiscomp/system/environment.h>

#include <algorithm>
#include <map>
#include <optional>
#include <string>

#include <boost/filesystem.hpp>


REGISTER_DEPTH_LOOKUP(Slab2DepthLookup, "Slab2");


namespace {

std::optional<double> parseAttr(const Seiscomp::Geo::GeoFeature *f,
                                const std::string &key) {
	const auto &attrs = f->attributes();
	auto it = attrs.find(key);
	if ( it == attrs.end() || it->second.empty() ) {
		return std::nullopt;
	}
	double v;
	if ( !Seiscomp::Core::fromString(v, it->second) ) {
		return std::nullopt;
	}
	return v;
}

} // anonymous namespace


// ---------------------------------------------------------------------------
bool Slab2DepthLookup::init(const Seiscomp::Config::Config &config) {
	std::string directory;
	try {
		directory = config.getString("dlslab2.directory");
	}
	catch ( ... ) {
		SEISCOMP_WARNING("dlslab2: no directory configured under dlslab2.directory");
		return false;
	}

	auto *env = Seiscomp::Environment::Instance();
	if ( env ) {
		directory = env->resolvePath(directory);
	}

	try {
		_fallback = config.getDouble("dlslab2.fallback");
	}
	catch ( ... ) {
		SEISCOMP_INFO("dlslab2: dlslab2.fallback not set, using default %.0f km",
		              _fallback);
	}

	namespace fs = boost::filesystem;

	if ( !fs::is_directory(directory) ) {
		SEISCOMP_WARNING("dlslab2: slab directory not found: %s",
		                 directory.c_str());
		return false;
	}

	_slabZones.clear();
	_slabFeatures.clear();

	std::map<std::string, size_t> zoneIndex;

	std::vector<fs::path> depthDirs;
	for ( const auto &entry : fs::directory_iterator(directory) ) {
		if ( fs::is_directory(entry.path()) ) {
			depthDirs.push_back(entry.path());
		}
	}
	std::sort(depthDirs.begin(), depthDirs.end());

	for ( const auto &depthDir : depthDirs ) {
		const std::string dirName = depthDir.filename().string();

		int depthKm = -1;
		try {
			size_t pos;
			int v = std::stoi(dirName, &pos);
			if ( pos == dirName.size() ) {
				depthKm = v;
			}
		}
		catch ( ... ) {}

		if ( depthKm < 0 ) {
			continue;
		}

		for ( const auto &fileEntry : fs::directory_iterator(depthDir) ) {
			const fs::path &fp = fileEntry.path();
			if ( fp.extension() != ".bna" ) {
				continue;
			}

			const std::string zoneName = fp.stem().string();
			size_t beforeCount = _slabFeatures.features().size();

			ssize_t n = _slabFeatures.readFile(fp.string(), nullptr);
			if ( n <= 0 ) {
				SEISCOMP_WARNING("dlslab2: could not read %s", fp.string().c_str());
				continue;
			}

			// BNA files with negative vertex counts are not auto-marked as closed
			// polygons by the reader, but GeoFeature::contains() requires it.
			const auto &allFeatures = _slabFeatures.features();
			std::vector<const Seiscomp::Geo::GeoFeature *> levelFeatures;
			for ( size_t i = beforeCount; i < allFeatures.size(); ++i ) {
				allFeatures[i]->setClosedPolygon(true);
				levelFeatures.push_back(allFeatures[i]);
			}

			auto result = zoneIndex.emplace(zoneName, _slabZones.size());
			if ( result.second ) {
				_slabZones.push_back(SlabZone{});
				_slabZones.back().name = zoneName;
			}
			SlabZone &zone = _slabZones[result.first->second];

			SlabLevel level;
			level.depthKm  = depthKm;
			level.features = std::move(levelFeatures);
			zone.levels.push_back(std::move(level));
		}
	}

	for ( SlabZone &zone : _slabZones ) {
		std::sort(zone.levels.begin(), zone.levels.end(),
		          [](const SlabLevel &a, const SlabLevel &b) {
		              return a.depthKm < b.depthKm;
		          });
		if ( !zone.levels.empty() ) {
			zone.maxDepthKm = zone.levels.back().depthKm;
		}

		SEISCOMP_INFO("dlslab2: zone '%s' — %zu level(s), max %d km",
		              zone.name.c_str(), zone.levels.size(), zone.maxDepthKm);
	}

	SEISCOMP_INFO("dlslab2: %zu zone(s) loaded from %s",
	              _slabZones.size(), directory.c_str());

	// Load supplementary polygon regions (consulted when a point is outside
	// all slab zones). Config key: dlslab2.polygon.regions
	_polyRegions.clear();
	std::vector<std::string> polyNames;
	try {
		polyNames = config.getStrings("dlslab2.polygon.regions");
	}
	catch ( ... ) {}

	if ( !polyNames.empty() ) {
		const Seiscomp::Geo::GeoFeatureSet &fs =
		    Seiscomp::Geo::GeoFeatureSetSingleton::getInstance();

		for ( const auto *f : fs.features() ) {
			if ( !f->closedPolygon() ) {
				continue;
			}
			if ( std::find(polyNames.begin(), polyNames.end(), f->name())
			         == polyNames.end() ) {
				continue;
			}
			auto dd = parseAttr(f, "defaultDepth");
			if ( !dd ) {
				SEISCOMP_WARNING("dlslab2: polygon region '%s' has no "
				                 "defaultDepth attribute — skipped",
				                 f->name().c_str());
				continue;
			}
			_polyRegions.push_back({f, *dd, parseAttr(f, "maxDepth")});
			SEISCOMP_INFO("dlslab2: polygon region '%s' defaultDepth=%.0f km",
			              f->name().c_str(), *dd);
		}
		SEISCOMP_INFO("dlslab2: %zu supplementary polygon region(s) loaded",
		              _polyRegions.size());
	}

	return !_slabZones.empty();
}


// ---------------------------------------------------------------------------
int Slab2DepthLookup::_slabDepthAt(double lat, double lon) const {
	const Seiscomp::Geo::GeoCoordinate loc(lat, lon);

	for ( const SlabZone &zone : _slabZones ) {
		for ( auto it = zone.levels.rbegin(); it != zone.levels.rend(); ++it ) {
			for ( const auto *f : it->features ) {
				if ( f->contains(loc) ) {
					SEISCOMP_DEBUG("dlslab2: slab '%s' depth %d km at (%.2f, %.2f)",
					               zone.name.c_str(), it->depthKm, lat, lon);
					return it->depthKm;
				}
			}
		}
	}
	return -1;
}


// ---------------------------------------------------------------------------
double Slab2DepthLookup::fetch(double lat, double lon) const {
	int d = _slabDepthAt(lat, lon);
	if ( d >= 0 ) {
		return static_cast<double>(d);
	}

	const Seiscomp::Geo::GeoCoordinate loc(lat, lon);
	for ( const PolyRegion &r : _polyRegions ) {
		if ( r.feature->contains(loc) ) {
			SEISCOMP_DEBUG("dlslab2: polygon region '%s' depth %.0f km at (%.2f, %.2f)",
			               r.feature->name().c_str(), r.defaultDepth, lat, lon);
			return r.defaultDepth;
		}
	}

	return _fallback;
}


// ---------------------------------------------------------------------------
double Slab2DepthLookup::fetchMaxDepth(double lat, double lon) const {
	const Seiscomp::Geo::GeoCoordinate loc(lat, lon);

	for ( const SlabZone &zone : _slabZones ) {
		if ( zone.levels.empty() ) {
			continue;
		}
		for ( const auto *f : zone.levels.front().features ) {
			if ( f->contains(loc) ) {
				return static_cast<double>(zone.maxDepthKm);
			}
		}
	}

	for ( const PolyRegion &r : _polyRegions ) {
		if ( r.feature->contains(loc) ) {
			return r.maxDepth.value_or(_fallback);
		}
	}

	return _fallback;
}
