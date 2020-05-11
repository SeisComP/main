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


#ifndef SEISCOMP_FDSNXML_FLOATNOUNITWITHNUMBERTYPE_H
#define SEISCOMP_FDSNXML_FLOATNOUNITWITHNUMBERTYPE_H


#include <fdsnxml/metadata.h>
#include <fdsnxml/floatnounittype.h>
#include <fdsnxml/countertype.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(FloatNoUnitWithNumberType);


/**
 * \brief Representation of floating-point numbers with index numbers.
 */
class FloatNoUnitWithNumberType : public FloatNoUnitType {
	DECLARE_CASTS(FloatNoUnitWithNumberType);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		FloatNoUnitWithNumberType();

		//! Copy constructor
		FloatNoUnitWithNumberType(const FloatNoUnitWithNumberType &other);

		//! Destructor
		~FloatNoUnitWithNumberType();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		FloatNoUnitWithNumberType& operator=(const FloatNoUnitWithNumberType &other);
		bool operator==(const FloatNoUnitWithNumberType &other) const;


	// ------------------------------------------------------------------
	//  Setters/Getters
	// ------------------------------------------------------------------
	public:
		//! XML tag: number
		void setNumber(const OPT(CounterType)& number);
		CounterType& number();
		const CounterType& number() const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		OPT(CounterType) _number;
};


}
}


#endif
