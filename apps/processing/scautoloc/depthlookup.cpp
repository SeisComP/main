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
#include <seiscomp/geo/feature.h>
#include <seiscomp/geo/featureset.h>
#include <seiscomp/core/strings.h>

#include <algorithm>
#include <filesystem>
#include <map>
#include <optional>
#include <regex>


namespace fs = std::filesystem;

namespace Seiscomp {
namespace Processing {


// ---------------------------------------------------------------------------
// DepthLookup base — default implementations
// ---------------------------------------------------------------------------

bool DepthLookup::covers(double /*lat*/, double /*lon*/) const {
	return true;
}

std::vector<double> DepthLookup::fetchCandidateDepths(double lat, double lon) const {
	return {fetch(lat, lon)};
}


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
// PolygonDepthLookup
// ---------------------------------------------------------------------------

namespace {

struct PolygonEntry {
	const Geo::GeoFeature *feature{nullptr};
	double defaultDepth{0.0};
	double maxDepth{-1.0};  // -1 means not set, use fallback
};

std::optional<double> parseAttr(const Geo::GeoFeature *f, const std::string &key) {
	const auto &attrs = f->attributes();
	auto it = attrs.find(key);
	if ( it == attrs.end() || it->second.empty() ) return std::nullopt;
	double v;
	if ( !Core::fromString(v, it->second) ) return std::nullopt;
	return v;
}

} // anonymous namespace


PolygonDepthLookup::PolygonDepthLookup(const std::vector<std::string> &regions,
                                       double fallbackDepth,
                                       double fallbackMaxDepth)
: _regions(regions)
, _fallbackDepth(fallbackDepth)
, _fallbackMaxDepth(fallbackMaxDepth)
, _entries(new std::vector<PolygonEntry>()) {}

PolygonDepthLookup::~PolygonDepthLookup() {
	delete static_cast<std::vector<PolygonEntry> *>(_entries);
}

bool PolygonDepthLookup::init() {
	if ( _regions.empty() ) {
		SEISCOMP_WARNING("DepthLookup/Polygon: no regions configured");
		return true;
	}

	auto *entries = static_cast<std::vector<PolygonEntry> *>(_entries);
	const Geo::GeoFeatureSet &fs = Geo::GeoFeatureSetSingleton::getInstance();

	for ( const auto *f : fs.features() ) {
		if ( !f || !f->closedPolygon() ) continue;
		if ( std::find(_regions.begin(), _regions.end(), f->name()) == _regions.end() ) continue;

		auto dd = parseAttr(f, "defaultDepth");
		if ( !dd ) {
			SEISCOMP_WARNING("DepthLookup/Polygon: feature '%s' has no defaultDepth attribute — skipped",
			                 f->name().c_str());
			continue;
		}

		auto md = parseAttr(f, "maxDepth");
		entries->push_back({f, *dd, md ? *md : -1.0});
		SEISCOMP_DEBUG("DepthLookup/Polygon: loaded region '%s' defaultDepth=%.0f",
		               f->name().c_str(), *dd);
	}

	SEISCOMP_INFO("DepthLookup/Polygon: %zu region(s) loaded", entries->size());
	return true;
}

bool PolygonDepthLookup::covers(double lat, double lon) const {
	const auto *entries = static_cast<const std::vector<PolygonEntry> *>(_entries);
	const Geo::GeoCoordinate coord(lat, lon);
	for ( const auto &e : *entries ) {
		if ( e.feature->contains(coord) ) return true;
	}
	return false;
}

double PolygonDepthLookup::fetch(double lat, double lon) const {
	const auto *entries = static_cast<const std::vector<PolygonEntry> *>(_entries);
	const Geo::GeoCoordinate coord(lat, lon);
	for ( const auto &e : *entries ) {
		if ( e.feature->contains(coord) ) return e.defaultDepth;
	}
	return _fallbackDepth;
}

double PolygonDepthLookup::fetchMaxDepth(double lat, double lon) const {
	const auto *entries = static_cast<const std::vector<PolygonEntry> *>(_entries);
	const Geo::GeoCoordinate coord(lat, lon);
	for ( const auto &e : *entries ) {
		if ( e.feature->contains(coord) ) {
			return e.maxDepth >= 0.0 ? e.maxDepth : _fallbackMaxDepth;
		}
	}
	return _fallbackMaxDepth;
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
		for ( const auto &level : zone.levels ) {
			for ( const auto *f : features->features() ) {
				if ( f && f->name() == level.featureName && f->contains(coord) ) {
					SEISCOMP_DEBUG("DepthLookup/Slab2: (%.2f, %.2f) → zone '%s' at %d km",
					               lat, lon, zone.name.c_str(), level.depthKm);
					return static_cast<double>(level.depthKm);
				}
			}
		}
	}

	return -1.0;  // not in any slab zone
}

bool Slab2DepthLookup::covers(double lat, double lon) const {
	return _lookupDepth(lat, lon) > 0.0;
}

double Slab2DepthLookup::fetch(double lat, double lon) const {
	double d = _lookupDepth(lat, lon);
	if ( d > 0.0 ) return d;
	SEISCOMP_DEBUG("DepthLookup/Slab2: (%.2f, %.2f) outside all zones → fallback %.0f km",
	               lat, lon, _fallbackDepth);
	return _fallbackDepth;
}

double Slab2DepthLookup::fetchMaxDepth(double lat, double lon) const {
	const auto *features = static_cast<const Geo::GeoFeatureSet *>(_features);
	const Geo::GeoCoordinate coord(lat, lon);

	for ( const auto &zone : _zones ) {
		if ( zone.levels.empty() ) continue;
		const auto &shallowest = zone.levels.back();
		for ( const auto *f : features->features() ) {
			if ( f && f->name() == shallowest.featureName && f->contains(coord) ) {
				SEISCOMP_DEBUG("DepthLookup/Slab2: (%.2f, %.2f) maxDepth → zone '%s' %d km",
				               lat, lon, zone.name.c_str(), zone.maxDepthKm);
				return static_cast<double>(zone.maxDepthKm);
			}
		}
	}

	SEISCOMP_DEBUG("DepthLookup/Slab2: (%.2f, %.2f) maxDepth outside all zones → fallback %.0f km",
	               lat, lon, _fallbackMaxDepth);
	return _fallbackMaxDepth;
}

std::vector<double> Slab2DepthLookup::fetchCandidateDepths(double lat, double lon) const {
	double d = _lookupDepth(lat, lon);
	if ( d > 0.0 ) {
		// Dual-seismicity: slab depth first (primary), shallow crustal fallback second.
		return {d, _fallbackDepth};
	}
	return {_fallbackDepth};
}


// ---------------------------------------------------------------------------
// CompositeDepthLookup
// ---------------------------------------------------------------------------

CompositeDepthLookup::CompositeDepthLookup(std::vector<DepthLookupPtr> backends)
: _backends(std::move(backends)) {}

const DepthLookup *CompositeDepthLookup::_select(double lat, double lon) const {
	for ( const auto &b : _backends ) {
		if ( b->covers(lat, lon) ) return b.get();
	}
	return _backends.back().get();
}

double CompositeDepthLookup::fetch(double lat, double lon) const {
	return _select(lat, lon)->fetch(lat, lon);
}

double CompositeDepthLookup::fetchMaxDepth(double lat, double lon) const {
	return _select(lat, lon)->fetchMaxDepth(lat, lon);
}

std::vector<double> CompositeDepthLookup::fetchCandidateDepths(double lat, double lon) const {
	return _select(lat, lon)->fetchCandidateDepths(lat, lon);
}


// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

DepthLookupPtr makeDepthLookup(const std::string &type,
                               double defaultDepth,
                               double maxDepth,
                               const std::string &slabDir,
                               const std::vector<std::string> &polygonRegions) {
	if ( type == "Composite" ) {
		std::vector<DepthLookupPtr> backends;

		if ( !polygonRegions.empty() ) {
			auto poly = std::make_unique<PolygonDepthLookup>(
			    polygonRegions, defaultDepth, maxDepth);
			poly->init();
			backends.push_back(std::move(poly));
		}

		if ( !slabDir.empty() ) {
			auto slab2 = std::make_unique<Slab2DepthLookup>(
			    slabDir, defaultDepth, maxDepth);
			if ( slab2->init() ) {
				backends.push_back(std::move(slab2));
			}
			else {
				SEISCOMP_WARNING("DepthLookup/Composite: Slab2 failed to init — omitted from chain");
			}
		}

		backends.push_back(std::make_unique<ConstantDepthLookup>(defaultDepth, maxDepth));
		SEISCOMP_INFO("DepthLookup/Composite: chain has %zu backend(s)", backends.size());
		return std::make_unique<CompositeDepthLookup>(std::move(backends));
	}

	if ( type == "Polygon" ) {
		auto poly = std::make_unique<PolygonDepthLookup>(
		    polygonRegions, defaultDepth, maxDepth);
		poly->init();
		return poly;
	}

	if ( type == "Slab2" ) {
		auto backend = std::make_unique<Slab2DepthLookup>(slabDir, defaultDepth, maxDepth);
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


}  // namespace Processing
}  // namespace Seiscomp
