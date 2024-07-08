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

#include <string>
#include <vector>

#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/sensorlocation.h>
#include "util.h"
#include "sc3adapters.h"
#include "locator.h"


namespace Autoloc {


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
MySensorLocationDelegate::~MySensorLocationDelegate() {
	_sensorLocations.clear();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MySensorLocationDelegate::setStation(const Autoloc::Station *station) {
	std::string key = station->net + "." + station->code;

	Seiscomp::DataModel::SensorLocationPtr
		sloc = Seiscomp::DataModel::SensorLocation::Create();

	sloc->setLatitude(  station->lat  );
	sloc->setLongitude( station->lon  );
	sloc->setElevation( station->alt  );
	_sensorLocations.insert(std::pair<std::string,Seiscomp::DataModel::SensorLocationPtr>(key, sloc));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Seiscomp::DataModel::SensorLocation *
MySensorLocationDelegate::getSensorLocation(Seiscomp::DataModel::Pick *pick) const {
	if ( !pick )
		return nullptr;

	std::string key = pick->waveformID().networkCode() + "." + pick->waveformID().stationCode();

	SensorLocationList::const_iterator it = _sensorLocations.find(key);
	if ( it != _sensorLocations.end() )
		return it->second.get();

	return nullptr;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Locator::Locator()
	: _scconfig(nullptr)
{
	_minDepth = 5;
	_locatorCallCounter = 0;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Locator::setSeiscompConfig(const Seiscomp::Config::Config *scconfig)
{
	_scconfig = scconfig;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Locator::init()
{
	if ( !_scconfig) {
		SEISCOMP_ERROR("_scconfig is NULL in Locator::init()");
		return false;
	}

	const std::string locator = "LOCSAT";
	_sclocator = Seiscomp::Seismology::LocatorInterface::Create(locator.c_str());
	if ( !_sclocator) {
		SEISCOMP_ERROR_S("Could not create " + locator + " instance");
		exit(-1);
	}

	if ( !_sclocator->init(*_scconfig))
		return false;

	_sclocator->useFixedDepth(false);
	_minDepth = 5;
	setFixedDepth(_minDepth, false);

	sensorLocationDelegate = new MySensorLocationDelegate;
	_sclocator->setSensorLocationDelegate(sensorLocationDelegate.get());

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Locator::~Locator()
{
	SEISCOMP_INFO("Locator instance called %ld times", _locatorCallCounter);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Locator::setStation(const Autoloc::Station *station) {
	sensorLocationDelegate->setStation(station);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Locator::setMinimumDepth(double depth)
{
	_minDepth = depth;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
static bool hasFixedDepth(const Origin *origin)
{
	switch(origin->depthType) {
		case Origin::DepthManuallyFixed:
		case Origin::DepthDefault:
			return true;
		default:
			break;
	}
	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Origin *Locator::relocate(const Origin *origin)
{
	_locatorCallCounter++;

// vvvvvvvvvvvvvvvvv
// FIXME: This is still needed, but it would be better to get rid of it!
	// if the origin to relocate has a fixed depth, keep it fixed!
	if ( hasFixedDepth(origin) ) {
		setFixedDepth(origin->hypocenter.dep);
	}
// ^^^^^^^^^^^^^^^^
/*
	else
		useFixedDepth(false);
*/


	Origin* relo = _screlocate(origin);
	if ( ! relo)
		return nullptr;

	if (relo->hypocenter.dep <= _minDepth &&
	    relo->depthType != Origin::DepthManuallyFixed &&
	    ! _sclocator->usingFixedDepth()) {

		// relocate again, this time fixing the depth to _minDepth
		// NOTE: This reconfigures the locator temporarily!
		setFixedDepth(_minDepth, true);
		Origin *relo2 = _screlocate(origin);
		useFixedDepth(false); // restore free depth

		delete relo;

		if ( ! relo2) {
			// give up
			return nullptr;
		}
		else {
			// adopt 2nd relocation result
			relo = relo2;
			relo->depthType = Origin::DepthMinimum;
		}
	}

	OriginQuality &q = relo->quality;
	if ( ! determineAzimuthalGaps(relo, &q.aziGapPrimary, &q.aziGapSecondary))
		q.aziGapPrimary = q.aziGapSecondary = 360.;

	return relo;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Origin* Locator::_screlocate(const Origin *origin)
{
	// convert origin to SC, relocate, and convert the result back

	Seiscomp::DataModel::OriginPtr scorigin = convertToSC(origin);
	if ( ! scorigin ) {
		// give up
		SEISCOMP_ERROR("Unexpected failure to relocate origin");
		return nullptr;
	}

	Seiscomp::DataModel::TimeQuantity sctq;
	Seiscomp::DataModel::RealQuantity scrq;

/*
	if (fixedDepth >= 0) {
		setFixedDepth(fixedDepth);
	}
	else if (_useFixedDepth == true) {
		setFixedDepth(origin->dep);
	}
	else
		releaseDepth();
*/

/*
	// Store SC Picks/Stations here so that they can be found
	// by LocSAT via SC PublicObject lookup
//	std::vector<Seiscomp::DataModel::PublicObjectPtr> scobjects;
*/

	size_t arrivalCount = origin->arrivals.size();
	for (size_t i=0; i<arrivalCount; i++) {

		const Arrival &arr = origin->arrivals[i];
		Seiscomp::DataModel::PickPtr
			scpick = Seiscomp::DataModel::Pick::Find(arr.pick->id);

		if ( !scpick) {
			SEISCOMP_ERROR("THIS MUST NEVER HAPPEN: Pick '%s' not found", arr.pick->id.c_str());
		}
/*
		if ( !scpick) {
			const Seiscomp::DataModel::Phase phase(arr.phase);

			scpick = Seiscomp::DataModel::Pick::Create(arr.pick->id);
			if ( !scpick ) {
				SEISCOMP_ERROR_S("Locator::_screlocate(): Failed to create pick "+arr.pick->id+" - giving up");
				return nullptr;
			}
			const Station *sta = arr.pick->station();
			Seiscomp::DataModel::WaveformStreamID wfid(sta->net, sta->code, "", "XYZ", "");
			scpick->setWaveformID(wfid);
			sctq.setValue(sctime(arr.pick->time));
			scpick->setTime(sctq);
			scpick->setPhaseHint(phase);
			scpick->setEvaluationMode(Seiscomp::DataModel::EvaluationMode(Seiscomp::DataModel::AUTOMATIC));
		}
		scobjects.push_back(scpick);
*/
	}

	// 
	// try the actual relocation
	//
	Seiscomp::DataModel::OriginCPtr screlo;
	try {
		// Sometimes LocSAT requires a second invocation. Reason TBD
		Seiscomp::DataModel::OriginCPtr temp;
		temp = _sclocator->relocate(scorigin.get());
		if ( !temp )
			return nullptr;
		screlo = _sclocator->relocate(temp.get());
		if ( !screlo )
			return nullptr;
	}
	catch ( Seiscomp::Seismology::LocatorException& ) {
		return nullptr;
	}
	catch ( Seiscomp::Seismology::PickNotFoundException& ) {
		SEISCOMP_WARNING("Unsuccessful location due to PickNotFoundException");
		return nullptr;
	}


	// Get the relocated origin back from SC
	// TODO: put it into sc3adapters.cpp
	// HOWEVER: here a copy of the original origin is just updated
	//
	// A copy is made of the input origins, i.e. the Arrival attributes
	// don't get lost or have to be searched for in a complicated manner.
	// However, this relies on the order of the arrivals as returned by
	// LocSAT being the same as in the input. If not, this is absolutely
	// fatal.
	//
	Origin *relo = new Origin(*origin);
	if ( ! relo)
		return nullptr;

	relo->hypocenter.lat = screlo->latitude().value();
	try {
		relo->hypocenter.laterr =
			0.5*screlo->latitude().lowerUncertainty() +
			0.5*screlo->latitude().upperUncertainty();
	}
	catch ( ... ) {
		relo->hypocenter.laterr = screlo->latitude().uncertainty();
	}

	relo->hypocenter.lon = screlo->longitude().value();
	try {
		relo->hypocenter.lonerr =
			0.5*screlo->longitude().lowerUncertainty() +
			0.5*screlo->longitude().upperUncertainty();
	}
	catch ( ... ) {
		relo->hypocenter.lonerr = screlo->longitude().uncertainty();
	}

	relo->hypocenter.dep = screlo->depth().value();
	try {
		relo->hypocenter.deperr =
			0.5*screlo->depth().lowerUncertainty() +
			0.5*screlo->depth().upperUncertainty();
	}
	catch ( ... ) {
		relo->hypocenter.deperr = screlo->depth().uncertainty();
	}

	relo->time = screlo->time().value() - Seiscomp::Core::Time();
	try {
		relo->timeerr =
			0.5*screlo->time().lowerUncertainty() +
			0.5*screlo->time().upperUncertainty();
	}
	catch ( ... ) {
		relo->timeerr = screlo->time().uncertainty();
	}

	relo->methodID = screlo->methodID();
	relo->earthModelID = screlo->earthModelID();

	for (size_t i=0; i<arrivalCount; i++) {

		Arrival &arr = relo->arrivals[i];
		const std::string &pickID = screlo->arrival(i)->pickID();

		if (arr.pick->id != pickID) {
			// If this should ever happen, complain loudly and fail
			SEISCOMP_ERROR("Locator: FATAL ERROR: Inconsistent arrival order");
			return nullptr;
		}

		arr.residual = screlo->arrival(i)->timeResidual();
		arr.distance = screlo->arrival(i)->distance();
		arr.azimuth  = screlo->arrival(i)->azimuth();

		if ( (arr.phase == "P" || arr.phase == "P1") && arr.distance > 115)
			arr.phase = "PKP";

//		if (arr.residual == -999.)
//			arr.residual = 0; // FIXME preliminary cosmetics;

// We do not copy the weight back, because it is still there in the original arrival
//		arr.weight   = screlo->arrival(i)->weight();
/*
		if ( arr.residual > 800 && ( arr.phase == "P" || arr.phase == "Pdiff" ) && \
		     arr.distance > 104 && arr.distance < 112) {

			Seiscomp::TravelTime tt;
			if ( ! travelTimeP(arr.distance, origin->dep, tt))
				continue;
			arr.residual = arr.pick->time - (origin->time + tt.time);
		}
*/
	}

	relo->error.sdobs  = 1; // FIXME
	double norm        = 1.8/relo->error.sdobs;
	relo->error.sdepth = norm*screlo->depth().uncertainty();
	relo->error.stime  = norm*screlo->time().uncertainty();
	relo->error.conf   = 0; // FIXME

	relo->scorigin = screlo;

	return relo;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool determineAzimuthalGaps(const Origin *origin, double *primary, double *secondary)
{
	std::vector<double> azi;

	size_t arrivalCount = origin->arrivals.size();
	for (size_t i=0; i<arrivalCount; i++) {

		const Arrival &arr = origin->arrivals[i];
	
		if (arr.excluded)
			continue;

		azi.push_back(arr.azimuth);
	}

	if (azi.size() < 2)
		return false;

	std::sort(azi.begin(), azi.end());

	*primary = *secondary = 0.;
	size_t aziCount = azi.size();
	azi.push_back(azi[0] + 360.);
	azi.push_back(azi[1] + 360.);

	for (size_t i=0; i<aziCount; i++) {
		double gap = azi[i+1] - azi[i];
		if (gap > *primary)
			*primary = gap;
		gap = azi[i+2] - azi[i];
		if (gap > *secondary)
			*secondary = gap;
	}
	
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



}  // namespace Autoloc
