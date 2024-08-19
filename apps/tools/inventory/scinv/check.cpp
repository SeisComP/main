/***************************************************************************
 * Copyright (C) 2013 by gempa GmbH
 *
 * Author: Jan Becker
 * Email: jabe@gempa.de
 ***************************************************************************/

#define SEISCOMP_COMPONENT INVMGR

#include <seiscomp/logging/log.h>
#include <seiscomp/math/geo.h>
#include <seiscomp/datamodel/utils.h>

#include "check.h"


using namespace std;
using namespace Seiscomp;
using namespace Seiscomp::DataModel;
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
namespace {

constexpr double MaxElevation = 8900.0;
constexpr double MinElevation = -12000.0;

string nslc(const Network *obj) {
	return obj->code();
}

string nslc(const Station *obj) {
	return nslc(obj->network()) + "." + obj->code();
}

string nslc(const SensorLocation *obj) {
	return nslc(obj->station()) + "." + obj->code();
}

string nslc(const Stream *obj) {
	return nslc(obj->sensorLocation()) + "." + obj->code();
}


string toString(const Core::TimeWindow &tw ) {
	string res;
	if ( tw.startTime().microseconds() > 0 )
		res = tw.startTime().toString("%F %T.%f");
	else
		res = tw.startTime().toString("%F %T");

	res += " - ";

	if ( tw.endTime().valid() ) {
		if ( tw.endTime().microseconds() > 0 )
			res += tw.endTime().toString("%F %T.%f");
		else
			res += tw.endTime().toString("%F %T");
	}
	else
		res += "...";

	return res;
}


bool overlaps(const Core::TimeWindow &tw1, const Core::TimeWindow &tw2) {
	// Both end times are open -> overlap
	if ( !tw1.endTime().valid() && !tw2.endTime().valid() ) return true;

	// Check overlap on both closed epochs
	if ( tw1.endTime().valid() && tw2.endTime().valid() ) {
		if ( tw1.startTime() >= tw2.endTime() ) return false;
		if ( tw1.endTime() <= tw2.startTime() ) return false;
	}
	// tw1 is an open epoch
	else if ( !tw1.endTime().valid() ) {
		if ( tw2.endTime() <= tw1.startTime() ) return false;
	}
	// tw2 is an open epoch
	else {
		if ( tw1.endTime() <= tw2.startTime() ) return false;
	}

	return true;
}


const Core::TimeWindow *
overlaps(const Check::TimeWindows &epochs, const Core::TimeWindow &tw) {
	Check::TimeWindows::const_iterator it;
	for ( it = epochs.begin(); it != epochs.end(); ++it ) {
		const Core::TimeWindow &epoch = *it;
		if ( overlaps(epoch, tw) )
			return &epoch;
	}

	return nullptr;
}


bool outside(const Core::TimeWindow &fence, const Core::TimeWindow &child) {
	if ( child.startTime() < fence.startTime() ) return true;
	if ( !child.endTime().valid() && fence.endTime().valid() ) return true;

	// Check overlap on both closed epochs
	if ( fence.endTime().valid() && child.endTime().valid() ) {
		if ( child.startTime() > fence.endTime() ) return true;
		if ( child.endTime() > fence.endTime() ) return true;
	}

	return false;
}


}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Check::Check(Inventory *inv) : InventoryTask(inv) {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Check::setMaxElevationDifference(double maxElevationDifference) {
	_maxElevationDifference = maxElevationDifference;
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Check::setMaxDistance(double maxDistance) {
	_maxDistance = maxDistance;
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Check::setMaxDepth(double maxDepth) {
	_maxDepth = maxDepth;
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Check::check() {
	if ( !_inv ) {
		return false;
	}

	EpochMap networkEpochs;

	for ( size_t n = 0; n < _inv->networkCount(); ++n ) {
		Network *net = _inv->network(n);
		checkEpoch(net);
		checkOverlap(networkEpochs[net->code()], net);

		EpochMap stationEpochs;
		if ( net->stationCount() == 0 ) {
			log(LogHandler::Warning,
			    (string(net->className()) + " " + nslc(net) + "\n  "
			     "has no station - may not be considered for data processing").c_str(),
			    net, nullptr);
		}
		if ( net->code().empty() ) {
			log(LogHandler::Warning,
			    (string(net->className()) + "\n  "
			     "found network without network code. ID: " + net->publicID()).c_str(),
			    net, nullptr);
		}

		for ( size_t s = 0; s < net->stationCount(); ++s ) {
			Station *sta = net->station(s);
			if ( sta->code().empty() ) {
				log(LogHandler::Warning,
				    (string(net->className()) + " " + nslc(sta) + "\n  "
				     "found station without station code. ID: " + sta->publicID()).c_str(),
				    sta, nullptr);
			}
			checkEpoch(sta);
			checkOverlap(stationEpochs[sta->code()], sta);
			checkOutside(net, sta);

			EpochMap locationEpochs;
			double lat = 0.0;
			double lon = 0.0;
			bool lvalid = true;

			try {
				lat = sta->latitude();
			}
			catch ( ... ) {
				log(LogHandler::Warning,
				    (string(sta->className()) + " " + nslc(sta) + "\n  "
				     "latitude is not set").c_str(),
				    sta, nullptr);
				lvalid = false;
			}

			try {
				lon = sta->longitude();
			}
			catch ( ... ) {
				log(LogHandler::Warning,
				    (string(sta->className()) + " " + nslc(sta) + "\n  "
				     "longitude is not set").c_str(),
				    sta, nullptr);
				lvalid = false;
			}

			try {
				if ( (sta->elevation() > MaxElevation)
				    || (sta->elevation() < MinElevation) ) {
					log(LogHandler::Error,
					    (string(sta->className()) + " " + nslc(sta) + "\n  "
					     "station elevation out of range").c_str(),
					    sta, nullptr);
				}
			}
			catch ( ... ) {
				log(LogHandler::Warning,
				    (string(sta->className()) + " " + nslc(sta) + "\n  "
				     "elevation is not set").c_str(),
				    sta, nullptr);
			}

			if ( lvalid && lat == 0.0 && lon == 0.0 ) {
				log(LogHandler::Warning,
				    (string(sta->className()) + " " + nslc(sta) + "\n  "
				     "coordinates are 0.0/0.0").c_str(),
				    sta, nullptr);
			}

			if ( sta->sensorLocationCount() == 0 ) {
				log(LogHandler::Warning,
				    (string(sta->className()) + " " + nslc(sta) + "\n  "
				     "has no location - may not be considered for data processing").c_str(),
				    sta, nullptr);
			}

			for ( size_t l = 0; l < sta->sensorLocationCount(); ++l ) {
				SensorLocation *loc = sta->sensorLocation(l);
				checkEpoch(loc);
				checkOverlap(locationEpochs[loc->code()], loc);
				checkOutside(sta, loc);

				double llat = 0.0;
				double llon = 0.0;
				bool llvalid = true;

				try {
					llat = loc->latitude();
				}
				catch ( ... ) {
					log(LogHandler::Warning,
					    (string(loc->className()) + " " + nslc(loc) + "\n  "
					     "latitude is not set").c_str(),
					    loc, nullptr);
					llvalid = false;
				}

				try {
					llon = loc->longitude();
				}
				catch ( ... ) {
					log(LogHandler::Warning,
					    (string(loc->className()) + " " + nslc(loc) + "\n  "
					     "longitude is not set").c_str(),
					    loc, nullptr);
					llvalid = false;
				}

				try {
					loc->elevation();
					if ( (loc->elevation() > MaxElevation)
					    || (loc->elevation() < MinElevation) ) {
						log(LogHandler::Error,
						    (string(loc->className()) + " " + nslc(loc) + "\n  "
						     "sensor location elevation out of range").c_str(),
						    loc, nullptr);
					}
				}
				catch ( ... ) {
					log(LogHandler::Warning,
					    (string(loc->className()) + " " + nslc(loc) + "\n  "
					     "elevation is not set").c_str(),
					    loc, nullptr);
				}

				if ( llvalid && llat == 0.0 && llon == 0.0 ) {
					log(LogHandler::Warning,
					    (string(loc->className()) + " " + nslc(loc) + "\n  "
					     "coordinates are 0.0/0.0").c_str(),
					    loc, nullptr);
				}

				if ( lvalid && llvalid ) {
					double dist,a1,a2;

					Math::Geo::delazi(lat,lon,llat,llon, &dist, &a1, &a2);
					dist = Math::Geo::deg2km(dist);
					if ( dist > _maxDistance ) {
						log(LogHandler::Warning,
						    (string(loc->className()) + " " + nslc(loc) + "\n  "
						     "location is " + Core::toString(dist) + " km away from parent station. "
						     "Distances > " + Core::toString(_maxDistance) + " km "
						     "are reported as per configuration").c_str(),
						    loc, nullptr);
					}
				}

				try {
					double elevationDiff = sta->elevation() - loc->elevation();
					if ( abs(elevationDiff) > _maxElevationDifference ) {
						log(LogHandler::Warning,
						    (string(loc->className()) + " " + nslc(loc) + "\n  "
						     "sensor location is " + Core::toString(elevationDiff) +
						     " m below parent station. Differences > "
						     + Core::toString(_maxElevationDifference) + " m "
						     "are reported as per configuration").c_str(),
						    loc, nullptr);
					}
				}
				catch ( ... ) {}

				EpochMap channelEpochs;
				if ( loc->streamCount() == 0 ) {
					log(LogHandler::Warning,
					    (string(loc->className()) + " " + nslc(loc) + "\n  "
					     "has no stream - may not be considered for data processing").c_str(),
					     loc, nullptr);
				}

				map<string, int> streamMap;
				set<string> streamSet;
				map<string, set<Seiscomp::Core::Time>> startTimes;
				for ( size_t c = 0; c < loc->streamCount(); ++c ) {
					Stream *cha = loc->stream(c);
					string group = cha->code().substr(0,2);

					// collect the channels
					streamSet.insert(cha->code());
					/*
					auto it = streamMap.find(group);
					if ( it == streamMap.end() ) {
						streamMap.insert(pair<string,int>(group,0));
					}
					else {
						it->second++;
					}
					*/
					++streamMap[group];

					// collect the channel epochs
					startTimes[group].insert(cha->start());

					checkEpoch(cha);
					checkOverlap(channelEpochs[cha->code()], cha);
					checkOutside(loc, cha);

					try {
						if ( cha->gain() == 0.0 ) {
							log(LogHandler::Warning,
							    (string(cha->className()) + " " + nslc(cha) + "\n  "
							     "invalid gain of 0").c_str(),
							    cha, nullptr);
						}
					}
					catch ( ... ) {
						log(LogHandler::Warning,
						    (string(cha->className()) + " " + nslc(cha) + "\n  "
						     "no gain set").c_str(),
						    cha, nullptr);
					}

					try {
						cha->dip();
						if ( cha->gain() < 0.0 && cha->dip() > 0.0 ) {
							log(LogHandler::Information,
							    (string(cha->className()) + " " + nslc(cha) + "\n  "
							     "negative gain and positive dip, consider positive gain and negative dip").c_str(),
							    cha, nullptr);
						}
					}
					catch ( Seiscomp::Core::ValueException &e ) {
						log(LogHandler::Warning,
						    (string(cha->className()) + " " + nslc(cha) + "\n  "
						     + e.what()).c_str(),
						    cha, nullptr);
					}

					try {
						cha->azimuth();
					}
					catch ( Seiscomp::Core::ValueException &e ) {
						log(LogHandler::Warning,
						    (string(cha->className()) + " " + nslc(cha) + "\n  "
						     + e.what()).c_str(),
						    cha, nullptr);
					}

					if ( cha->gainUnit().empty() ) {
						log(LogHandler::Warning,
						    (string(cha->className()) + " " + nslc(cha) + "\n  "
						     "no gain unit set").c_str(),
						    cha, nullptr);
					}

					try {
						if ( cha->depth() < 0.0 ) {
							log(LogHandler::Warning,
							    (string(cha->className()) + " " + nslc(cha) + "\n  "
							     "channel depth is " + Core::toString(cha->depth())
							     + " m which seems unreasonable").c_str(),
							    cha, nullptr);
						}

						if ( cha->depth() > _maxDepth ) {
							log(LogHandler::Warning,
							    (string(cha->className()) + " " + nslc(cha) + "\n  "
							     "channel depth is " + Core::toString(cha->depth())
							     + " m. Depths > " + Core::toString(_maxDepth)
							     + " m are reported as per configuration").c_str(),
							    cha, nullptr);
						}
					}
					catch ( ... ) {
						log(LogHandler::Warning,
						    (string(cha->className()) + " " + nslc(cha) + "\n  "
						     "no channel depth set").c_str(),
						    cha, nullptr);
					}

					if ( !cha->sensor().empty() ) {
						// Done already in merge
						/*
						Sensor *sensor = findSensor(cha->sensor());
						if ( !sensor ) {
							log(LogHandler::Unresolved,
							    (string(cha->className()) + " " + id(cha) + "\n  "
							     "referenced sensor is not available").c_str(), nullptr, nullptr);
						}
						*/
					}
					else {
						log(LogHandler::Information,
						    (string(cha->className()) + " " + nslc(cha) + "\n  "
						     "no sensor and thus no response information available").c_str(),
						    cha, nullptr);
					}

					if ( cha->datalogger().empty() ) {
						log(LogHandler::Information,
						    (string(cha->className()) + " " + nslc(cha) + "\n  "
						     "no data logger and thus no response information available").c_str(),
						    cha, nullptr);
					}

				}

				// check number of channels and orthogonality
				for ( auto &item : streamMap ) {
					// do not continue only if stream group has only 1 component
					auto group = item.first;
					item.second = 0;
					for ( const auto &str : streamSet ) {
						if ( (str.size() >= 2) && (str.substr(0, 2) == group) ) {
							++item.second;
						}
					}
					if ( item.second <= 1 ) {
						continue;
					}

					// limit the check to some sensor types
					auto sensor = group.substr(1,1);
					if ( sensor.compare("G") != 0 && sensor.compare("H") != 0
					     && sensor.compare("L") != 0
					     && sensor.compare("N") != 0 ) {
						continue;
					}

					auto starts = startTimes.find(group);
					if ( starts == startTimes.end() ) {
						continue;
					}

					for ( const auto &start : starts->second ) {
						const int countStream = DataModel::numberOfComponents(loc, group.c_str(), start);

						// Do not report 1-C stream groups
						if ( countStream == 1 ) {
							continue;
						}
						// check if there are exactly 3 components
						if ( countStream !=  3 ) {
							log(LogHandler::Information,
							    (string(loc->className()) + " " + id(loc) + "."
							     + group + "?\n  found " + std::to_string(countStream)
							     + " but not 3 components in " + toString(start)).c_str(),
							    loc, nullptr);
							continue;
						}

						DataModel::ThreeComponents tc;
						if ( !DataModel::getThreeComponents(tc, loc, group.c_str(), start) ) {
							log(LogHandler::Warning,
							    (string(loc->className()) + " " + id(loc) + "." + group + "?\n"
							     "  streams are not orthogonal in " + toString(start) +
							     " - may not be considered for data processing").c_str(),
							    loc, nullptr);
						}
					}
				}
			}
		}
	}

	for ( size_t i = 0; i < _inv->sensorCount(); ++i ) {
		Sensor *sensor = _inv->sensor(i);
		Object *o = findPAZ(sensor->response());
		if ( !o ) {
			o = findPoly(sensor->response());
		}
		if ( !o ) {
			o = findFAP(sensor->response());
		}
		if ( !o ) {
			// Done in merge
			/*
			log(LogHandler::Unresolved,
			    (string(sensor->className()) + " " + id(sensor) + "\n  "
			     "referenced response is not available").c_str(), nullptr, nullptr);
			*/
		}
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
template <typename T>
void Check::checkEpoch(const T *obj) {
	try {
		if ( obj->start() >= obj->end() ) {
			log(LogHandler::Error,
			    (string(obj->className()) + " " + nslc(obj) + "\n  "
			     "invalid epoch: start >= end: " +
			     toString(Core::TimeWindow(obj->start(), obj->end()))).c_str(),
			    obj, nullptr);
		}
	}
	catch ( ... ) {}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
template <typename T>
void Check::checkOverlap(TimeWindows &epochs, const T *obj) {
	Core::Time end;
	try { end = obj->end(); }
	catch ( ... ) {}

	Core::TimeWindow epoch(obj->start(), end);
	const Core::TimeWindow *tw = overlaps(epochs, epoch);
	if ( tw != nullptr ) {
		log(LogHandler::Conflict,
		    (string(obj->className()) + " " + nslc(obj) + "\n  "
		     "overlapping epochs " +
		     toString(epoch) + " and " + toString(*tw)).c_str(),
		    obj, obj);
	}

	epochs.push_back(epoch);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
template <typename T1, typename T2>
void Check::checkOutside(const T1 *parent, const T2 *obj) {
	Core::Time pend, end;

	try { pend = parent->end(); }
	catch ( ... ) {}
	try { end = obj->end(); }
	catch ( ... ) {}

	Core::TimeWindow pepoch(parent->start(), pend);
	Core::TimeWindow epoch(obj->start(), end);

	if ( outside(pepoch, epoch) ) {
		log(LogHandler::Conflict,
		    (string(obj->className()) + " " + nslc(obj) + "\n  "
		     "epoch " + toString(epoch) + " outside parent " +
		     parent->className() + " epoch " + toString(pepoch)).c_str(),
		    obj, obj);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Sensor *Check::findSensor(const string &id) const {
	for ( size_t i = 0; i < _inv->sensorCount(); ++i ) {
		Sensor *sensor = _inv->sensor(i);
		if ( sensor->publicID() == id ) return sensor;
	}

	return nullptr;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
ResponsePAZ *Check::findPAZ(const string &id) const {
	for ( size_t i = 0; i < _inv->responsePAZCount(); ++i ) {
		ResponsePAZ *paz = _inv->responsePAZ(i);
		if ( paz->publicID() == id ) return paz;
	}

	return nullptr;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
ResponsePolynomial *Check::findPoly(const string &id) const {
	for ( size_t i = 0; i < _inv->responsePolynomialCount(); ++i ) {
		ResponsePolynomial *poly = _inv->responsePolynomial(i);
		if ( poly->publicID() == id ) return poly;
	}

	return nullptr;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
ResponseFAP *Check::findFAP(const std::string &id) const {
	for ( size_t i = 0; i < _inv->responseFAPCount(); ++i ) {
		ResponseFAP *fap = _inv->responseFAP(i);
		if ( fap->publicID() == id ) return fap;
	}

	return nullptr;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
