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


#ifndef SEISCOMP_FDSNXML_FLOATTYPE_H
#define SEISCOMP_FDSNXML_FLOATTYPE_H


#include <fdsnxml/metadata.h>
#include <fdsnxml/floatnounittype.h>
#include <string>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(FloatType);


/**
 * \brief Representation of floating-point numbers used as measurements.
 */
class FloatType : public FloatNoUnitType {
	DECLARE_CASTS(FloatType);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		FloatType();

		//! Copy constructor
		FloatType(const FloatType &other);

		//! Destructor
		~FloatType();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		FloatType& operator=(const FloatType &other);
		bool operator==(const FloatType &other) const;


	// ------------------------------------------------------------------
	//  Setters/Getters
	// ------------------------------------------------------------------
	public:
		//! XML tag: unit
		void setUnit(const std::string& unit);
		const std::string& unit() const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		std::string _unit;
};


}
}


#endif
