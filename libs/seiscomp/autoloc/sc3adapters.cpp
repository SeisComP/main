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

#include <seiscomp/core/datetime.h>
#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/publicobject.h>

#include <seiscomp/math/geo.h>
#include <seiscomp/math/mean.h>

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <seiscomp/autoloc/sc3adapters.h>
#include <seiscomp/autoloc/util.h>
#include <seiscomp/autoloc/datamodel.h>

namespace Seiscomp {

namespace Autoloc {



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Seiscomp::DataModel::Origin *exportToSC(
	const Autoloc::DataModel::Origin* origin, bool allPhases)
{
	Seiscomp::DataModel::Origin *scorigin
	    = Seiscomp::DataModel::Origin::Create();

	Seiscomp::DataModel::TimeQuantity sctq;
	Seiscomp::DataModel::RealQuantity scrq;

	scorigin->setTime(
		Seiscomp::DataModel::TimeQuantity(
			Autoloc::sctime(origin->time), origin->timeerr,
			Seiscomp::Core::None, Seiscomp::Core::None, Seiscomp::Core::None));
	scorigin->setLatitude(
		Seiscomp::DataModel::RealQuantity(
			origin->lat, origin->laterr,
			Seiscomp::Core::None, Seiscomp::Core::None, Seiscomp::Core::None));
	scorigin->setLongitude(
		Seiscomp::DataModel::RealQuantity(
			origin->lon, origin->lonerr,
			Seiscomp::Core::None, Seiscomp::Core::None, Seiscomp::Core::None));
	scorigin->setDepth(
		Seiscomp::DataModel::RealQuantity(
			origin->dep, origin->deperr,
			Seiscomp::Core::None, Seiscomp::Core::None, Seiscomp::Core::None));

	scorigin->setMethodID(origin->methodID);
	scorigin->setEarthModelID(origin->earthModelID);

	scorigin->setEvaluationMode(
		Seiscomp::DataModel::EvaluationMode(
			Seiscomp::DataModel::AUTOMATIC));
	if ( origin->preliminary )
		scorigin->setEvaluationStatus(
			Seiscomp::DataModel::EvaluationStatus(
				Seiscomp::DataModel::PRELIMINARY));

	switch ( origin->depthType ) {
	case Autoloc::DataModel::Origin::DepthFree:
			scorigin->setDepthType(
				Seiscomp::DataModel::OriginDepthType(
					Seiscomp::DataModel::FROM_LOCATION));
			break;

	case Autoloc::DataModel::Origin::DepthMinimum:
			break;

	case Autoloc::DataModel::Origin::DepthDefault:
			break;

	case Autoloc::DataModel::Origin::DepthManuallyFixed:
			scorigin->setDepthType(
				Seiscomp::DataModel::OriginDepthType(
					Seiscomp::DataModel::OPERATOR_ASSIGNED));
			break;
	default:
			break;
	}

	int arrivalCount = origin->arrivals.size();
	for (int i=0; i<arrivalCount; i++) {
		const Autoloc::DataModel::Arrival &arr = origin->arrivals[i];

		// If not all (automatic) phases are requested, only include P and PKP
		if ( !allPhases && automatic(arr.pick.get()) && arr.phase != "P" && arr.phase != "PKP") {
			SEISCOMP_DEBUG_S("SKIPPING 1  "+arr.pick->id);
			continue;
		}

		// Don't include arrivals with huge residuals as (unless by
		// accident) these are excluded from the location anyway.

		// if (arr.excluded && fabs(arr.residual) > 30.) {
		// 	// FIXME: quick+dirty fix
		// 	SEISCOMP_DEBUG_S("SKIPPING 1  "+arr.pick->id);
		// 	continue;
		// }

		const Seiscomp::DataModel::Phase phase(arr.phase);
		Seiscomp::DataModel::ArrivalPtr scarr 
		    = new Seiscomp::DataModel::Arrival();
		scarr->setPickID(   arr.pick->id);
		scarr->setDistance( arr.distance);
		scarr->setAzimuth(  arr.azimuth);
		scarr->setTimeResidual(arr.residual);
		scarr->setTimeUsed( arr.excluded == Autoloc::DataModel::Arrival::NotExcluded);
		scarr->setWeight(   arr.excluded == Autoloc::DataModel::Arrival::NotExcluded ? 1. : 0.);
		scarr->setPhase(phase);

		// This is practically impossible
		if ( arr.pick->scpick == NULL ) {
			SEISCOMP_ERROR_S("CRITICAL: pick not found: "+arr.pick->id);
			return NULL;
		}

		scorigin->add(scarr.get());
	}

	// set the OriginQuality
	Seiscomp::DataModel::OriginQuality oq;
	oq.setAssociatedPhaseCount(scorigin->arrivalCount());
	oq.setUsedPhaseCount(origin->definingPhaseCount());
	oq.setAssociatedStationCount(origin->associatedStationCount());
	oq.setUsedStationCount(origin->definingStationCount());
	double msd = origin->medianStationDistance();
	if (msd>0)
		oq.setMedianDistance(msd);
	oq.setStandardError(origin->rms());

	double minDist, maxDist, aziGap;
	origin->geoProperties(minDist, maxDist, aziGap);
	oq.setMinimumDistance(minDist);
	oq.setMaximumDistance(maxDist);
	oq.setAzimuthalGap(aziGap);

	scorigin->setQuality(oq);

	return scorigin;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


} // namespace Autoloc

} // namespace Seiscomp
