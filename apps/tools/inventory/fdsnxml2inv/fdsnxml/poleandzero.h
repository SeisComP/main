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


#ifndef SEISCOMP_FDSNXML_POLEANDZERO_H
#define SEISCOMP_FDSNXML_POLEANDZERO_H


#include <fdsnxml/metadata.h>
#include <fdsnxml/floatnounittype.h>
#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(PoleAndZero);


/**
 * \brief Complex numbers used as poles or zeros in channel response.
 */
class PoleAndZero : public Core::BaseObject {
	DECLARE_CASTS(PoleAndZero);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		PoleAndZero();

		//! Copy constructor
		PoleAndZero(const PoleAndZero &other);

		//! Destructor
		~PoleAndZero();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		PoleAndZero& operator=(const PoleAndZero &other);
		bool operator==(const PoleAndZero &other) const;


	// ------------------------------------------------------------------
	//  Setters/Getters
	// ------------------------------------------------------------------
	public:
		//! XML tag: Real
		void setReal(const FloatNoUnitType& real);
		FloatNoUnitType& real();
		const FloatNoUnitType& real() const;

		//! XML tag: Imaginary
		void setImaginary(const FloatNoUnitType& imaginary);
		FloatNoUnitType& imaginary();
		const FloatNoUnitType& imaginary() const;

		//! XML tag: number
		void setNumber(int number);
		int number() const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		FloatNoUnitType _real;
		FloatNoUnitType _imaginary;
		int _number;
};


}
}


#endif
