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


#ifndef SEISCOMP_FDSNXML_FIR_H
#define SEISCOMP_FDSNXML_FIR_H


#include <fdsnxml/metadata.h>
#include <vector>
#include <fdsnxml/basefilter.h>
#include <fdsnxml/types.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {

DEFINE_SMARTPOINTER(NumeratorCoefficient);



DEFINE_SMARTPOINTER(FIR);


/**
 * \brief Response: FIR filter. Corresponds to SEED blockette 61. FIR filters
 * \brief are also commonly documented using the CoefficientsType element.
 */
class FIR : public BaseFilter {
	DECLARE_CASTS(FIR);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		FIR();

		//! Copy constructor
		FIR(const FIR &other);

		//! Destructor
		~FIR();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		FIR& operator=(const FIR &other);
		bool operator==(const FIR &other) const;


	// ------------------------------------------------------------------
	//  Setters/Getters
	// ------------------------------------------------------------------
	public:
		//! XML tag: Symmetry
		void setSymmetry(SymmetryType symmetry);
		SymmetryType symmetry() const;

	
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
		bool addNumeratorCoefficient(NumeratorCoefficient *obj);

		/**
		 * Removes an object.
		 * @param obj The object pointer
		 * @return true The object has been removed
		 * @return false The object has not been removed
		 *               because it does not exist in the list
		 */
		bool removeNumeratorCoefficient(NumeratorCoefficient *obj);

		/**
		 * Removes an object of a particular class.
		 * @param i The object index
		 * @return true The object has been removed
		 * @return false The index is out of bounds
		 */
		bool removeNumeratorCoefficient(size_t i);

		//! Retrieve the number of objects of a particular class
		size_t numeratorCoefficientCount() const;

		//! Index access
		//! @return The object at index i
		NumeratorCoefficient* numeratorCoefficient(size_t i) const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		SymmetryType _symmetry;

		// Aggregations
		std::vector<NumeratorCoefficientPtr> _numeratorCoefficients;
};


}
}


#endif
