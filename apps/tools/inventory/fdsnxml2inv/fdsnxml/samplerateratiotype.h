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


#ifndef SEISCOMP_FDSNXML_SAMPLERATERATIOTYPE_H
#define SEISCOMP_FDSNXML_SAMPLERATERATIOTYPE_H


#include <fdsnxml/metadata.h>
#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(SampleRateRatioType);


/**
 * \brief Sample rate expressed as number of samples in a number of seconds.
 */
class SampleRateRatioType : public Core::BaseObject {
	DECLARE_CASTS(SampleRateRatioType);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		SampleRateRatioType();

		//! Copy constructor
		SampleRateRatioType(const SampleRateRatioType &other);

		//! Destructor
		~SampleRateRatioType();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		SampleRateRatioType& operator=(const SampleRateRatioType &other);
		bool operator==(const SampleRateRatioType &other) const;


	// ------------------------------------------------------------------
	//  Setters/Getters
	// ------------------------------------------------------------------
	public:
		//! XML tag: NumberSamples
		void setNumberSamples(int numberSamples);
		int numberSamples() const;

		//! XML tag: NumberSeconds
		void setNumberSeconds(int numberSeconds);
		int numberSeconds() const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		int _numberSamples;
		int _numberSeconds;
};


}
}


#endif
