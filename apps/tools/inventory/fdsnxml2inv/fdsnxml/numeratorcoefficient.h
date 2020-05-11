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


#ifndef SEISCOMP_FDSNXML_NUMERATORCOEFFICIENT_H
#define SEISCOMP_FDSNXML_NUMERATORCOEFFICIENT_H


#include <fdsnxml/metadata.h>
#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(NumeratorCoefficient);


class NumeratorCoefficient : public Core::BaseObject {
	DECLARE_CASTS(NumeratorCoefficient);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		NumeratorCoefficient();

		//! Copy constructor
		NumeratorCoefficient(const NumeratorCoefficient &other);

		//! Custom constructor
		NumeratorCoefficient(double value);
		NumeratorCoefficient(double value,
		                     const OPT(int)& i);

		//! Destructor
		~NumeratorCoefficient();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		NumeratorCoefficient& operator=(const NumeratorCoefficient &other);
		bool operator==(const NumeratorCoefficient &other) const;


	// ------------------------------------------------------------------
	//  Setters/Getters
	// ------------------------------------------------------------------
	public:
		//! XML tag: value
		void setValue(double value);
		double value() const;

		//! XML tag: i
		void setI(const OPT(int)& i);
		int i() const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		double _value;
		OPT(int) _i;
};


}
}


#endif
