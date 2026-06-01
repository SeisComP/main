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

#include <seiscomp/math/mean.h>

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "sc3adapters.h"
#include "app.h"
#include "util.h"
#include "scutil.h"
#include "datamodel.h"


namespace Seiscomp {



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
DataModel::Origin *convertToSC(const AutolocInternal::Origin* origin, const std::string &author, const std::string &agencyID, bool allPhases) {
	using namespace AutolocInternal;

	DataModel::Origin *scorigin = DataModel::Origin::Create();

	DataModel::TimeQuantity sctim(sctime(origin->time), origin->timeerr, Core::None, Core::None, Core::None);
	DataModel::RealQuantity sclat(origin->hypocenter.lat, origin->hypocenter.laterr, Core::None, Core::None, Core::None);
	DataModel::RealQuantity sclon(origin->hypocenter.lon, origin->hypocenter.lonerr, Core::None, Core::None, Core::None);
	DataModel::RealQuantity scdep(origin->hypocenter.dep, origin->hypocenter.deperr, Core::None, Core::None, Core::None);

	scorigin->setTime(sctim);
	scorigin->setLatitude(sclat);
	scorigin->setLongitude(sclon);
	scorigin->setDepth(scdep);

	scorigin->setMethodID(origin->methodID);
	scorigin->setEarthModelID(origin->earthModelID);

	scorigin->setEvaluationMode(DataModel::EvaluationMode(DataModel::AUTOMATIC));
	if ( origin->preliminary ) {
		scorigin->setEvaluationStatus(DataModel::EvaluationStatus(DataModel::PRELIMINARY));
	}

	switch ( origin->depthType ) {
	case AutolocInternal::Origin::DepthFree:
		scorigin->setDepthType(DataModel::OriginDepthType(DataModel::FROM_LOCATION));
		break;

	case AutolocInternal::Origin::DepthMinimum:
		break;

	case AutolocInternal::Origin::DepthDefault:
		break;

	case AutolocInternal::Origin::DepthManuallyFixed:
		scorigin->setDepthType(DataModel::OriginDepthType(DataModel::OPERATOR_ASSIGNED));
		break;
	default:
		break;
	}

	// This is a preliminary fix which prevents autoloc from producing
	// origins with fixed depth, as this caused some problems at BMG
	// where the fixed-depth checkbox was not unchecked and an incorrect
	// depth was retained. Need a better way, though.
//	scorigin->setDepthType(DataModel::OriginDepthType(DataModel::FROM_LOCATION));

	size_t arrivalCount = origin->arrivals.size();

	for ( size_t i = 0; i < arrivalCount; i++ ) {
		const AutolocInternal::Arrival &arr = origin->arrivals[i];

		// If not all (automatic) phases are requested, only include P and PKP
		if ( !allPhases && automatic(arr.pick.get()) && arr.phase != "P" && arr.phase != "PKP") {
			SEISCOMP_DEBUG_S("SKIPPING 1  " + arr.pick->id());
			continue;
		}

		// Don't include arrivals with huge residuals as (unless by
		// accident) these are excluded from the location anyway.
/*
		if (arr.excluded && std::abs(arr.residual) > 30.) { // FIXME: quick+dirty fix
			SEISCOMP_DEBUG_S("SKIPPING 1  "+arr.pick->id);
			continue;
		}
*/
		const DataModel::Phase phase(arr.phase);
		DataModel::Arrival* scarr = new DataModel::Arrival();
		scarr->setPickID(arr.pick->id());
		scarr->setDistance(arr.distance);
		scarr->setAzimuth(arr.azimuth);
		scarr->setTimeResidual(arr.residual);
		scarr->setTimeUsed(arr.excluded == Arrival::NotExcluded);
		if ( arr.backazimuthUsed ) {
			scarr->setBackazimuthUsed(arr.excluded == Arrival::NotExcluded);
		}
		else {
			scarr->setBackazimuthUsed(false);
		}
		if ( arr.slownessUsed ) {
			scarr->setHorizontalSlownessUsed(arr.excluded == Arrival::NotExcluded);
		}
		else {
			scarr->setHorizontalSlownessUsed(false);
		}
		scarr->setWeight(arr.excluded == Arrival::NotExcluded ? 1. : 0.);
		scarr->setPhase(phase);

		scorigin->add(scarr);
	}
	DataModel::OriginQuality oq;
	oq.setAssociatedPhaseCount(scorigin->arrivalCount());
	oq.setUsedPhaseCount(origin->definingPhaseCount());
	oq.setAssociatedStationCount(origin->associatedStationCount());
	oq.setUsedStationCount(origin->definingStationCount());
	double msd = origin->medianStationDistance();
	if ( msd > 0 ) {
		oq.setMedianDistance(msd);
	}
	oq.setStandardError(origin->rms());

	double minDist, maxDist, aziGap;
	origin->geoProperties(minDist, maxDist, aziGap);
	oq.setMinimumDistance(minDist);
	oq.setMaximumDistance(maxDist);
	oq.setAzimuthalGap(aziGap);

	scorigin->setQuality(oq);

	DataModel::CreationInfo ci;
	ci.setAgencyID(agencyID);
	ci.setAuthor(author);
	scorigin->setCreationInfo(ci);

	return scorigin;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


}  // namespace Seiscomp
