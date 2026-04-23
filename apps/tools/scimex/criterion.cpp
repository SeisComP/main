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


#define SEISCOMP_COMPONENT IMEX

#include <seiscomp/logging/log.h>
#include <algorithm>

#include "criterion.h"


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
namespace Seiscomp {
namespace Applications {
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
CriterionFactory::CriterionFactory(const std::string &sinkName,
                                   Client::Application *app)
: _sinkName(sinkName), _app(app)
{}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Utils::LeExpression *CriterionFactory::createExpression(std::string_view name) const {
	auto criterion = new Criterion;

	if ( !configGetLatitude("criteria." + std::string(name), "latitude", criterion) ) {
		delete criterion;
		return nullptr;
	}

	if ( !configGetLongitude("criteria." + std::string(name), "longitude", criterion) ) {
		delete criterion;
		return nullptr;
	}

	if ( !configGetMagnitude("criteria." + std::string(name), "magnitude", criterion) ) {
		delete criterion;
		return nullptr;
	}

	if ( !configGetArrivalCount("criteria." + std::string(name), "arrivalCount", criterion) ) {
		delete criterion;
		return nullptr;
	}

	if ( !configGetAgencyID("criteria." + std::string(name), "agencyID", criterion) ) {
		delete criterion;
		return nullptr;
	}

	return criterion;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool CriterionFactory::configGetLatitude(const std::string &prefix,
                                         const std::string &name,
                                         Criterion *criterion) const {
	return getRangeFromConfig(
		prefix, name,
		std::bind(
			&Criterion::setLatitudeRange,
			criterion,
			std::placeholders::_1, std::placeholders::_2
		)
	);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool CriterionFactory::configGetLongitude(const std::string &prefix,
                                          const std::string &name,
                                          Criterion *criterion) const {
	return getRangeFromConfig(
		prefix, name,
		std::bind(
			&Criterion::setLongitudeRange, criterion,
			std::placeholders::_1, std::placeholders::_2
		)
	);

}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool CriterionFactory::configGetMagnitude(const std::string&prefix,
                                          const std::string &name,
                                          Criterion *criterion) const {
	return getRangeFromConfig(
		prefix, name,
		std::bind(
			&Criterion::setMagnitudeRange, criterion,
			std::placeholders::_1, std::placeholders::_2
		)
	);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool CriterionFactory::configGetArrivalCount(const std::string &prefix,
                                             const std::string &name,
                                             Criterion *criterion) const {
	try {
		criterion->setArrivalCount(_app->configGetInt(prefix + ".arrivalcount"));
	}
	catch ( Config::Exception &e ) {
		SEISCOMP_ERROR("(%s) %s", _sinkName, e.what());
		return false;
	}
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool CriterionFactory::configGetAgencyID(const std::string &prefix,
                                         const std::string &name,
                                         Criterion *criterion) const {
	std::vector<std::string> tokens;
	try {
		std::string agencyIDs =  _app->configGetString(prefix + ".agencyID");
		if ( !agencyIDs.empty() ) {
			Core::split(tokens, agencyIDs.c_str(), ",");
			std::vector<std::string>::iterator it = tokens.begin();
			for ( ; it != tokens.end(); ++it ) {
				Core::trim(*it);
			}
			criterion->setAgencyIDs(tokens);
		}
	}
	catch ( Config::Exception &e ) {
		SEISCOMP_ERROR("(%s) %s", _sinkName, e.what());
		return false;
	}
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
template <typename FuncObj>
bool CriterionFactory::getRangeFromConfig(const std::string &prefix,
                                          const std::string &name,
                                          FuncObj funcObj) const {
	std::vector<std::string> tokens;
	try {
		double val0 = 0, val1 = 0;
		std::string range = _app->configGetString(prefix + "." + name);
		if ( Core::split(tokens, range.c_str(), ":") == 2 ) {
			if ( !Core::fromString(val0, tokens[0]) ) {
				SEISCOMP_ERROR("(%s) Malformed %s: %s", _sinkName, name, tokens[0]);
				return false;
			}
			if ( !Core::fromString(val1, tokens[1]) ) {
				SEISCOMP_ERROR("(%s) Malformed %s: %s", _sinkName, name, tokens[1]);
				return false;
			}
			funcObj(val0, val1);
		}
		else {
			SEISCOMP_ERROR("(%s) Malformed %s range: %s", _sinkName, name, range);
			return false;
		}
	}
	catch ( Config::Exception &e ) {
		SEISCOMP_ERROR("(%s) %s", _sinkName, e.what());
		return false;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const Criterion::LatitudeRange &Criterion::latitudeRange() const {
	return _latitudeRange;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Criterion::setLatitudeRange(double lat0, double lat1) {
	_latitudeRange = std::make_pair(lat0, lat1);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const Criterion::LongitudeRange& Criterion::longitudeRange() const {
	return _longitudeRange;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Criterion::setLongitudeRange(double lon0, double lon1) {
	_longitudeRange = std::make_pair(lon0, lon1);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const Criterion::MagnitudeRange& Criterion::magnitudeRange() const {
	return _magnitudeRange;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Criterion::setMagnitudeRange(double mag0, double mag1) {
	_magnitudeRange = std::make_pair(mag0, mag1);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
size_t Criterion::arrivalCount() const {
	return _arrivalCount;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Criterion::setArrivalCount(size_t count) {
	_arrivalCount = count;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const Criterion::AgencyIDs& Criterion::agencyIDs() const {
	return _agencyIDs;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Criterion::setAgencyIDs(const AgencyIDs& ids) {
	_agencyIDs = ids;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Criterion::isInLatLonRange(double lat, double lon) {
	bool result = true;

	SEISCOMP_DEBUG("Checking latitude/longitude");

	std::ostringstream ss;

	if ( (lat < _latitudeRange.first) || (lat > _latitudeRange.second) ) {
		result = false;
		ss << "Latitude " << lat << " not in [" << _latitudeRange.first << ":" << _latitudeRange.second << "]";
	}

	if ( (lon < _longitudeRange.first) || (lon > _longitudeRange.second) ) {
		if ( !result ) {
			ss << std::endl;
		}

		result = false;
		ss << "Longitude " << lon << " not in [" << _longitudeRange.first << ":" << _longitudeRange.second << "]";
	}

	if ( !result ) {
		SEISCOMP_DEBUG("= latitude/longitude mismatch =");
		SEISCOMP_DEBUG("%s", ss.str());
	}

	return result;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Criterion::isInMagnitudeRange(double mag) {
	if ( (mag >= _magnitudeRange.first) && (mag <= _magnitudeRange.second) ) {
		return true;
	}

	SEISCOMP_DEBUG("= magnitude mismatch =");
	SEISCOMP_DEBUG("Magnitude %f not in [%f:%f]", mag, _magnitudeRange.first, _magnitudeRange.second);
	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Criterion::checkArrivalCount(size_t count) {
	SEISCOMP_DEBUG("Checking arrival count");

	if ( count >= _arrivalCount ) {
		return true;
	}

	SEISCOMP_DEBUG("Number of arrivals %ld is below the minimum", count);

	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Criterion::checkAgencyID(const std::string &id) {
	SEISCOMP_DEBUG("Checking agencyID");

	// Every agency is eligible
	if ( _agencyIDs.empty() ) {
		return true;
	}

	auto it = std::find(_agencyIDs.begin(), _agencyIDs.end(), id);
	if ( it != _agencyIDs.end() ) {
		return true;
	}

	SEISCOMP_DEBUG("Could not find agencyID: %s", id);

	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Criterion::eval(const void *context) {
	auto ctx = reinterpret_cast<const FilterContext*>(context);

	if ( ctx->org ) {
		return isInLatLonRange(ctx->org->latitude().value(), ctx->org->longitude().value()) &&
		       checkArrivalCount(ctx->org->arrivalCount()) &&
		       checkAgencyID(objectAgencyID(ctx->org));
	}

	if ( ctx->mag ) {
		return isInMagnitudeRange(ctx->mag->magnitude().value());
	}

	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
} // namespace Applications
} // namespace Seiscomp
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
