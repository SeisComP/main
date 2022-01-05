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

#include <seiscomp/autoloc/associator.h>
#include <seiscomp/autoloc/util.h>

#include <seiscomp/logging/log.h>


namespace Seiscomp {

#define minimumAffinity 0.1


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Associator::Associator()
{
	_origins = 0;
	_stations = 0;

	// The order of the phases is crucial!
	_phaseRanges.push_back( PhaseRange("P",      0, 180) );
	_phaseRanges.push_back( PhaseRange("PcP",   25,  55) );
	_phaseRanges.push_back( PhaseRange("ScP",   25,  55) );
	_phaseRanges.push_back( PhaseRange("PP",    60, 160) );

	// For the following phases there are no tables in LocSAT!
	_phaseRanges.push_back( PhaseRange("SKP",  120, 150) );
	_phaseRanges.push_back( PhaseRange("PKKP",  80, 130) );
	_phaseRanges.push_back( PhaseRange("PKiKP", 30, 120) );
	_phaseRanges.push_back( PhaseRange("SKKP", 110, 152) );

	// TODO
	// - make the phase ranges configurable
	// - wider view also involving LocSAT tables
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Associator::~Associator()
{
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void
Associator::setStations(const Autoloc::DataModel::StationMap *stations)
{
	_stations = stations;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void
Associator::setOrigins(const Autoloc::DataModel::OriginVector *origins)
{
	_origins = origins;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void
Associator::setPickPool(const Autoloc::DataModel::PickPool *p)
{
	pickPool = p;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void
Associator::reset()
{
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void
Associator::shutdown()
{
	reset();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Associator::findMatchingPicks(
	const Autoloc::DataModel::Origin *origin,
	AssociationVector &associations) const
{
	for (const auto &item: *pickPool) {
		Autoloc::DataModel::PickCPtr pick = item.second;

		if (pick->time < origin->time)
			continue;
		if (pick->time > origin->time + 1500.)
			continue;

		bool requiresAmplitude = automatic(pick.get());
		if (requiresAmplitude && ! hasAmplitude(pick.get()))
			continue;

		const Autoloc::DataModel::Station *station = pick->station();
		if ( ! station) {
			SEISCOMP_ERROR("Station missing for pick %s", pick->id.c_str());
			continue;
		}

		double delta, az, baz;
		Autoloc::delazi(origin, station, delta, az, baz);

		// Weight residuals at regional distances "a bit" lower
		// This is quite hackish!
		double x = 1 + 0.6*exp(-0.003*delta*delta) + 0.5*exp(-0.03*(15-delta)*(15-delta));

		Seiscomp::TravelTimeList
			*ttlist = ttt.compute(
				origin->lat, origin->lon, origin->dep,
				station->lat, station->lon, 0);

		// For each pick in the pool we compute a travel time table.
		// Then we try to match predicted and measured arrival times.
		double ttime = -1;
		for (const Seiscomp::TravelTime &tt : *ttlist) {
			PhaseRange phaseRange;
			// We skip this travel time unless we have a phase
			// range definition for the phase
			if ( ! findPhaseRange(tt.phase, phaseRange))
				continue;

			// We skip this phase if we are out of the interersting range.
			// This may well be withing the defined phase range
			// but e.g. PcP gets too close to P which we avoid.
			if ( ! phaseRange.contains(delta, origin->dep))
				continue;

			// phase range definition found
			ttime = tt.time;
			double affinity = 0;
			double residual = double(pick->time - (origin->time + ttime));

			residual /= x;
			residual /= 10; // normalize

			// TODO: REVIEW:
			affinity = Autoloc::avgfn(residual); // test if exp(-residual**2) if better
			if (affinity < minimumAffinity)
				continue;

			Association asso(
				origin, pick.get(),
				phaseRange.code,
				residual, affinity);
			asso.distance = delta;
			asso.azimuth = az;
			associations.push_back(asso);
			break;
		}

		delete ttlist;

		if (ttime == -1)
			continue;
		
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool
Associator::findMatchingOrigins(
	const Autoloc::DataModel::Pick *pick,
	AssociationVector &associations) const
{
	using namespace Autoloc::DataModel;

	if ( ! _origins)
		return false;

	for (const OriginPtr &origin : *_origins) {
		const Station *station = pick->station();

		double delta, az, baz;
		Autoloc::delazi(origin.get(), station, delta, az, baz);

		Seiscomp::TravelTimeList
			*ttlist = ttt.compute(origin->lat, origin->lon, origin->dep,
			                      station->lat, station->lon, 0);

		// An imported origin is treated as if it had a very high
		// score. => Anything can be associated with it.
		double score = origin->imported ? 1000 : origin->score;

		for (const PhaseRange &phaseRange: _phaseRanges) {

			// TODO: make this configurable
			// if (origin->definingPhaseCount() < (phase.code=="P" ? 8 : 30))
			if (score < (phaseRange.code=="P" ? 20 : 50))
				continue;


			if ( ! phaseRange.contains(delta, origin->dep))
				continue;

			double ttime = -1, x = 1;

			if (phaseRange.code == "P") {
				for (const Seiscomp::TravelTime &tt : *ttlist) {
					if (delta < 114) {
						// for  distances < 114, allways take 1st arrival
						ttime = tt.time;
						break;
					}
					if (tt.phase.substr(0,2) != "PK")
						// for distances >= 114, skip Pdiff etc., take first
						// PKP*, PKiKP*
						continue;
					ttime = tt.time;
					break;
				}
				// Weight residuals at regional distances "a bit" lower
				// This is quite hackish!
				x = 1 + 0.6*exp(-0.003*delta*delta) + 0.5*exp(-0.03*(15-delta)*(15-delta));
			}
			else {
				for (const Seiscomp::TravelTime &tt : *ttlist) {
					if (tt.phase.substr(0, phaseRange.code.size()) == phaseRange.code) {
						ttime = tt.time;
						break;
					}
				}
			}

			if (ttime == -1) // phase not found
				continue;

			// compute "affinity" based on distance and residual
			double affinity = 0;
			double residual = double(pick->time - (origin->time + ttime));
			if ( origin->imported ) {
				// This is a clear-cut decision:
				// If the pick is within the interval
				//   -> associate it with affinity one
				// otherwise
				//   -> skip this pick
				//
				// TODO: This range may be too large. Make it configurable.
				if (residual < -20 || residual > 30)
					continue;
				affinity = 1;
			}
			else {
				residual = residual/x;
				residual /= 10; // normalize

				// TODO: REVIEW:
				affinity = Autoloc::avgfn(residual); // test if exp(-residual**2) if better
				if (affinity < minimumAffinity)
					continue;
			}
			std::string phcode = phaseRange.code;
			if (phcode=="P" && ttime > 960)
				phcode = "PKP";
			Association asso(origin.get(), pick, phcode, residual, affinity);
			asso.distance = delta;
			asso.azimuth = az;
			associations.push_back(asso);

			// Not more than one association per origin
			// TODO: REVIEW!
			break;
		}

		delete ttlist;
	}

	return (associations.size() > 0);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool
Associator::feed(const Autoloc::DataModel::Pick* pick)
{
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Association*
Associator::associate(
	      Autoloc::DataModel::Origin *origin,
	const Autoloc::DataModel::Pick *pick,
	const std::string &phase)
{
	return NULL;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Associator::findPhaseRange(const std::string &code, PhaseRange &_pr) const
{
	// exact match
	for (const PhaseRange &pr: _phaseRanges) {
		if (pr.code == code) {
			_pr = pr;
			return true;
		}
	}

	using namespace std;

	// no exact match -> try to find equivalent
	for (const PhaseRange &pr: _phaseRanges) {
		vector<string> basenames = {"P","S"};

		for (const string &base: basenames) {
			if ((pr.code == base     && code == base+"n") ||
			    (pr.code == base+"n" && code == base)) {
				_pr = pr;
				return true;
			}
			if ((pr.code == base        && code == base+"diff") ||
			    (pr.code == base+"diff" && code == base)) {
				_pr = pr;
				return true;
			}
		}
	}

	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
PhaseRange::PhaseRange(
	const std::string &code,
	double dmin, double dmax, double zmin, double zmax)
	: code(code), dmin(dmin), dmax(dmax), zmin(zmin), zmax(zmax) {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool
PhaseRange::contains(double delta, double depth) const
{
	if (delta < dmin || delta > dmax)
		return false;
	if (depth < zmin || depth > zmax)
		return false;

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


}  // namespace Seiscomp
