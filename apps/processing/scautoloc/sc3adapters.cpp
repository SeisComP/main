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
#include "datamodel.h"


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Seiscomp::Core::Time sctime(const Autoloc::Time &time)
{
	return Seiscomp::Core::Time() + Seiscomp::Core::TimeSpan(time);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Seiscomp::DataModel::Origin *convertToSC(const Autoloc::Origin* origin, bool allPhases)
{
	using namespace Autoloc;

	Seiscomp::DataModel::Origin *scorigin
	    = Seiscomp::DataModel::Origin::Create();

	Seiscomp::DataModel::TimeQuantity sctq;
	Seiscomp::DataModel::RealQuantity scrq;

	scorigin->setTime(Seiscomp::DataModel::TimeQuantity(sctime(origin->time), origin->timeerr, Seiscomp::Core::None, Seiscomp::Core::None, Seiscomp::Core::None));
	scorigin->setLatitude(Seiscomp::DataModel::RealQuantity(origin->hypocenter.lat, origin->hypocenter.laterr, Seiscomp::Core::None, Seiscomp::Core::None, Seiscomp::Core::None));
	scorigin->setLongitude(Seiscomp::DataModel::RealQuantity(origin->hypocenter.lon, origin->hypocenter.lonerr, Seiscomp::Core::None, Seiscomp::Core::None, Seiscomp::Core::None));
	scorigin->setDepth(Seiscomp::DataModel::RealQuantity(origin->hypocenter.dep, origin->hypocenter.deperr, Seiscomp::Core::None, Seiscomp::Core::None, Seiscomp::Core::None));

	scorigin->setMethodID(origin->methodID);
	scorigin->setEarthModelID(origin->earthModelID);

	scorigin->setEvaluationMode(Seiscomp::DataModel::EvaluationMode(Seiscomp::DataModel::AUTOMATIC));
	if ( origin->preliminary ) {
		scorigin->setEvaluationStatus(Seiscomp::DataModel::EvaluationStatus(Seiscomp::DataModel::PRELIMINARY));
	}

	switch ( origin->depthType ) {
	case Autoloc::Origin::DepthFree:
			scorigin->setDepthType(Seiscomp::DataModel::OriginDepthType(Seiscomp::DataModel::FROM_LOCATION));
			break;

	case Autoloc::Origin::DepthMinimum:
			break;

	case Autoloc::Origin::DepthDefault:
			break;

	case Autoloc::Origin::DepthManuallyFixed:
			scorigin->setDepthType(Seiscomp::DataModel::OriginDepthType(Seiscomp::DataModel::OPERATOR_ASSIGNED));
			break;
	default:
			break;
	}

	// This is a preliminary fix which prevents autoloc from producing
	// origins with fixed depth, as this caused some problems at BMG
	// where the fixed-depth checkbox was not unchecked and an incorrect
	// depth was retained. Need a better way, though.
//	scorigin->setDepthType(Seiscomp::DataModel::OriginDepthType(Seiscomp::DataModel::FROM_LOCATION));

	size_t arrivalCount = origin->arrivals.size();

	for ( size_t i=0; i<arrivalCount; i++ ) {
		const Autoloc::Arrival &arr = origin->arrivals[i];

		// If not all (automatic) phases are requested, only include P and PKP
		if ( !allPhases && automatic(arr.pick.get()) && arr.phase != "P" && arr.phase != "PKP") {
			SEISCOMP_DEBUG_S("SKIPPING 1  "+arr.pick->id);
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
		const Seiscomp::DataModel::Phase phase(arr.phase);
		Seiscomp::DataModel::Arrival* scarr
		        = new Seiscomp::DataModel::Arrival();
		scarr->setPickID(arr.pick->id);
		scarr->setDistance(arr.distance);
		scarr->setAzimuth(arr.azimuth);
		scarr->setTimeResidual( arr.residual);
		scarr->setTimeUsed(arr.excluded == Arrival::NotExcluded);
		scarr->setBackazimuthUsed(arr.excluded == Arrival::NotExcluded);
		scarr->setHorizontalSlownessUsed(arr.excluded == Arrival::NotExcluded);
		scarr->setWeight(arr.excluded == Arrival::NotExcluded ? 1. : 0.);
		scarr->setPhase(phase);

		scorigin->add(scarr);
	}
	Seiscomp::DataModel::OriginQuality oq;
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

	return scorigin;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Autoloc::Origin *Seiscomp::Applications::Autoloc::App::convertFromSC(const Seiscomp::DataModel::Origin *scorigin)
{
	double lat = scorigin->latitude().value();
	double lon = scorigin->longitude().value();
	double dep = scorigin->depth().value();
	double time = double(scorigin->time().value() - Seiscomp::Core::Time());

	::Autoloc::Origin *origin = new ::Autoloc::Origin(lat, lon, dep, time);

	try {
		origin->hypocenter.laterr = 0.5 * (scorigin->latitude().lowerUncertainty()
		                        + scorigin->latitude().upperUncertainty());
	}
	catch ( ... ) {
		try {
			origin->hypocenter.laterr = scorigin->latitude().uncertainty();
		}
		catch ( ... ) {
			origin->hypocenter.laterr = 0;
		}
	}

	try {
		origin->hypocenter.lonerr = 0.5 * (scorigin->longitude().lowerUncertainty()
		                        + scorigin->longitude().upperUncertainty());
	}
	catch ( ... ) {
		try {
			origin->hypocenter.lonerr = scorigin->longitude().uncertainty();
		}
		catch ( ... ) {
			origin->hypocenter.lonerr = 0;
		}
	}

	try {
		origin->hypocenter.deperr = 0.5 * (scorigin->depth().lowerUncertainty()
		                        + scorigin->depth().upperUncertainty());
	}
	catch ( ... ) {
		try {
			origin->hypocenter.deperr = scorigin->depth().uncertainty();
		}
		catch ( ... ) {
			origin->hypocenter.deperr = 0;
		}
	}

	try {
		origin->timeerr = 0.5 * (scorigin->time().lowerUncertainty()
		                         + scorigin->time().upperUncertainty());
	}
	catch ( ... ) {
		try {
			origin->timeerr = scorigin->time().uncertainty();
		}
		catch ( ... ) {
			origin->timeerr = 0;
		}
	}

	int arrivalCount = scorigin->arrivalCount();
	for ( int i=0; i<arrivalCount; i++ ) {
		const std::string &pickID = scorigin->arrival(i)->pickID();
/*
		Seiscomp::DataModel::Pick *scpick = Seiscomp::DataModel::Pick::Find(pickID);
		if ( ! scpick) {
			SEISCOMP_ERROR_S("Pick " + pickID + " not found - cannot convert origin");
			delete origin;
			return nullptr;
			// TODO:
			// Trotzdem mal schauen, ob wir den Pick nicht
			// als Autoloc-Pick schon haben
		}
*/
		const ::Autoloc::Pick *pick = ::Autoloc::Autoloc3::pick(pickID);
		if ( !pick ) {
// TODO: Use Cache here!
			// XXX FIXME: This may also happen after Autoloc cleaned up older picks, so the pick isn't available any more!
			SEISCOMP_ERROR_S("Pick " + pickID + " not found in internal pick pool - SKIPPING this pick");
//			if (Seiscomp::DataModel::PublicObject::Find(pickID))
//				SEISCOMP_ERROR("HOWEVER, this pick is present in pool of public objects");
			// This actually IS an error but we try to work around
			// it instead of giving up in this origin completely.
			continue;
//			delete origin;
//			return nullptr;
		}

		::Autoloc::Arrival arr(pick /* , const std::string &phase="P", double residual=0 */ );
		try {
			arr.residual = scorigin->arrival(i)->timeResidual();
		}
		catch ( ... ) {
			arr.residual = 0;
			SEISCOMP_WARNING("got arrival with timeResidual not set");
		}

		try {
			arr.distance = scorigin->arrival(i)->distance();
		}
		catch ( ... ) {
			arr.distance = 0;
			SEISCOMP_WARNING("got arrival with distance not set");
		}

		try {
			arr.azimuth  = scorigin->arrival(i)->azimuth();
		}
		catch ( ... ) {
			arr.azimuth = 0;
			SEISCOMP_WARNING("got arrival with azimuth not set");
		}

		if ( scorigin->evaluationMode() == DataModel::MANUAL ) {
			// for manual origins we do allow secondary phases like pP
			arr.phase = scorigin->arrival(i)->phase();

			try {
				if ( !scorigin->arrival(i)->timeUsed() ) {
					arr.excluded = ::Autoloc::Arrival::ManuallyExcluded;
				}
			}
			catch ( ... ) {
				// In a manual origin in which the time is not
				// explicitly used we treat the arrival as if
				// it was explicitly excluded.
				arr.excluded = ::Autoloc::Arrival::ManuallyExcluded;
			}
		}

		origin->arrivals.push_back(arr);
	}

	origin->publicID = scorigin->publicID();
	try {
	// FIXME: In scolv the Origin::depthType is not set!
	Seiscomp::DataModel::OriginDepthType dtype = scorigin->depthType();
	if ( dtype == Seiscomp::DataModel::OriginDepthType(Seiscomp::DataModel::FROM_LOCATION) ) {
		origin->depthType = ::Autoloc::Origin::DepthFree;
	}

	else if ( dtype == Seiscomp::DataModel::OriginDepthType(Seiscomp::DataModel::OPERATOR_ASSIGNED) ) {
		origin->depthType = ::Autoloc::Origin::DepthManuallyFixed;
	}
	}
	catch ( ... ) {
		SEISCOMP_WARNING("Origin::depthType is not set!");
		if (scorigin->evaluationMode() == DataModel::MANUAL &&
		    _config.adoptManualDepth == true) {
			// This is a hack! We cannot know wether the operator
			// assigned a depth manually, but we can assume the
			// depth to be opperator approved and this is better
			// than nothing.
			// TODO: Make this behavior configurable?
			origin->depthType = ::Autoloc::Origin::DepthManuallyFixed;
			SEISCOMP_WARNING("Treating depth as if it was manually fixed");
		}
		else {
			origin->depthType = ::Autoloc::Origin::DepthFree;
			SEISCOMP_WARNING("Leaving depth free");
		}
	}

	return origin;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Autoloc::Pick *Seiscomp::Applications::Autoloc::App::convertFromSC(const Seiscomp::DataModel::Pick *scpick)
{
	const std::string &id  = scpick->publicID();
	const std::string &net = scpick->waveformID().networkCode();
	const std::string &sta = scpick->waveformID().stationCode();
	::Autoloc::Time time = ::Autoloc::Time(scpick->time().value());

	::Autoloc::Pick* pick = new ::Autoloc::Pick(id,net,sta,time);

	pick->mode = ::Autoloc::Utils::mode(scpick);
	pick->loc = scpick->waveformID().locationCode();
	pick->cha = scpick->waveformID().channelCode();

	if (pick->loc=="")
		pick->loc = "__";

	pick->attachment = scpick;

	return pick;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
