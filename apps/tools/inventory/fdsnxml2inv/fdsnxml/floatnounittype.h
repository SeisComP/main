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


#ifndef SEISCOMP_FDSNXML_FLOATNOUNITTYPE_H
#define SEISCOMP_FDSNXML_FLOATNOUNITTYPE_H


#include <fdsnxml/metadata.h>
#include <string>
#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(FloatNoUnitType);


/**
 * \brief Representation of floating-point numbers.
 */
class FloatNoUnitType : public Core::BaseObject {
	DECLARE_CASTS(FloatNoUnitType);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		FloatNoUnitType();

		//! Copy constructor
		FloatNoUnitType(const FloatNoUnitType &other);

		//! Custom constructor
		FloatNoUnitType(double value);
		FloatNoUnitType(double value,
		                const OPT(double)& upperUncertainty,
		                const OPT(double)& lowerUncertainty,
		                const std::string& measurementMethod);

		//! Destructor
		~FloatNoUnitType();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		operator double&();
		operator double() const;

		//! Copies the metadata of other to this
		FloatNoUnitType& operator=(const FloatNoUnitType &other);
		bool operator==(const FloatNoUnitType &other) const;


	// ------------------------------------------------------------------
	//  Setters/Getters
	// ------------------------------------------------------------------
	public:
		//! XML tag: value
		void setValue(double value);
		double value() const;

		//! XML tag: plusError
		void setUpperUncertainty(const OPT(double)& upperUncertainty);
		double upperUncertainty() const;

		//! XML tag: minusError
		void setLowerUncertainty(const OPT(double)& lowerUncertainty);
		double lowerUncertainty() const;

		//! XML tag: measurementMethod
		void setMeasurementMethod(const std::string& measurementMethod);
		const std::string& measurementMethod() const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		double _value;
		OPT(double) _upperUncertainty;
		OPT(double) _lowerUncertainty;
		std::string _measurementMethod;
};


}
}


#endif
