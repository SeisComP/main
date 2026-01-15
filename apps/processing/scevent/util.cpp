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


#include "util.h"
#include "string.h"

#include <seiscomp/core/strings.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/focalmechanism.h>
#include <seiscomp/datamodel/magnitude.h>
#include <seiscomp/datamodel/event.h>
#include <seiscomp/datamodel/databasearchive.h>
#include <seiscomp/seismology/regions.h>

#define SEISCOMP_COMPONENT SCEVENT
#include <seiscomp/logging/log.h>


using namespace std;
using namespace Seiscomp::Core;
using namespace Seiscomp::DataModel;


namespace Seiscomp {
namespace Private {
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
std::string makeUpper(const std::string &src) {
	string dest = src;
	for ( size_t i = 0; i < src.size(); ++i )
		dest[i] = toupper(src[i]);
	return dest;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
std::string encode(uint64_t x, const char *sym, int numsym,
                   int len, uint64_t *width) {
	string enc;

	if ( len == 0 ) {
		len = 4;
	}

	uint64_t ms_total = uint64_t(((366 * 24) * 60) * 60) * 1000;
	uint64_t slot_total = 1;

	for ( int i = 0; i < len; ++i ) {
		auto tmp = slot_total * numsym;
		if ( tmp > slot_total ) {
			slot_total = tmp;
		}
		else {
			len = i;
			break;
		}
	}

	double ms_per_slot = ms_total > slot_total ? static_cast<double>(ms_total) / static_cast<double>(slot_total) : 1;
	x /= ms_per_slot;

	for ( int i = 0; i < len; ++i ) {
		uint64_t d = x / numsym;
		uint64_t r = x % numsym;
		enc += sym[r];
		x = d;
	}

	if ( width ) {
		*width = static_cast<uint64_t>(ceil(ms_per_slot));
	}

	return string(enc.rbegin(), enc.rend());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
string encodeChar(uint64_t x, int len, uint64_t *width) {
	return encode(x, "abcdefghijklmnopqrstuvwxyz", 26, len, width);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
string encodeInt(uint64_t x, int len, uint64_t *width) {
	return encode(x, "0123456789", 10, len, width);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
string encodeHex(uint64_t x, int len, uint64_t *width) {
	return encode(x, "0123456789abcdef", 16, len, width);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
std::string generateEventID(int year, uint64_t x, const std::string &prefix,
                            const string &pattern, std::string &textBlock, uint64_t *width) {
	string evtID;
	textBlock = "";

	for ( size_t i = 0; i < pattern.size(); ++i ) {
		if ( pattern[i] != '%' )
			evtID += pattern[i];
		else {
			++i;
			int len = 0;
			while ( i < pattern.size() ) {
				if ( pattern[i] >= '0' && pattern[i] <= '9' ) {
					len *= 10;
					len += int(pattern[i] - '0');
					++i;
					continue;
				}
				else if ( pattern[i] == '%' ) {
					evtID += pattern[i];
				}
				else if ( pattern[i] == 'c' ) {
					textBlock = encodeChar(x, len, width);
					evtID += textBlock;
				}
				else if ( pattern[i] == 'C' ) {
					textBlock = makeUpper(encodeChar(x, len, width));
					evtID += textBlock;
				}
				else if ( pattern[i] == 'd' ) {
					textBlock = encodeInt(x, len, width);
					evtID += textBlock;
				}
				else if ( pattern[i] == 'x' ) {
					textBlock = encodeHex(x, len, width);
					evtID += textBlock;
				}
				else if ( pattern[i] == 'X' ) {
					textBlock = makeUpper(encodeHex(x, len, width));
					evtID += textBlock;
				}
				else if ( pattern[i] == 'Y' )
					evtID += toString(year);
				else if ( pattern[i] == 'p' )
					evtID += prefix;
				else
					return "";

				break;
			}
		}
	}

	return evtID;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
string allocateEventID(DatabaseArchive *ar, const Origin *origin,
                       const Seiscomp::Client::Config &config) {
	if ( !origin ) {
		return string();
	}

	int year, yday, hour, min, sec, usec;

	if ( !origin->time().value().get2(&year, &yday, &hour, &min, &sec, &usec) ) {
		return string();
	}

	uint64_t width; // in milliseconds
	// Maximum precission is 1 millisecond
	uint64_t x = uint64_t((((yday * 24) + hour) * 60 + min) * 60 + sec) * 1000 + usec / 1000;

	string text;
	string eventID = generateEventID(year, x, config.eventIDPrefix,
	                                 config.eventIDPattern, text, &width);
	ObjectPtr o = ar?ar->getObject(Event::TypeInfo(), eventID):Event::Find(eventID);
	bool blocked = config.blacklistIDs.find(text) != config.blacklistIDs.end();

	if ( !o && !blocked ) {
		return eventID;
	}

	if ( blocked ) {
		SEISCOMP_WARNING("Blocked ID: %s (rejected %s)", eventID.c_str(), text.c_str());
	}

	int eventIDLookupMargin = config.eventIDLookupMargin;

	if ( config.eventIDLookupMargin < 0 ) {
		// Auto mode, compute the number of slots based on half an hour.
		eventIDLookupMargin = static_cast<int>(config.eventAssociation.eventTimeAfter.length() * 1000 / width);
		SEISCOMP_DEBUG("Using event ID slot ahead lookup of %d", eventIDLookupMargin);
	}

	for ( int i = 1; i < eventIDLookupMargin; ++i ) {
		eventID = generateEventID(year, x+i*width, config.eventIDPrefix,
		                          config.eventIDPattern, text);
		blocked = config.blacklistIDs.find(text) != config.blacklistIDs.end();
		o = ar?ar->getObject(Event::TypeInfo(), eventID):Event::Find(eventID);

		if ( !o && !blocked ) {
			return eventID;
		}

		if ( blocked ) {
			SEISCOMP_WARNING("Blocked ID: %s (rejected %s)", eventID.c_str(), text.c_str());
		}
	}

	if ( config.eventIDLookupMargin < 0 ) {
		// Auto mode, compute the number of slots based on half an hour.
		eventIDLookupMargin = static_cast<int>(config.eventAssociation.eventTimeBefore.length() * 1000 / width);
		SEISCOMP_DEBUG("Using event ID slot back lookup of %d", eventIDLookupMargin);
	}

	for ( int i = 1; i < eventIDLookupMargin; ++i ) {
		eventID = generateEventID(year, x-i*width, config.eventIDPrefix,
		                          config.eventIDPattern, text);
		blocked = config.blacklistIDs.find(text) != config.blacklistIDs.end();
		o = ar?ar->getObject(Event::TypeInfo(), eventID):Event::Find(eventID);

		if ( !o && !blocked ) {
			return eventID;
		}

		if ( blocked ) {
			SEISCOMP_WARNING("Blocked ID: %s (rejected %s)", eventID.c_str(), text.c_str());
		}
	}

	return string();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
string region(const Origin *origin, bool withFERegions) {
	if ( withFERegions ) {
		return Regions::getRegionName(origin->latitude(), origin->longitude());
	}
	else {
		return Regions::polyRegions().findRegionName(origin->latitude(), origin->longitude());
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
double arrivalWeight(const DataModel::Arrival *arr, double defaultWeight) {
	try {
		return arr->weight();
	}
	catch ( Core::ValueException& ) {
		return defaultWeight;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int stationCount(const DataModel::Magnitude *mag) {
	try {
		return mag->stationCount();
	}
	catch ( ... ) {
		return -1;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int modePriority(const DataModel::Origin *origin) {
	try {
		switch ( origin->evaluationMode() ) {
			case AUTOMATIC:
				return 1;
			case MANUAL:
				return 2;
			default:
				break;
		}
	}
	catch ( ValueException & ) {}

	return 0;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int modePriority(const DataModel::FocalMechanism *fm) {
	try {
		switch ( fm->evaluationMode() ) {
			case AUTOMATIC:
				return 1;
			case MANUAL:
				return 2;
			default:
				break;
		}
	}
	catch ( ValueException & ) {}

	return 0;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int priority(const Origin *origin) {
	EvaluationMode mode = AUTOMATIC;
	try {
		mode = origin->evaluationMode();
	}
	catch ( ValueException& ) {}

	switch ( mode ) {
		case MANUAL:

			try {

				switch ( origin->evaluationStatus() ) {
					case PRELIMINARY: return 0;
					case CONFIRMED: return 1;
					case REVIEWED: return 2;
					case FINAL: return 3;
					case REPORTED: return -1;
					case REJECTED: return -100;
					default: break;
				}

			}
			catch ( ValueException& ) {
				return 1;
			}

			break;

		case AUTOMATIC:
		default:

			try {

				switch ( origin->evaluationStatus() ) {
					case PRELIMINARY: return 0;
					case CONFIRMED: return 1;
					case REVIEWED: return 2;
					case FINAL: return 3;
					case REPORTED: return -1;
					case REJECTED: return -100;
					default: break;
				}

			}
			catch ( ValueException& ) {
				return 0;
			}

			break;
	}

	SEISCOMP_WARNING("Origin %s has unknown evaluation mode", origin->publicID().c_str());

	return 0;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int priority(const DataModel::FocalMechanism *fm) {
	EvaluationMode mode = AUTOMATIC;
	try {
		mode = fm->evaluationMode();
	}
	catch ( ValueException& ) {}

	switch ( mode ) {
		case MANUAL:

			try {

				switch ( fm->evaluationStatus() ) {
					case PRELIMINARY: return 0;
					case CONFIRMED: return 1;
					case REVIEWED: return 2;
					case FINAL: return 3;
					case REPORTED: return -1;
					case REJECTED: return -100;
					default: break;
				}

			}
			catch ( ValueException& ) {
				return 1;
			}

			break;

		case AUTOMATIC:
		default:

			try {

				switch ( fm->evaluationStatus() ) {
					case PRELIMINARY: return 0;
					case CONFIRMED: return 1;
					case REVIEWED: return 2;
					case FINAL: return 3;
					case REPORTED: return -1;
					case REJECTED: return -100;
					default: break;
				}

			}
			catch ( ValueException& ) {
				return 0;
			}

			break;
	}

	/*
	EvaluationMode mode = AUTOMATIC;
	try {
		mode = fm->evaluationMode();
	}
	catch ( ValueException& ) {}

	switch ( mode ) {
		case MANUAL:
			return 1;

		case AUTOMATIC:
		default:
			return 0;
	}
	*/

	SEISCOMP_WARNING("FocalMechanism %s has unknown evaluation mode", fm->publicID().c_str());

	return 0;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int definingPhaseCount(const Origin *origin) {
	try {
		return origin->quality().usedPhaseCount();
	}
	catch ( ValueException& ) {
		int n = 0;
		for ( unsigned int i = 0; i < origin->arrivalCount(); ++i ) {
			try {
				if ( origin->arrival(i)->weight() > 0 )
					++n;
			}
			catch ( ValueException& ) {
				++n;
			}
		}

		return n;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
double rms(const DataModel::Origin *origin) {
	try {
		return origin->quality().standardError();
	}
	catch ( ValueException& ) {
		return -1;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
DataModel::EventDescription *
eventRegionDescription(DataModel::Event *ev) {
	return ev->eventDescription(EventDescriptionIndex(REGION_NAME));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
DataModel::EventDescription *
eventFERegionDescription(DataModel::Event *ev) {
	return ev->eventDescription(EventDescriptionIndex(FLINN_ENGDAHL_REGION));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int magnitudePriority(const std::string &magType, const Client::Config &config) {
	int n = config.eventAssociation.magTypes.size();
	for ( auto it = config.eventAssociation.magTypes.begin();
	      it != config.eventAssociation.magTypes.end(); ++it, --n ) {
		if ( magType == *it ) {
			break;
		}
	}

	return n;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int agencyPriority(const std::string &agencyID, const Client::Config &config) {
	int n = config.eventAssociation.agencies.size();
	for ( auto it = config.eventAssociation.agencies.begin();
	      it != config.eventAssociation.agencies.end(); ++it, --n ) {
		if ( agencyID == *it ) {
			break;
		}
	}

	return n;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int authorPriority(const std::string &author, const Client::Config &config) {
	int n = config.eventAssociation.authors.size();
	for ( auto it = config.eventAssociation.authors.begin();
	      it != config.eventAssociation.authors.end(); ++it, --n ) {
		if ( author == *it ) {
			break;
		}
	}

	return n;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int methodPriority(const std::string &methodID, const Client::Config &config) {
	int n = config.eventAssociation.methods.size();
	for ( auto it = config.eventAssociation.methods.begin();
	      it != config.eventAssociation.methods.end(); ++it, --n ) {
		if ( methodID == *it ) {
			break;
		}
	}

	return n;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int goodness(const Magnitude *netmag, int mbcount,
             double mbval, const Client::Config &config) {
	if ( !netmag ) {
		return -1;
	}

	size_t mcount = stationCount(netmag);
	double mval = netmag->magnitude().value();

	if ( mcount < config.eventAssociation.minStationMagnitudes ) {
		return 0;
	}

	// Special Mw(mB) criterion

	if ( isMw(netmag) ) {
		if ( mcount < config.eventAssociation.minMwCount ) {
			return 0;
		}

		if ( mcount < config.eventAssociation.mbOverMwCount ) {
			if ( (mval + mbval) / 2 < config.eventAssociation.mbOverMwValue ) {
				return 0;
			}

			if ( static_cast<int>(mcount) < mbcount / 2 ) {
				return 0;
			}
		}
	}

	return magnitudePriority(netmag->type(), config);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool isMw(const Magnitude *netmag) {
	return (string(netmag->type(), 0, 2) == "Mw");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
}
}
