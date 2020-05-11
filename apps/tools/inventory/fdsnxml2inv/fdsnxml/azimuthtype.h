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


#ifndef SEISCOMP_FDSNXML_AZIMUTHTYPE_H
#define SEISCOMP_FDSNXML_AZIMUTHTYPE_H


#include <fdsnxml/metadata.h>
#include <fdsnxml/floattype.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(AzimuthType);


/**
 * \brief Instrument azimuth, measured from north.
 */
class AzimuthType : public FloatType {
	DECLARE_CASTS(AzimuthType);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		AzimuthType();

		//! Copy constructor
		AzimuthType(const AzimuthType &other);

		//! Destructor
		~AzimuthType();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		AzimuthType& operator=(const AzimuthType &other);
		bool operator==(const AzimuthType &other) const;

};


}
}


#endif
