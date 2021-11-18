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

#include <seiscomp/seismology/ttt.h>

#include "util.h"
#include "lib/associator.h"

namespace Seiscomp {

#define minimumAffinity 0.1


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Associator::Associator()
{
	_origins = 0;
	_stations = 0;

	// The order of the phases is crucial!
	_phases.push_back( Phase("P",      0, 180) );
	_phases.push_back( Phase("PcP",   25,  55) );
	_phases.push_back( Phase("ScP",   25,  55) );
	_phases.push_back( Phase("PP",    60, 160) );

	// For the following phases there are no tables in LocSAT!
	_phases.push_back( Phase("SKP",  120, 150) );
	_phases.push_back( Phase("PKKP",  80, 130) );
	_phases.push_back( Phase("PKiKP", 30, 120) );
	_phases.push_back( Phase("SKKP", 110, 152) );

	// TODO
	// - make the phase set configurable
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
Associator::feed(const Autoloc::DataModel::Pick* pick)
{
	using namespace Autoloc::DataModel;

	_associations.clear();

	if ( ! _origins)
		return false;

	static Seiscomp::TravelTimeTable ttt;

	int count = 0;

	for (const OriginPtr &origin : *_origins) {
		const Station *station = pick->station();

		double delta, az, baz;
		Autoloc::delazi(origin.get(), station, delta, az, baz);

		Seiscomp::TravelTimeList
			*ttlist = ttt.compute(origin->lat, origin->lon, origin->dep,
			                      station->lat, station->lon, 0);

		// An imported origin is treated as if it had a very high
		// score. => Anything can be associated with it.
		double origin_score = origin->imported ? 1000 : origin->score;

		for (const Phase &phase: _phases) {

			// TODO: make this configurable
			// if (origin->definingPhaseCount() < (phase.code=="P" ? 8 : 30))
			if (origin_score < (phase.code=="P" ? 20 : 50))
				continue;

			if (delta < phase.dmin || delta > phase.dmax)
				continue;

			double ttime = -1, x = 1;

			if (phase.code == "P") {
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

				affinity = Autoloc::avgfn(residual); // test if exp(-residual**2) if better
				if (affinity < minimumAffinity)
					continue;
			}
			std::string phcode = phase.code;
			if (phcode=="P" && ttime > 960)
				phcode = "PKP";
			Association asso(origin.get(), pick, phcode, residual, affinity);
			asso.distance = delta;
			asso.azimuth = az;
			_associations.push_back(asso);
			count++;

			break; // ensure no more than one association per origin
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
Associator::associate(Autoloc::DataModel::Origin *origin, const Autoloc::DataModel::Pick *pick, const std::string &phase)
{
	return NULL;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Associator::Phase::Phase(const std::string &code, double dmin, double dmax)
	: code(code), dmin(dmin), dmax(dmax) {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


}  // namespace Seiscomp
