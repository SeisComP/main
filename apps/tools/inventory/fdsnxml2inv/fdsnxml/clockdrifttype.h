/***************************************************************************
 * Copyright (C) gempa GmbH                                                *
 * Contact: gempa GmbH (seiscomp-dev@gempa.de)                             *
 *                                                                         *
 * This file may be used under the terms of the GNU Affero                 *
 * Public License version 3.0 as published by the Free Software Foundation *
 * and appearing in the file LICENSE included in the packaging of this     *
 * file. Please review the following information to ensure the GNU Affero  *
 * Public License version 3.0 requirements will be met:                    *
 * https://www.gnu.org/licenses/agpl-3.0.html.                             *
 ***************************************************************************/


#ifndef SEISCOMP_FDSNXML_CLOCKDRIFTTYPE_H
#define SEISCOMP_FDSNXML_CLOCKDRIFTTYPE_H


#include <fdsnxml/metadata.h>
#include <fdsnxml/floattype.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(ClockDriftType);


class ClockDriftType : public FloatType {
	DECLARE_CASTS(ClockDriftType);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		ClockDriftType();

		//! Copy constructor
		ClockDriftType(const ClockDriftType &other);

		//! Destructor
		~ClockDriftType();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		ClockDriftType& operator=(const ClockDriftType &other);
		bool operator==(const ClockDriftType &other) const;

};


}
}


#endif
