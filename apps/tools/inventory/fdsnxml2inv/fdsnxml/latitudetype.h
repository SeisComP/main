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


#ifndef SEISCOMP_FDSNXML_LATITUDETYPE_H
#define SEISCOMP_FDSNXML_LATITUDETYPE_H


#include <fdsnxml/metadata.h>
#include <string>
#include <fdsnxml/floattype.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(LatitudeType);


/**
 * \brief Type for latitude coordinates.
 */
class LatitudeType : public FloatType {
	DECLARE_CASTS(LatitudeType);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		LatitudeType();

		//! Copy constructor
		LatitudeType(const LatitudeType &other);

		//! Destructor
		~LatitudeType();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		LatitudeType& operator=(const LatitudeType &other);
		bool operator==(const LatitudeType &other) const;


	// ------------------------------------------------------------------
	//  Setters/Getters
	// ------------------------------------------------------------------
	public:
		//! XML tag: datum
		void setDatum(const std::string& datum);
		const std::string& datum() const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		std::string _datum;
};


}
}


#endif
