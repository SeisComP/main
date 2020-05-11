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


#ifndef SEISCOMP_FDSNXML_COEFFICIENTS_H
#define SEISCOMP_FDSNXML_COEFFICIENTS_H


#include <fdsnxml/metadata.h>
#include <vector>
#include <fdsnxml/basefilter.h>
#include <fdsnxml/types.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {

DEFINE_SMARTPOINTER(FloatNoUnitWithNumberType);
DEFINE_SMARTPOINTER(FloatNoUnitWithNumberType);



DEFINE_SMARTPOINTER(Coefficients);


/**
 * \brief Response: coefficients for FIR filter. Laplace transforms or IIR
 * \brief filters can be expressed using type as well but the
 * \brief PolesAndZerosType should be used instead. Corresponds to SEED
 * \brief blockette 54.
 */
class Coefficients : public BaseFilter {
	DECLARE_CASTS(Coefficients);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		Coefficients();

		//! Copy constructor
		Coefficients(const Coefficients &other);

		//! Destructor
		~Coefficients();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		Coefficients& operator=(const Coefficients &other);
		bool operator==(const Coefficients &other) const;


	// ------------------------------------------------------------------
	//  Setters/Getters
	// ------------------------------------------------------------------
	public:
		//! XML tag: CfTransferFunctionType
		void setCfTransferFunctionType(CfTransferFunctionType cfTransferFunctionType);
		CfTransferFunctionType cfTransferFunctionType() const;

	
	// ------------------------------------------------------------------
	//  Public interface
	// ------------------------------------------------------------------
	public:
		/**
		 * Add an object.
		 * @param obj The object pointer
		 * @return true The object has been added
		 * @return false The object has not been added
		 *               because it already exists in the list
		 *               or it already has another parent
		 */
		bool addNumerator(FloatNoUnitWithNumberType *obj);
		bool addDenominator(FloatNoUnitWithNumberType *obj);

		/**
		 * Removes an object.
		 * @param obj The object pointer
		 * @return true The object has been removed
		 * @return false The object has not been removed
		 *               because it does not exist in the list
		 */
		bool removeNumerator(FloatNoUnitWithNumberType *obj);
		bool removeDenominator(FloatNoUnitWithNumberType *obj);

		/**
		 * Removes an object of a particular class.
		 * @param i The object index
		 * @return true The object has been removed
		 * @return false The index is out of bounds
		 */
		bool removeNumerator(size_t i);
		bool removeDenominator(size_t i);

		//! Retrieve the number of objects of a particular class
		size_t numeratorCount() const;
		size_t denominatorCount() const;

		//! Index access
		//! @return The object at index i
		FloatNoUnitWithNumberType* numerator(size_t i) const;
		FloatNoUnitWithNumberType* denominator(size_t i) const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		CfTransferFunctionType _cfTransferFunctionType;

		// Aggregations
		std::vector<FloatNoUnitWithNumberTypePtr> _numerators;
		std::vector<FloatNoUnitWithNumberTypePtr> _denominators;
};


}
}


#endif
