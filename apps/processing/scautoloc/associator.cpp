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

#include <math.h>
#include <seiscomp/seismology/ttt.h>

#include "util.h"
#include "sc3adapters.h"
#include "associator.h"

using namespace std;

namespace Autoloc {

#define AFFMIN 0.1

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Associator::Associator()
{
	_origins = nullptr;
	_stations = nullptr;

	// The order of the phases is essential!
	_phases.push_back( Phase("P",      0, 180) );
	_phases.push_back( Phase("PcP",   25,  55) );
	_phases.push_back( Phase("ScP",   25,  55) );
	_phases.push_back( Phase("PP",    60, 160) );

	// FIXME: For these, there are no tables in LocSAT!
	_phases.push_back( Phase("SKP",  120, 150) );
	_phases.push_back( Phase("PKKP",  80, 130) );
	_phases.push_back( Phase("PKiKP", 30, 120) );
	_phases.push_back( Phase("SKKP", 110, 152) );

	// TODO: make the phase set configurable
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void
Associator::setStations(const StationMap *stations)
{
	_stations = stations;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void
Associator::setOrigins(const OriginVector *origins)
{
	_origins = origins;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void
Associator::reset()
{
	_associations.clear();
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
bool
Associator::feed(const Pick* pick)
{
	_associations.clear();

	if ( ! _origins)
		return false;

	static Seiscomp::TravelTimeTable ttt;

	int count = 0;

	for (const OriginPtr& _origin : *_origins) {

		const Origin  *origin = _origin.get();
		const Station *station = pick->station();

		const Hypocenter& hypo{origin->hypocenter};
		double delta, az, baz;
		delazi(&hypo, station, delta, az, baz);

		Seiscomp::TravelTimeList
			*ttlist = ttt.compute(hypo.lat, hypo.lon, hypo.dep,
			                      station->lat, station->lon, 0);

		// An imported origin is treated as if it had a very high
		// score. => Anything can be associated with it.
		double origin_score = origin->imported ? 1000 : origin->score;

		for (const Phase &phase : _phases) {

			// TODO: make this configurable
//			if (origin->definingPhaseCount() < (phase.code == "P" ? 8 : 30))
			if (origin_score < (phase.code == "P" ? 20 : 50))
				continue;

			if (delta < phase.dmin || delta > phase.dmax)
				continue;

			double ttime = -1, x = 1;

			if (phase.code == "P") {
				for (const auto& tt: *ttlist) {
					if (delta < 114) {
						// for delta < 114,
						// always take 1st arrival
						ttime = tt.time;
						break;
					}
					if (tt.phase.substr(0,2) != "PK")
						// for delta >= 114,
						// skip Pdiff etc.,
						// take first of PKP*, PKiKP*
						continue;

					ttime = tt.time;
					break;
				}
				// Weight residuals at regional distances
				// "a bit" lower. This is quite hackish!
				x = 1 + 0.6*exp(-0.003*delta*delta) + 0.5*exp(-0.03*(15-delta)*(15-delta));
			}
			else {
				for (const auto& tt: *ttlist) {
					if (tt.phase.substr(0, phase.code.size()) == phase.code) {
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
				// If the pick is within the interval,
				// associate it with affinity 1, otherwise skip
				//
				// TODO: Make this configurable
				if (residual < -20 || residual > 30)
					continue;
				affinity = 1;
			}
			else {
				residual = residual/x;
				residual /= 10;       // normalize residual

				affinity = avgfn(residual); // test if exp(-residual**2) if better
				if (affinity < AFFMIN)
					continue;
			}
			string phcode = phase.code;
			if (phcode=="P" && ttime > 960)
				phcode = "PKP";
			Association asso(origin, pick, phcode, residual, affinity);
			asso.distance = delta;
			asso.azimuth = az;
			_associations.push_back(asso);
			count++;

			// ensure no more than one association per origin
			break;
		}

		delete ttlist;
	}

	return (_associations.size() > 0);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const AssociationVector &
Associator::associations() const
{
	return _associations;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Association*
Associator::associate(Origin *origin, const Pick *pick, const string &phase)
{
	return nullptr;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Associator::Phase::Phase(const string &code, double dmin, double dmax)
	: code(code), dmin(dmin), dmax(dmax) {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


}  // namespace Autoloc
