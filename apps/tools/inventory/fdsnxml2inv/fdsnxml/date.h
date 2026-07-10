/***************************************************************************
 *   Copyright (C) 2013 by gempa GmbH
 *
 *   Author: Jan Becker
 *   Email: jabe@gempa.de $
 *
 ***************************************************************************/

#ifndef SEISCOMP_FDSNXML_DATE_H__
#define SEISCOMP_FDSNXML_DATE_H__


#include <seiscomp/core/datetime.h>
#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/strings.h>
#include <cstdlib>


namespace Seiscomp {
namespace FDSNXML {


class DateTime : public Core::Time {
	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		DateTime();

		//! Copy constructor
		DateTime(const Core::Time &other);
		DateTime(const DateTime &other);

		void serialize(Core::BaseObject::Archive &ar);
};


inline bool fromString(DateTime &date, const std::string &str) {
	int year = strtol(str.c_str(), nullptr, 10);
	if ( year < -32768 ) {
		return false;
	}
	else if ( year >= 2500 ) {
		return false;
	}

	return Core::fromString(static_cast<Core::Time&>(date), str);
}


inline std::string toString(const DateTime &date) {
	if ( date.microseconds() == 0 ) {
		return date.toString("%FT%TZ");
	}
	else {
		return date.toString("%FT%T.%fZ");
	}
}


}
}


#endif
