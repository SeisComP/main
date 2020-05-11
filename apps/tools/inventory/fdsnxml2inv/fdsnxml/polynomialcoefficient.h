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


#ifndef SEISCOMP_FDSNXML_POLYNOMIALCOEFFICIENT_H
#define SEISCOMP_FDSNXML_POLYNOMIALCOEFFICIENT_H


#include <fdsnxml/metadata.h>
#include <fdsnxml/floatnounittype.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(PolynomialCoefficient);


class PolynomialCoefficient : public FloatNoUnitType {
	DECLARE_CASTS(PolynomialCoefficient);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		PolynomialCoefficient();

		//! Copy constructor
		PolynomialCoefficient(const PolynomialCoefficient &other);

		//! Destructor
		~PolynomialCoefficient();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		PolynomialCoefficient& operator=(const PolynomialCoefficient &other);
		bool operator==(const PolynomialCoefficient &other) const;


	// ------------------------------------------------------------------
	//  Setters/Getters
	// ------------------------------------------------------------------
	public:
		//! XML tag: number
		void setNumber(int number);
		int number() const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		int _number;
};


}
}


#endif
