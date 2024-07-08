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
#include <seiscomp/core/strings.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <list>
#include <cmath>

#include "util.h"
#include "sc3adapters.h"
#include "locator.h"
#include "nucleator.h"


namespace Autoloc {

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
static std::string station_key (const Station *station)
{
	return station->net + "." + station->code;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




typedef std::set<PickCPtr> PickSet;

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Nucleator::setStation(const Station *station)
{
	std::string key = station->net + "." + station->code;
	if (_stations.find(key) != _stations.end())
		return; // nothing to insert
	_stations.insert(StationMap::value_type(key, station));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const OriginVector &Nucleator::newOrigins() const {
	return _newOrigins;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void GridSearch::setStation(const Station *station)
{
	Nucleator::setStation(station);
	_relocator.setStation(station);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
GridSearch::GridSearch()
{
//	_stations = 0;
	_abort = false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void GridSearch::setSeiscompConfig(const Seiscomp::Config::Config *scconfig) {
	_scconfig = scconfig;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool GridSearch::init()
{
	_relocator.setSeiscompConfig(_scconfig);
	if ( ! _relocator.init()) {
		SEISCOMP_ERROR("GridSearch::init(): Failed to initialize relocator");
		return false;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool GridSearch::setGridFile(const std::string &gridfile)
{
	return _readGrid(gridfile);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void GridSearch::setLocatorProfile(const std::string &profile) {
	_relocator.setProfile(profile);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int GridSearch::cleanup(const Time& minTime)
{
	int count = 0;

	for (auto& gridpoint: _grid) {
		count += gridpoint->cleanup(minTime);
	}

	return count;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




static size_t _projectedPickCount {0};

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
ProjectedPick::ProjectedPick(const Time &t)
	: _projectedTime(t)
{
	_projectedPickCount++;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
ProjectedPick::ProjectedPick(PickCPtr p, StationWrapperCPtr w)
	: p(p), wrapper(w), _projectedTime(p->time - w->ttime)
{
	_projectedPickCount++;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
ProjectedPick::ProjectedPick(const ProjectedPick &other)
	: p(other.p), wrapper(other.wrapper), _projectedTime(other._projectedTime)
{
	_projectedPickCount++;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
ProjectedPick::~ProjectedPick()
{
	_projectedPickCount--;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
size_t ProjectedPick::Count()
{
	return _projectedPickCount;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
GridPoint::GridPoint(double latitude, double longitude, double depth)
	: hypocenter(latitude, longitude, depth), _radius(4), _dt(50), maxStaDist(180), _nmin(6)
{
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const Origin*
GridPoint::feed(const Pick* pick)
{
	// find the station corresponding to the pick
	const std::string key = station_key(pick->station());

	std::map<std::string, StationWrapperCPtr>::const_iterator
		xit = _wrappers.find(key);
	if (xit==_wrappers.end())
		// this grid cell may be out of range for that station
		return nullptr;
	StationWrapperCPtr wrapper = (*xit).second;
	if ( ! wrapper->station ) {
		SEISCOMP_ERROR("Nucleator: station '%s' not found", key.c_str());
		return nullptr;

	}

	// At this point we hold a "wrapper" which wraps a station and adds a
	// fews grid-point specific attributes such as the distance from this
	// gridpoint to the station etc.

	// If the station distance exceeds the maximum station distance
	// configured for the grid point...
	if ( wrapper->distance > maxStaDist )
		return nullptr;

	// If the station distance exceeds the maximum nucleation distance
	// configured for the station...
	if ( wrapper->distance > wrapper->station->maxNucDist )
		return nullptr;

	// back-project pick to hypothetical origin time
	ProjectedPick pp(pick, wrapper);

	// store newly inserted pick
	/* std::multiset<ProjectedPick>::iterator latest = */ _picks.insert(pp);

	// roughly test if there is a cluster around the new pick
	std::vector<ProjectedPick> pps;
	std::multiset<ProjectedPick>::iterator it,
		lower  = _picks.lower_bound(pp.projectedTime() - _dt),
		upper  = _picks.upper_bound(pp.projectedTime() + _dt);
	for (it=lower; it!=upper; ++it)
		pps.push_back(*it);
	size_t npick = pps.size();

	// if the number of picks around the new pick is too low...
	if (npick < _nmin)
		return nullptr;

	// now take a closer look at how tightly clustered the picks are
	double dt0 = 4; // XXX
	std::vector<size_t> _cnt(npick);
	std::vector<size_t> _flg(npick);
	for (size_t i=0; i<npick; i++) {
		_cnt[i] = _flg[i] = 0;
	}
	for (size_t i=0; i<npick; i++) {

		ProjectedPick &ppi = pps[i];
		double t_i   = ppi.projectedTime();
		double azi_i = ppi.wrapper->azimuth;
		double slo_i = ppi.wrapper->hslow;

		for (size_t k=i; k<npick; k++) {

			ProjectedPick &ppk = pps[k];
			double t_k   = ppk.projectedTime();
			double azi_k = ppk.wrapper->azimuth;
			double slo_k = ppk.wrapper->hslow;

			double azi_diff = std::abs(fmod(((azi_k-azi_i)+180.), 360.)-180.);
			double dtmax = _radius*(slo_i+slo_k) * azi_diff/90. + dt0;

			if (std::abs(t_i-t_k) < dtmax) {
				_cnt[i]++;
				_cnt[k]++;

				if (ppi.p == pp.p || ppk.p == pp.p)
					_flg[k] = _flg[i] = 1;
			}
		}
	}

	size_t sum=0;
	for (size_t i=0; i<npick; i++)
		sum += _flg[i];
	if (sum < _nmin)
		return nullptr;

	std::vector<ProjectedPick> group;
	size_t cntmax = 0;
	Time otime;
	for (size_t i=0; i<npick; i++) {
		if ( ! _flg[i])
			continue;
		group.push_back(pps[i]);
		if (_cnt[i] > cntmax) {
			cntmax = _cnt[i];
			otime = pps[i].projectedTime();
		}
	}


// vvvvvvvvvvvvv Iteration

	std::vector<double> ptime(npick);
	for (size_t i=0; i<npick; i++) {
		ptime[i] = pps[i].projectedTime();
	}

	Origin* _origin = new Origin(hypocenter.lat, hypocenter.lon, hypocenter.dep, otime);

	// add Picks/Arrivals to that newly created Origin
	std::set<std::string> stations;
	for (size_t i=0; i<group.size(); i++) {
		const ProjectedPick &pp = group[i];

		PickCPtr pick = pp.p;
		const std::string key = station_key(pick->station());
		// avoid duplicate stations XXX ugly without amplitudes
		if ( stations.count(key))
			continue;
		stations.insert(key);

		StationWrapperCPtr sw( _wrappers[key]);

		Arrival arr(pick.get());
		arr.residual = pp.projectedTime() - otime;
		arr.distance = sw->distance;
		arr.azimuth  = sw->azimuth;
		arr.excluded = Arrival::NotExcluded;
		arr.phase = (pick->time - otime < 960.) ? "P" : "PKP";
//		arr.weight   = 1;
		_origin->arrivals.push_back(arr);
	}

	if (_origin->arrivals.size() < _nmin) {
		delete _origin;
		return nullptr;
	}

	return _origin;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int GridPoint::cleanup(const Time& minTime)
{
	int count = 0;

	// this is for counting only
	std::multiset<ProjectedPick>::iterator it, upper=_picks.upper_bound(minTime);
	for (it=_picks.begin(); it!=upper; ++it)
		count++;

	_picks.erase(_picks.begin(), upper);

	return count;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool GridPoint::setupStation(const Station *station)
{
	double delta=0, az=0, baz=0;
	delazi(&hypocenter, station, delta, az, baz);

	// Don't setup the grid point for a station if it is out of
	// range for that station - this reduces the memory used by
	// the grid
	if ( delta > station->maxNucDist )
		return false;

	TravelTime tt;
	if ( ! travelTimeP(hypocenter.lat, hypocenter.lon, hypocenter.dep, station->lat, station->lon, 0, delta, tt))
		return false;

	StationWrapperCPtr sw = new StationWrapper(station, tt.phase, delta, az, tt.time, tt.dtdd);
	std::string key = station_key (sw->station);
	_wrappers[key] = sw;

	return true;
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
static double depthFactor(double depth)
{
	// straight line, easy (but also risky!) to be made configurable
	return 1+0.0005*(200-depth);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
static PickSet originPickSet(const Origin *origin)
{
	PickSet picks;

	for (Arrival &arr : ((Origin*)origin)->arrivals) {
		if (arr.excluded)
			continue;
		picks.insert(arr.pick);
	}

	return picks;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
double originScore(const Origin *origin, double maxRMS, double networkSizeKm)
{
	((Origin*)origin)->arrivals.sort();

	double score = 0, amplScoreMax=0;
	size_t arrivalCount = origin->arrivals.size();
	//int n = origin->definingPhaseCount();
	for ( size_t i=0; i<arrivalCount; i++) {
		double phaseScore = 1; // 1 for P / 0.3 for PKP
		Arrival &arr = ((Origin*)origin)->arrivals[i];
		PickCPtr pick = arr.pick;
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

		// For a manual pick without SNR, as produced by
		// scolv, we assume a default value.
		if (manual(pick.get()) && pick->snr <= 0)
			snr = 10; // make this configureable

		double normamp = pick->normamp;
		if (manual(pick.get()) && normamp <= 0)
			normamp = 1; // make this configureable

		double snrScore = log10(snr);

		double d = arr.distance;
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
		if (amplScore < 1)
			amplScore = 1;

		// Amplitudes usually decrease with distance.
		// This hack takes this fact into account for computing the score.
		// A sudden big amplitude at large distance cannot increase the score too badly
		if (amplScoreMax==0)
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
		// higher residual. Note that this may increase the score
		// for origins with less arrivals, which might be harmful.

		double timeScore = avgfn2(arr.residual/(2*maxRMS));

		arr.dscore = distScore;
		arr.ascore = amplScore;
		arr.tscore = timeScore;

		switch (arr.excluded) {
			case Arrival::UnusedPhase:
				if (arr.phase.substr(0,3) == "PKP")
					phaseScore = 0.3;
				else
					phaseScore = 0.1;
				break;

			// REVIEW!
			// case Arrival::TemporarilyExcluded:
			//	break;

			case Arrival::NotExcluded:
				break;

			default:
				continue;
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
	score *= depthFactor(origin->hypocenter.dep);

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
static Origin* bestOrigin(OriginVector &origins)
{
	double maxScore = 0;
	Origin* best = nullptr;

	for (auto& origin : origins) {
		double score = originScore(origin.get());
		if (score > maxScore) {
			maxScore = score;
			best = origin.get();
		}
	}

	return best;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool GridSearch::feed(const Pick *pick)
{
	_newOrigins.clear();

	if (_stations.size() == 0) {
		SEISCOMP_ERROR("\nGridSearch::feed() NO STATIONS SET\n");
		exit(1);
	}

	std::string net_sta = pick->net + "." + pick->sta;

	// link pick to station through pointer

	if ( ! pick->station()) {
		StationMap::const_iterator it = _stations.find(net_sta);
		if (it == _stations.end()) {
			SEISCOMP_ERROR_S("\nGridSearch::feed() NO STATION " + net_sta + "\n");
			return false;
		}
		SEISCOMP_ERROR("GridSearch::feed()  THIS SHOULD NEVER HAPPEN");
		pick->setStation((*it).second.get());
	}


	// Has the station been configured already? If not, do it now.

	bool stationSetupNeeded = false;
	if (_configuredStations.find(net_sta) == _configuredStations.end()) {
		_configuredStations.insert(net_sta);
		stationSetupNeeded = true;
		SEISCOMP_DEBUG_S("GridSearch: setting up station " + net_sta);
	}

	std::map<PickSet, OriginPtr> pickSetOriginMap;

	// Main loop
	//
	// Feed the new pick into the individual grid points
	// and save all "candidate" origins in originVector

	double maxScore = 0;
	for (GridPointPtr &gp : _grid) {

		if (stationSetupNeeded)
			gp->setupStation(pick->station());

		OriginCPtr origin = gp->feed(pick);
		if ( ! origin)
			continue;

		// look at the origin, check whether
		//  * it fulfils certain minimum criteria
		//  * we have already seen a similar but better origin

		// test minimum number of picks
		if (origin->arrivals.size() < 6) // TODO: make this limit configurable
			continue;
		// is the new pick part of the returned origin?
		if (origin->findArrival(pick) == -1)
			// this is actually an unexpected condition!
			continue;

		const PickSet pickSet = originPickSet(origin.get());
		// test if we already have an origin with this particular pick set
		if (pickSetOriginMap.find(pickSet) != pickSetOriginMap.end()) {
			double score1 = originScore(pickSetOriginMap[pickSet].get());
			double score2 = originScore(origin.get());
			if ( score2 <= score1 ) {
				continue;
			}
		}

		double score = originScore(origin.get());
		if ( score < 0.6*maxScore ) {
			continue;
		}

		if ( score > maxScore ) {
			maxScore = score;
		}

/*
		_relocator.useFixedDepth(true);
		OriginPtr relo = _relocator.relocate(origin.get());
		if ( ! relo)
			continue;

		double delta, az, baz;
		delazi(origin->lat, origin->lon, gp->lat, gp->lon, delta, az, baz);
		if (_config.maxRadiusFactor > 0 &&
		    delta > _config.maxRadiusFactor*gp->_radius) // XXX private
			continue;
*/

		OriginPtr newOrigin = new Origin(*origin.get());

		pickSetOriginMap[pickSet] = newOrigin;
	}

	OriginVector tempOrigins;
	for (auto& item: pickSetOriginMap) {
		Origin *origin = item.second.get();
		if (originScore(origin) < 0.6*maxScore) {
			continue;
		}

		_relocator.useFixedDepth(true);
		OriginPtr relo = _relocator.relocate(origin);
		if ( !relo ) {
			continue;
		}

		// see if the new pick is within the maximum allowed nucleation distance
		int index = relo->findArrival(pick);
		if ( index==-1 ) {
			SEISCOMP_ERROR("pick unexpectedly not found in GridSearch::feed()");
			continue;
		}
		if ( relo->arrivals[index].distance > pick->station()->maxNucDist ) {
			continue;
		}

		tempOrigins.push_back(relo);
	}

	// For all candidate origins in tempOrigins try to find the
	// "best" one. This is a bit problematic as we don't go back and retry
	// using the second-best but give up here. Certainly scope for
	// improvement.

	OriginPtr best = bestOrigin(tempOrigins);
	if ( best ) {
		_relocator.useFixedDepth(false);
		OriginPtr relo = _relocator.relocate(best.get());
		if ( relo ) {
			_newOrigins.push_back(relo);
		}
	}

	return _newOrigins.size() > 0;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool GridSearch::_readGrid(const std::string &gridfile)
{
	std::ifstream ifile(gridfile.c_str());

	if ( ifile.good() ) {
		SEISCOMP_DEBUG_S("Reading grid file for nucleator: " + gridfile);
	}
	else {
		SEISCOMP_ERROR_S("Failed to read grid file for nucleator: " + gridfile);
		return false;
	}

	_grid.clear();
	double lat, lon, dep, rad, dmax; int nmin;
	while ( ! ifile.eof() ) {
		std::string line;
		std::getline(ifile, line);

		Seiscomp::Core::trim(line);

		// Skip empty lines
		if ( line.empty() ) continue;

		// Skip comments
		if ( line[0] == '#' ) continue;

		std::istringstream iss(line, std::istringstream::in);

		if (iss >> lat >> lon >> dep >> rad >> dmax >> nmin) {
			GridPoint *gp = new GridPoint(lat, lon, dep);
			gp->_nmin = nmin;
			gp->_radius = rad;
			gp->maxStaDist = dmax;
			_grid.push_back(gp);
		}
	}
	SEISCOMP_DEBUG("read %d grid lines",int(_grid.size()));
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void GridSearch::setup()
{
//	_relocator.setStations(_stations);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


} // namespace Autoloc
