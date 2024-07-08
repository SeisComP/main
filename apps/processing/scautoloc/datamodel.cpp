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
#include <seiscomp/logging/log.h>

#include <assert.h>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <set>
#include <seiscomp/math/mean.h>
#include "datamodel.h"
#include "util.h"
#include "sc3adapters.h"

namespace Autoloc {


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Station::Station(const std::string &code, const std::string &net, double lat, double lon, double alt)
	: code(code), net(net), lat(lat), lon(lon), alt(alt), used(true), maxNucDist(180), maxLocDist(180)
{
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




static size_t _pickCount=0;

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Pick::Pick(const std::string &id, const std::string &net, const std::string &sta, const Time &time)
	: id(id), net(net), sta(sta),
	  time(time), amp(0), per(0), snr(0), normamp(0),
	  mode(Automatic), xxl(false), _originID(0), _station(nullptr)
{
	_pickCount++;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Pick::~Pick()
{
	_pickCount--;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
size_t Pick::count()
{
	return _pickCount;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Pick::setStation(const Station *sta) const
{
	_station = (Station*)sta;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Pick::setOrigin(OriginID id) const
{
	_originID = id;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Arrival::Arrival(const Pick *pick, const std::string &phase, double residual)
	: origin(nullptr), pick(pick), phase(phase), residual(residual),
	  distance(0), azimuth(0), affinity(0),
	  score(0), ascore(0), dscore(0), tscore(0), excluded(NotExcluded)
{
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Arrival::Arrival(const Origin *origin, const Pick *pick, const std::string &phase, double residual, double affinity)
	: origin(origin), pick(pick), phase(phase), residual(residual),
	  affinity(affinity),
	  score(0), ascore(0), dscore(0), tscore(0), excluded(NotExcluded)
{
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool operator<(const Arrival& a, const Arrival& b) {

	if (a.distance < b.distance)
		return true;
	if (a.distance > b.distance)
		return false;

	if (a.pick->time < b.pick->time)
		return true;
	if (a.pick->time > b.pick->time)
		return false;

	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool ArrivalVector::sort()
{
	std::sort(begin(), end());
	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



static size_t _originCount {0};

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Origin::Origin(double lat, double lon, double dep, const Time &time)
	: id(0), hypocenter(lat, lon, dep), time(time), timeerr(0),
	  imported(false), preliminary(false)
{
	_originCount++;
	processingStatus = New;
	locationStatus = Automatic;
	depthType = DepthFree;
	timestamp = 0.;
	score = 0;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Origin::Origin(const Origin &other)
	: hypocenter(other.hypocenter),
	  time(other.time), timeerr(other.timeerr)
{
	updateFrom(&other);
	id = other.id;
	_originCount++;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Origin::~Origin()
{
	_originCount--;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
size_t Origin::count()
{
	return _originCount;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int Origin::findArrival(const Pick *pick) const
{
	int arrivalCount = arrivals.size();
	for (int i=0; i<arrivalCount; i++) {
		if (arrivals[i].pick == pick)
			return i;
	}

	return -1;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Origin::updateFrom(const Origin *other)
{
	size_t _id = id;
	*this = *other;
	arrivals = other->arrivals;
	id = _id;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Origin::add(const Arrival &arr)
{
	if ( findArrival(arr.pick.get()) != -1 ) {
		SEISCOMP_WARNING_S("Pick already present -> not added.  id = " + arr.pick->id);
		return false;
	}

	arr.pick->setOrigin(id);
	arrivals.push_back(arr);
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
size_t Origin::phaseCount(double dmin, double dmax) const
{
	size_t count {0};

	for (const Arrival &arr : arrivals) {

		if (dmin==0. && dmax==180.) {
			double delta, az, baz;
			delazi(&hypocenter, arr.pick->station(), delta, az, baz);
			if (delta < dmin || delta > dmax)
				continue;
		}

		if (arr.excluded && arr.phase != "PKP")
			continue;

		count++;
	}

	return count;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
size_t Origin::definingPhaseCount(double dmin, double dmax) const
{
	size_t count {0};

	for (const Arrival &arr : arrivals) {

		if (dmin!=0. || dmax!=180.) {
			double delta, az, baz;
			delazi(&hypocenter, arr.pick->station(), delta, az, baz);
			if (delta < dmin || delta > dmax)
				continue;
		}

		if (arr.excluded)
			continue;

		count++;

	}

	return count;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
size_t Origin::associatedStationCount() const
{
	std::set<std::string> stations;

	for (const Arrival& arr : arrivals) {

		if ( !arr.pick )
			continue;

		stations.insert(arr.pick->net + "." + arr.pick->sta);
	}

	return stations.size();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
size_t Origin::definingStationCount() const {
	std::set<std::string> stations;

	for (const Arrival& arr : arrivals) {

		if (arr.excluded)
			continue;

		if ( !arr.pick )
			continue;

		stations.insert(arr.pick->net + "." + arr.pick->sta);
	}

	return stations.size();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
double Origin::rms() const
{
	// This essentially implies that for an imported origin RMS has
	// no meaning, no matter if the origin has arrivals or not.
	if (imported)
		return 0;

	std::vector<double> res;
	for (const Arrival& arr : arrivals) {
		if ( ! arr.excluded)
			res.push_back(arr.residual);
	}

	return Seiscomp::Math::Statistics::rms(res);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
double Origin::medianStationDistance() const
{
	std::vector<double> distance;

	for (const Arrival& arr : arrivals) {
		if ( ! arr.excluded)
			distance.push_back(arr.distance);
	}

	if (distance.size() == 0)
		return -1;

	return Seiscomp::Math::Statistics::median(distance);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Origin::geoProperties(double &min, double &max, double &gap) const
{
	min = 180.;
	max = 0.;
	gap = -1;

	std::vector<double> azi;

	for (const Arrival& arr : arrivals) {
		if ( ! arr.excluded ) {
			if ( arr.distance < min )
				min = arr.distance;
			else if ( arr.distance > max )
				max = arr.distance;
			
			azi.push_back(arr.azimuth);
		}
	}
	if (azi.size() == 0) {
		// This may happen if for whatever reason all arrivals
		// are excluded.
		gap = 360.;
		min = max = 0.;
		return;
	}

	std::sort(azi.begin(), azi.end());
	azi.push_back(azi.front()+360.);

	gap = 0.;

	for ( size_t i = 0; i < azi.size()-1; ++i ) {
		double azGap = azi[i+1]-azi[i];
		if ( azGap > gap )
			gap = azGap;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int OriginVector::mergeEquivalentOrigins(const Origin *start)
{
	return 0;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool OriginVector::find(const Origin *origin) const
{
	for (auto& item : *this) {
		if (origin == item.get())
			return true;
	}

	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Origin* OriginVector::find(const OriginID &id)
{
	for (auto& item: *this) {
		if (id == item->id)
			return item.get();
	}
	return nullptr;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
static size_t countCommonPicks(const Origin *origin1, const Origin *origin2)
{
	size_t count {0};
	for (const Arrival& arr1 : origin1->arrivals) {
		for (const Arrival& arr2 : origin2->arrivals) {
			if (arr1.pick == arr2.pick)
				count++;
		}
	}

	return count;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const Origin *OriginVector::bestEquivalentOrigin(const Origin *origin) const
{
	const Origin *best {nullptr};
	size_t maxCommonPickCount {0};

	for (const auto& item : *this) {

		const Origin* this_origin = item.get();
		const double max_dt = 1500;

		if (std::abs(this_origin->time - origin->time) > max_dt)
			continue;

		size_t commonPickCount = countCommonPicks(origin, this_origin);
		if (commonPickCount < 3)
			continue; // FIXME: hackish

		if (commonPickCount > maxCommonPickCount) {
			maxCommonPickCount = commonPickCount;
			best = this_origin;
		}
	}

	return best;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


}  // namespace Autoloc
