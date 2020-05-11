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


#ifndef SEISCOMP_FDSNXML_OUTPUT_H
#define SEISCOMP_FDSNXML_OUTPUT_H


#include <fdsnxml/metadata.h>
#include <fdsnxml/types.h>
#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(Output);


/**
 * \brief The type of data this channel collects. Corresponds to channel
 * \brief flags in SEED blockette 52. The SEED volume producer could use the
 * \brief first letter of an Output value as the SEED channel flag.
 */
class Output : public Core::BaseObject {
	DECLARE_CASTS(Output);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		Output();

		//! Copy constructor
		Output(const Output &other);

		//! Custom constructor
		Output(OutputType type);

		//! Destructor
		~Output();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		Output& operator=(const Output &other);
		bool operator==(const Output &other) const;


	// ------------------------------------------------------------------
	//  Setters/Getters
	// ------------------------------------------------------------------
	public:
		//! XML tag: type
		void setType(OutputType type);
		OutputType type() const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		OutputType _type;
};


}
}


#endif
