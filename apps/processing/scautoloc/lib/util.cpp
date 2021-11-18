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




# define SEISCOMP_COMPONENT Autoloc
#include <seiscomp/logging/log.h>
#include <seiscomp/core/datetime.h>
#include <seiscomp/math/geo.h>
#include <seiscomp/math/mean.h>

#include <assert.h>
#include <math.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <map>

#include "util.h"
#include "datamodel.h"


namespace Seiscomp {

namespace Autoloc {


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Seiscomp::Core::Time sctime(const Autoloc::DataModel::Time &time)
{
	return Seiscomp::Core::Time() + Seiscomp::Core::TimeSpan(time);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
double distance(const Autoloc::DataModel::Station* s1, const Autoloc::DataModel::Station* s2)
{
	double delta, az, baz;
	delazi(s1->lat, s1->lon, s2->lat, s2->lon, delta, az, baz);
	return delta;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void delazi(double lat1, double lon1, double lat2, double lon2,
            double &delta, double &az1, double &az2)
{
	Seiscomp::Math::Geo::delazi(lat1, lon1, lat2, lon2, &delta, &az1, &az2);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void delazi(const Autoloc::DataModel::Hypocenter *hypo, const Autoloc::DataModel::Station *station,
            double &delta, double &az1, double &az2)
{
	Seiscomp::Math::Geo::delazi(
		hypo->lat,    hypo->lon,
		station->lat, station->lon,
		&delta, &az1, &az2);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
std::string printDetailed(const Autoloc::DataModel::Origin *origin)
{
	return printOrigin(origin, false);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
std::string printOneliner(const Autoloc::DataModel::Origin *origin)
{
	return printOrigin(origin, true);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool travelTimeP(
	double lat1, double lon1, double dep1,
	double lat2, double lon2, double alt2, double delta,
	TravelTime &tt)
{
	static Seiscomp::TravelTimeTable ttt;

	Seiscomp::TravelTimeList
		*ttlist = ttt.compute(lat1, lon1, dep1, lat2, lon2, alt2);

	double maxPdiffDelta = 114;
	for (TravelTime &t : *ttlist) {
		tt = t;
		if (delta < maxPdiffDelta)
			// for  distances < maxPdiffDelta, always take 1st arrival
			break;
		if (tt.phase.substr(0,2) != "PK")
			// for  distances >= maxPdiffDelta, skip Pdiff etc. and
			// take first of PKP*, PKiKP* FIXME: a bit crude
			continue;
		break;
	}
	delete ttlist;

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
std::string time2str(const Autoloc::DataModel::Time &t)
{
	return sctime(t).toString("%F %T.%f000000").substr(0,21);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
double meandev(const Autoloc::DataModel::Origin* origin)
{
	int arrivalCount = origin->arrivals.size();
	double cumresid = 0, cumweight = 0.;

	for(int i=0; i<arrivalCount; i++) {
		const Autoloc::DataModel::Arrival &arr = origin->arrivals[i];
		if (arr.excluded)
			continue;
		cumresid  += fabs(arr.residual);
		cumweight += 1;
	}

	if (cumweight == 0.)
		return 0;

	return cumresid/cumweight;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
double avgfn(double x)
{
	if (x<-1 || x>1)
		return 0;

	x *= M_PI*0.5;
	x = cos(x);
	return x*x;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
std::string printOrigin(const Autoloc::DataModel::Origin *origin, bool oneliner)
{
	assert(origin!=NULL);

	std::ostringstream out;

	int precision = out.precision();

	out.precision(10);

	char depthFlag = ' ';
	switch (origin->depthType) {
	case Autoloc::DataModel::Origin::DepthMinimum:       depthFlag = 'i'; break;
	case Autoloc::DataModel::Origin::DepthPhases:        depthFlag = 'p'; break;
	case Autoloc::DataModel::Origin::DepthDefault:       depthFlag = 'd'; break;
	case Autoloc::DataModel::Origin::DepthManuallyFixed: depthFlag = 'f'; break;
	default:                         depthFlag = ' ';
	}

	if (oneliner) {
		double score = originScore(origin);
		char s[200];
		std::string tstr = time2str(origin->time).substr(11);
		sprintf(s, "%-6lu %s %6.2f %7.2f %3.0f%c %4.1f %3d %3lu s=%5.1f",
			origin->id, tstr.c_str(),
			origin->lat, origin->lon, origin->dep, depthFlag,
			origin->rms(), origin->definingPhaseCount(),
			(unsigned long)origin->arrivals.size(),score);
		out << s;
	}
	else {
		out << "Detailed info for Origin " << origin->id << std::endl
		    << time2str(origin->time) << "  ";
		out.precision(5);
		out << origin->lat << "  " << origin->lon << "  ";
		out.precision(3);
		out << origin->dep << depthFlag << std::endl;

		out.setf(std::ios::right);

		int arrivalCount = origin->arrivals.size();
		for(int i=0; i<arrivalCount; i++) {
			const Autoloc::DataModel::Arrival &arr = origin->arrivals[i];
			const Autoloc::DataModel::Pick* pick = arr.pick.get();

			std::string excludedFlag;
			switch(arr.excluded) {
			case Autoloc::DataModel::Arrival::NotExcluded:          excludedFlag = "  "; break;
			case Autoloc::DataModel::Arrival::LargeResidual:        excludedFlag = "Xr"; break;
			case Autoloc::DataModel::Arrival::StationDistance:      excludedFlag = "Xd"; break;
			case Autoloc::DataModel::Arrival::ManuallyExcluded:     excludedFlag = "Xm"; break;
			case Autoloc::DataModel::Arrival::UnusedPhase:          excludedFlag = "Xp"; break;
			case Autoloc::DataModel::Arrival::DeterioratesSolution: excludedFlag = "X!"; break;
			case Autoloc::DataModel::Arrival::TemporarilyExcluded:  excludedFlag = "Xt"; break;
			default:                            excludedFlag = "X ";
			}
			if ( ! pick->station()) {
				out << pick->id << "   missing station information" << std::endl;
				continue;
			}

			std::string net = pick->station()->net;
			std::string sta = pick->station()->code;

			out.precision(1);
			out     << std::left << std::setw(4) << (i+1)
				<< std::setw(6) << sta
				<< std::setw(3) << net;
			out.precision(2);
			out	<< std::right << std::fixed << std::setw(6) << arr.distance;
			out.precision(0);
			out	<< std::setw(4) << arr.azimuth
				<< std::left << " "
				<< std::setw(11) << time2str(pick->time).substr(11);
			out.precision(1);
			out	<< std::right
				<< std::setiosflags(std::ios::fixed)
				<< std::setw(6) << arr.residual << " "
				<< ((arr.pick->status == Autoloc::DataModel::Pick::Automatic) ?"A":"M") // << " "
				<< excludedFlag << " "
				<< std::left << std::setw(6) << arr.phase << " "
				<< (arr.pick->xxl ? "XXL":"   ");
			out.precision(1);
			out	<< "  " << std::right << std::fixed << std::setw(5) << arr.pick->snr;;
			out.precision(2);
			out	<< " " << arr.score << " -";
			out	<< " " << arr.tscore;
			out	<< " " << arr.ascore;
			out	<< " " << arr.dscore;
			out	<< std::endl;
		}

		out.precision(1);
		out << std::endl << "RMS   = " << origin->rms() << std::endl;
		out << "MD    = " << meandev(origin) << std::endl;
		out << "PGAP  = " << origin->quality.aziGapPrimary << std::endl;
		out << "SGAP  = " << origin->quality.aziGapSecondary << std::endl;
		out << "SCORE = " << originScore(origin) << std::endl;
		out << "preliminary = "  << (origin->preliminary ? "true":"false") << std::endl;

		out.precision(precision);
	}
	return out.str();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Autoloc::DataModel::Pick::Status status(const Seiscomp::DataModel::Pick *pick) {
	try {
		switch ( pick->evaluationMode() ) {
			case Seiscomp::DataModel::AUTOMATIC:
				return Autoloc::DataModel::Pick::Automatic;
			case Seiscomp::DataModel::MANUAL:
				return Autoloc::DataModel::Pick::Manual;
			//case Seiscomp::DataModel::CONFIRMED_PICK:
			//	return Autoloc::DataModel::Pick::Confirmed;
			default:
				break;
		}
	}
	catch ( ... ) {}

	return Autoloc::DataModel::Pick::Automatic;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
static double depthFactor(double depth)
{
	// straight line, easy (but also risky!) to be made configurable
	return 1+0.0005*(200-depth);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
static double avgfn2(double x)
{
	const double w = 0.2; // plateau width

	if (x < -1 || x > 1)
		return 0;
	if (x > -w && x < w)
		return 1;

	x = (x + (x>0 ? -w : w))/(1-w);
//	x = cos(x*M_PI*0.5);
	x = 0.5*(cos(x*M_PI)+1);
	return x*x;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
double originScore(const Autoloc::DataModel::Origin *origin, double maxRMS, double networkSizeKm)
{
//	networkSizeKm = 200.;

	// FIXME:
	((Autoloc::DataModel::Origin*)origin)->arrivals.sort();

	double score = 0, amplScoreMax=0;
	int arrivalCount = origin->arrivals.size();
	//int n = origin->definingPhaseCount();
	for(int i=0; i<arrivalCount; i++) {
		double phaseScore = 1; // 1 for P / 0.3 for PKP
		Autoloc::DataModel::Arrival &arr = ((Autoloc::DataModel::Origin*)origin)->arrivals[i];
		Autoloc::DataModel::PickCPtr pick = arr.pick;
		if ( ! pick->station())
			continue;

		arr.score = 0;
		arr.ascore = arr.dscore = arr.tscore = 0;
		// higher score for picks with higher SNR
		double snr = pick->snr > 3 ? pick->snr : 3;
		if ( snr > 1.E07 )
			continue;
		if ( snr > 100 )
			snr = 100;

		// FIXME: This is HIGHLY experimental:
		// For a manual pick without SNR, as produced by
		// scolv, we assume a default value.
		if (manual(pick.get()) && pick->snr <= 0)
			snr = 10; // make this configureable

		double normamp = pick->normamp;
		if (manual(pick.get()) && normamp <= 0)
			normamp = 1; // make this configureable

		double snrScore = log10(snr);

		double d = arr.distance;
		// FIXME: This is HIGHLY experimental:
		// For a small, dense network the distance score must decay quickly with distance
		// whereas for teleseismic usages it must be a much broader function
		double r = networkSizeKm <= 0 ? pick->station()->maxNucDist : (0.5*networkSizeKm/111.195);
		double distScore = 1.5*exp(-d*d/(r*r));

		// Any amplitude > 1 percent of the XXL threshold
		// will have an increased amplitude score
		double q = 0.8;
		if (normamp <= 0) {
			SEISCOMP_WARNING("THIS SHOULD NEVER HAPPEN: pick %s with  normamp %g  amp %g (not critical)",
				pick->id.c_str(), normamp, pick->amp);
			continue;
		}

		double amplScore = 1+q*(1+0.5*log10(normamp));
		// The score must *not* be reduced based on low amplitude
		if (amplScore < 1) amplScore = 1;

		// Amplitudes usually decrease with distance.
		// This hack takes this fact into account for computing the score.
		// A sudden big amplitude at large distance cannot increase the score too badly
		if(amplScoreMax==0)
			amplScoreMax = amplScore;
		else {
			if (i>2 && amplScore > amplScoreMax+0.4)
				amplScore = amplScoreMax+0.4;
			if (amplScore > amplScoreMax)
				amplScoreMax = amplScore;
		}

		amplScore *= snrScore;
//		weight += amplScore;

		// "scaled" residual
		// This accounts for the fact that at the beginning, origins
		// are more susceptible to outliers, so we allow a somewhat
		// higher residual. XXX Note that this may increase the score
		// for origins with less arrivals, which might be harmful.

		double timeScore = avgfn2(arr.residual/(2*maxRMS));

		arr.dscore = distScore;
		arr.ascore = amplScore;
		arr.tscore = timeScore;

		if (arr.excluded) {
			if (arr.excluded != Autoloc::DataModel::Arrival::UnusedPhase)
				continue;
			if (arr.phase.substr(0,3) != "PKP")
				continue;
			phaseScore = 0.3;
		}

//		arr.score = phaseScore*weight*timeScore;
		arr.score = phaseScore*timeScore*distScore*amplScore;
		score += arr.score;

		// higher score for picks not excluded from location
//		score += arr.excluded ? 0 : 0.3;
	}

	// the closer the better
//	score -= 0.1*(origin->medianStationDistance() - 30);

	// slight preference to shallower origins
	score *= depthFactor(origin->dep);

/*
	double rms = origin->rms();

	// This is a function that penalizes rms > q*maxRMS
	// At rms == maxRMS this function is 1.
	double  q = 0.33, rmspenalty = 0;
	if (rms > maxRMS) {
		double x = ((rms-maxRMS*q)/(maxRMS-maxRMS*q));
		rmspenalty = x*x;
	}
*/

	return score;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool determineAzimuthalGaps(const Autoloc::DataModel::Origin *origin, double *primary, double *secondary)
{
	std::vector<double> azi;

	int arrivalCount = origin->arrivals.size();
	for (int i=0; i<arrivalCount; i++) {
		const Autoloc::DataModel::Arrival &arr = origin->arrivals[i];

		if (arr.excluded)
			continue;

		azi.push_back(arr.azimuth);
	}

	if (azi.size() < 2)
		return false;

	sort(azi.begin(), azi.end());

	*primary = *secondary = 0.;
	int aziCount = azi.size();
	azi.push_back(azi[0] + 360.);
	azi.push_back(azi[1] + 360.);

	for (int i=0; i<aziCount; i++) {
		double gap = azi[i+1]-azi[i];
		if (gap > *primary)
			*primary = gap;
		gap = azi[i+2]-azi[i];
		if (gap > *secondary)
			*secondary = gap;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



} // namespace Seiscomp::Autoloc


namespace Math {
namespace Statistics {

double rms(const std::vector<double> &v, double offset /* = 0 */)
{
	unsigned int n = v.size();
	const double *f = &v[0];
	double fi, r=0;

	if(offset) {
		for(unsigned int i=0; i<n; i++, f++) {
			fi = ((*f)-offset);
			r += fi*fi;
		}
	}
	else {
		for(unsigned int i=0; i<n; i++, f++) {
			r += (*f)*(*f);
		}
	}

	return sqrt(r/n);
}

} // namespace Statistics
} // namespace Math
} // namespace Seiscomp
