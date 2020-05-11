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


#ifndef SEISCOMP_FDSNXML_DECIMATION_H
#define SEISCOMP_FDSNXML_DECIMATION_H


#include <fdsnxml/metadata.h>
#include <fdsnxml/floattype.h>
#include <fdsnxml/frequencytype.h>
#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(Decimation);


/**
 * \brief Corresponds to SEED blockette 57.
 */
class Decimation : public Core::BaseObject {
	DECLARE_CASTS(Decimation);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		Decimation();

		//! Copy constructor
		Decimation(const Decimation &other);

		//! Destructor
		~Decimation();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		Decimation& operator=(const Decimation &other);
		bool operator==(const Decimation &other) const;


	// ------------------------------------------------------------------
	//  Setters/Getters
	// ------------------------------------------------------------------
	public:
		//! XML tag: InputSampleRate
		void setInputSampleRate(const FrequencyType& inputSampleRate);
		FrequencyType& inputSampleRate();
		const FrequencyType& inputSampleRate() const;

		//! XML tag: Factor
		void setFactor(int factor);
		int factor() const;

		//! XML tag: Offset
		void setOffset(int offset);
		int offset() const;

		//! XML tag: Delay
		void setDelay(const FloatType& delay);
		FloatType& delay();
		const FloatType& delay() const;

		//! XML tag: Correction
		void setCorrection(const FloatType& correction);
		FloatType& correction();
		const FloatType& correction() const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		FrequencyType _inputSampleRate;
		int _factor;
		int _offset;
		FloatType _delay;
		FloatType _correction;
};


}
}


#endif
