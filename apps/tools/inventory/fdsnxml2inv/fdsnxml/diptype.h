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


#ifndef SEISCOMP_FDSNXML_DIPTYPE_H
#define SEISCOMP_FDSNXML_DIPTYPE_H


#include <fdsnxml/metadata.h>
#include <fdsnxml/floattype.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(DipType);


/**
 * \brief Instrument dip in degrees down from horizontal. Together azimuth
 * \brief and dip describe the direction of the sensitive axis of the
 * \brief instrument.
 */
class DipType : public FloatType {
	DECLARE_CASTS(DipType);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		DipType();

		//! Copy constructor
		DipType(const DipType &other);

		//! Destructor
		~DipType();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		DipType& operator=(const DipType &other);
		bool operator==(const DipType &other) const;

};


}
}


#endif
