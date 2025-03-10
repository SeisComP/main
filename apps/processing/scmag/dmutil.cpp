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


#include "component.h"

#include <string>
#include <map>
#include <set>


using namespace std;


#include <seiscomp/core/datetime.h>
#include <seiscomp/logging/log.h>
#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/arrival.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/amplitude.h>
#include <seiscomp/datamodel/stationmagnitude.h>
#include <seiscomp/datamodel/magnitude.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/publicobject.h>
#include <seiscomp/datamodel/utils.h>
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool dumpOrigin(const Seiscomp::DataModel::Origin *origin) {
	using namespace Seiscomp::DataModel;
	using namespace Seiscomp::Core;

	double dt = double(Time::UTC() - origin->time().value())/60.;
	SEISCOMP_INFO("**** origin %s",    origin->publicID().c_str());
	SEISCOMP_INFO("* time      %s   = %.1f min ago",
		      origin->time().value().toString("%F %T.%f").c_str(), dt );
	SEISCOMP_INFO("* latitude  %8.3f", origin->latitude().value());
	SEISCOMP_INFO("* longitude %8.3f", origin->longitude().value());
	SEISCOMP_INFO("* depth     %6.1f", origin->depth().value());
	SEISCOMP_INFO("* arrivals  %4lu",   (unsigned long)origin->arrivalCount());
	SEISCOMP_INFO("** Magnitudes:");

	int nmag = origin->magnitudeCount();
	if (nmag==0) {
		SEISCOMP_INFO("* None");
	}
	for (int i=0; i<nmag; i++) {

		const MagnitudeCPtr mag = origin->magnitude(i);
		double stdev;
		size_t sc = 0;
		try {         sc = mag->stationCount(); } catch (...) {}
		try {         stdev = Seiscomp::DataModel::quantityUncertainty(mag->magnitude()); }
		catch (...) { stdev = 0; }
		SEISCOMP_INFO("* NetMag %-6s %.2f +/- %.2f (%lu)",
			      mag->type().c_str(),
			      mag->magnitude().value(), stdev,
			      (unsigned long)sc);
	}

	// this much detail is actually beyond info scope...
	for (int i=0, nmag = origin->stationMagnitudeCount(); i<nmag; i++) {

		if (i==0) {
			SEISCOMP_DEBUG("** StationMagnitudes:");
		}

		const StationMagnitudeCPtr mag = origin->stationMagnitude(i);
		const WaveformStreamID &wfid = mag->waveformID();

		SEISCOMP_DEBUG("* StaMag %-6s %.2f  %2s.%-6s %s",
				mag->type().c_str(),
				mag->magnitude().value(),
				wfid.networkCode().c_str(),
				wfid.stationCode().c_str(),
				mag->publicID().c_str());
	}

	SEISCOMP_INFO("****");

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool equivalent(const Seiscomp::DataModel::WaveformStreamID &wfid1,
                const Seiscomp::DataModel::WaveformStreamID &wfid2) {
	if (wfid1.networkCode() != wfid2.networkCode()) return false;
	if (wfid1.stationCode() != wfid2.stationCode()) return false;
	// here we consider different component codes to be irrelevant
	if (wfid1.channelCode().substr(0,2) != wfid2.channelCode().substr(0,2)) return false;
	// here we consider different location codes to be irrelevant
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
double arrivalWeight(const Seiscomp::DataModel::Arrival *arr, double defaultWeight) {
	try {
		return arr->weight();
	}
	catch (Seiscomp::Core::ValueException &) {
		return defaultWeight;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
char getShortPhaseName(const string &phase) {
	for ( string::const_reverse_iterator it = phase.rbegin();
	      it != phase.rend(); ++it ) {
		if ( isupper(*it ) )
			return *it;
	}

	return phase[0];
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Seiscomp::DataModel::EvaluationStatus status(const Seiscomp::DataModel::Origin *origin) {
	Seiscomp::DataModel::EvaluationStatus status = Seiscomp::DataModel::PRELIMINARY;

	try {
		status = origin->evaluationStatus();
	}
	catch (...) {
		SEISCOMP_DEBUG("assuming origin.status as %s", status.toString());
	}
	return status;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
