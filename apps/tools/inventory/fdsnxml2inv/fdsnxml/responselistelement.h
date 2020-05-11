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


#ifndef SEISCOMP_FDSNXML_RESPONSELISTELEMENT_H
#define SEISCOMP_FDSNXML_RESPONSELISTELEMENT_H


#include <fdsnxml/metadata.h>
#include <fdsnxml/floattype.h>
#include <fdsnxml/frequencytype.h>
#include <fdsnxml/angletype.h>
#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(ResponseListElement);


class ResponseListElement : public Core::BaseObject {
	DECLARE_CASTS(ResponseListElement);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		ResponseListElement();

		//! Copy constructor
		ResponseListElement(const ResponseListElement &other);

		//! Destructor
		~ResponseListElement();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		ResponseListElement& operator=(const ResponseListElement &other);
		bool operator==(const ResponseListElement &other) const;


	// ------------------------------------------------------------------
	//  Setters/Getters
	// ------------------------------------------------------------------
	public:
		//! XML tag: Frequency
		void setFrequency(const FrequencyType& frequency);
		FrequencyType& frequency();
		const FrequencyType& frequency() const;

		//! XML tag: Amplitude
		void setAmplitude(const FloatType& amplitude);
		FloatType& amplitude();
		const FloatType& amplitude() const;

		//! XML tag: Phase
		void setPhase(const AngleType& phase);
		AngleType& phase();
		const AngleType& phase() const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		FrequencyType _frequency;
		FloatType _amplitude;
		AngleType _phase;
};


}
}


#endif
