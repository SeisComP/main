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
#include <vector>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <map>
#include <algorithm>
#include <cmath>

#include "util.h"
#include "nucleator.h"
#include "datamodel.h"


namespace Seiscomp {

namespace AutolocInternal {


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Seiscomp::Core::Time sctime(const Time &time)
{
	return Seiscomp::Core::Time() + Seiscomp::Core::TimeSpan(time);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void delazi(double lat1, double lon1, double lat2, double lon2,
            double &delta, double &az1, double &az2) {
	Math::Geo::delazi(lat1, lon1, lat2, lon2, &delta, &az1, &az2);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void delazi(const Hypocenter *hypo, const Station *station,
            double &delta, double &az1, double &az2) {
	Math::Geo::delazi(hypo->lat, hypo->lon, station->lat, station->lon, &delta, &az1, &az2);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
double distance(const Station* s1, const Station* s2) {
	double delta, az, baz;
	delazi(s1->lat, s1->lon, s2->lat, s2->lon, delta, az, baz);
	return delta;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
std::string printDetailed(const Origin *origin) {
	return printOrigin(origin, false);
}

// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
std::string printOneliner(const Origin *origin) {
	return printOrigin(origin, true);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool automatic(const Pick *pick) {
	return pick->mode == Pick::Automatic;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool ignored(const Pick *pick) {
	return pick->mode == Pick::IgnoredAutomatic;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool manual(const Pick *pick) {
	return pick->mode == Pick::Manual;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
char modeFlag(const Pick *pick) {
	return automatic(pick) ? 'A' : 'M';
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool hasAmplitude(const Pick *pick) {
	if ( pick->amp <= 0 ) {
		return false;
	}
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool travelTimeP(double lat1, double lon1, double dep1, double lat2, double lon2, double alt2, double delta, TravelTime &result) {
	static TravelTimeTable ttt;

	TravelTimeList *ttlist { nullptr };

	try {
		ttlist = ttt.compute(lat1, lon1, std::max(dep1, 0.01), lat2, lon2, alt2);
	}
	catch ( std::out_of_range & ) {
		return false;
	}
	if ( !ttlist ) {
		return false;
	}

	for ( auto& tt : *ttlist ) {
		result = tt;
		if ( delta < 114 ) {
			// for  distances < 114, allways take 1st arrival
			break;
		}
		if ( tt.phase.substr(0,2) != "PK" ) {
			// for  distances >= 114, skip Pdiff etc., take first
			// PKP*, PKiKP*
			continue;
		}
		break;
	}
	delete ttlist;

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
std::string time2str(const Time &t) {
	return sctime(t).toString("%F %T.%f000000").substr(0, 21);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
double meandev(const Origin* origin) {
	double cumresid {0};
	double cumweight {0};

	for (const Arrival &arr : origin->arrivals) {
		if ( arr.excluded ) {
			continue;
		}
		cumresid  += std::abs(arr.residual);
		cumweight += 1;
	}

	if ( cumweight == 0. ) {
		return 0;
	}

	return cumresid/cumweight;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
double avgfn(double x) {
	if ( x<-1 || x>1 ) {
		return 0;
	}

	x *= M_PI*0.5;
	x = cos(x);
	return x*x;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
std::string printOrigin(const Origin *origin, bool oneliner) {
	if ( !origin ) {
		return "invalid origin";
	}

	std::ostringstream out;

	int precision = out.precision();

	out.precision(10);

	char depthFlag = ' ';
	switch (origin->depthType) {
	case Origin::DepthMinimum:       depthFlag = 'i'; break;
	case Origin::DepthPhases:        depthFlag = 'p'; break;
	case Origin::DepthDefault:       depthFlag = 'd'; break;
	case Origin::DepthManuallyFixed: depthFlag = 'f'; break;
	default:                         depthFlag = ' ';
	}

	if ( oneliner ) {
		double score = originScore(origin);
		char s[200];
		std::string tstr = time2str(origin->time).substr(11);
		snprintf(s, 200, "%-6lu %s %6.2f %7.2f %3.0f%c %4.1f %3ld %3ld s=%6.1f",
		         origin->id, tstr.c_str(),
		         origin->hypocenter.lat, origin->hypocenter.lon, origin->hypocenter.dep, depthFlag,
		         origin->rms(), origin->definingPhaseCount(),
		         origin->arrivals.size(), score);
		out << s;
	}
	else {
		out << "Detailed info for Origin " << origin->id << std::endl
		    << time2str(origin->time) << "  ";
		out.precision(5);
		out << origin->hypocenter.lat << "  " << origin->hypocenter.lon << "  ";
		out.precision(3);
		out << origin->hypocenter.dep << depthFlag << std::endl;

		out.setf(std::ios::right);

		size_t lineno = 0;
		for (const Arrival &arr : origin->arrivals) {
			const Pick* pick = arr.pick.get();

			std::string excludedFlag;
			switch (arr.excluded) {
			case Arrival::NotExcluded:          excludedFlag = "  "; break;
			case Arrival::LargeResidual:        excludedFlag = "Xr"; break;
			case Arrival::StationDistance:      excludedFlag = "Xd"; break;
			case Arrival::ManuallyExcluded:     excludedFlag = "Xm"; break;
			case Arrival::UnusedPhase:          excludedFlag = "Xp"; break;
			case Arrival::DeterioratesSolution: excludedFlag = "X!"; break;
			case Arrival::TemporarilyExcluded:  excludedFlag = "Xt"; break;
			default:                            excludedFlag = "X ";
			}
			if ( !pick->station() ) {
				out << pick->id() << "   missing station information" << std::endl;
				continue;
			}

			std::string net = pick->station()->net;
			std::string sta = pick->station()->code;

			out.precision(1);
			out     << std::left << std::setw(4) << (++lineno)
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
				<< ((arr.pick->mode == Pick::Automatic) ?"A":"M") // << " "
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
	}
	out.precision(precision);

	return out.str();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




namespace Utils {

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
StationMap *readStationLocations(const std::string &fname) {
	StationMap *stations = new StationMap;
	std::string code, net;
	double lat, lon, alt;

	std::ifstream ifile(fname.c_str());
	while (ifile >> net >> code >> lat >> lon >> alt) {

		Station* station = new Station(code, net, lat, lon, alt);
		station->maxNucDist = 180; // TODO make this default configurable
		station->enabled = true;
		std::string key = net + "." + code;
		stations->insert(std::pair<std::string, Station*>(key, station));
	}

	if ( stations->size() == 0) {
		SEISCOMP_ERROR_S("No stations read from file " + fname);
		delete stations;
		return nullptr;
	}

	return stations;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
DataModel::Inventory* inventoryFromStationLocationFile(const std::string &filename) {
	// read inventory from station locations file
	StationMap *stationMap = readStationLocations(filename);

	DataModel::Inventory *inventory = new DataModel::Inventory;
	for ( auto& item : *stationMap ) {
		std::string key = item.first;
		const Station *s = item.second.get();
		std::string netId = "Network/"+s->net;
		DataModel::Network *network = inventory->findNetwork(netId);
		if ( !network) {
			network = new DataModel::Network(netId);
			network->setCode(s->net);
			inventory->add(network);
		}

		std::string staId = "Station/"+s->net+"/"+s->code;
		DataModel::Station *station = new DataModel::Station(staId);
		station->setCode(s->code);
		station->setLatitude(s->lat);
		station->setLongitude(s->lon);
		station->setElevation(s->alt);
		network->add(station);
	}
	delete stationMap;

	return inventory;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Pick::Mode mode(const DataModel::Pick *pick) {
	try {
		switch ( pick->evaluationMode() ) {
			case DataModel::AUTOMATIC:
				return Pick::Automatic;
			case DataModel::MANUAL:
				return Pick::Manual;
			//case DataModel::CONFIRMED_PICK:
			//	return Pick::Confirmed;
			default:
				break;
		}
	}
	catch ( ... ) {}

	return Pick::Automatic;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void minimizeInventory(Seiscomp::DataModel::Inventory *inventory) {
	// Remove unneeded inventory items to save some memory
	if ( !inventory ) {
		return;
	}
	
	for ( size_t n = 0; n < inventory->networkCount(); ++n ) {
		DataModel::Network *network = inventory->network(n);

		for ( size_t s = 0; s < network->stationCount(); ++s ) {
			DataModel::Station *station = network->station(s);

			for ( size_t l = 0; l < station->sensorLocationCount(); ++l ) {
				DataModel::SensorLocation *sensorLocation = station->sensorLocation(l);
				while (sensorLocation->streamCount())
					sensorLocation->removeStream(0);
				while (sensorLocation->auxStreamCount())
					sensorLocation->removeAuxStream(0);
				while (sensorLocation->commentCount())
					sensorLocation->removeComment(0);
			}
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


} // namespace AutolocInternal::Utils
} // namespace AutolocInternal


namespace Math {
namespace Statistics {

double rms(const std::vector<double> &v, double offset) {
	double r{0};

	if (v.empty())
		return 0;

	if ( offset ) {
		for ( double f : v ) {
			f -= offset;
			r += f*f;
		}
	}
	else {
		for ( double f : v ) {
			r += f*f;
		}
	}

	return sqrt(r/v.size());
}

} // namespace Statistics
} // namespace Math
} // namespace Seiscomp

