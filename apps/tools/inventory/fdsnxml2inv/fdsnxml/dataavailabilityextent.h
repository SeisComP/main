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


#ifndef SEISCOMP_FDSNXML_DATAAVAILABILITYEXTENT_H
#define SEISCOMP_FDSNXML_DATAAVAILABILITYEXTENT_H


#include <fdsnxml/metadata.h>
#include <fdsnxml/date.h>
#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(DataAvailabilityExtent);


/**
 * \brief A type for describing data availability extents, the earliest and
 * \brief latest data available. No information is included about the
 * \brief continuity of the data is included or implied.
 */
class DataAvailabilityExtent : public Core::BaseObject {
	DECLARE_CASTS(DataAvailabilityExtent);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		DataAvailabilityExtent();

		//! Copy constructor
		DataAvailabilityExtent(const DataAvailabilityExtent &other);

		//! Destructor
		~DataAvailabilityExtent();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		DataAvailabilityExtent& operator=(const DataAvailabilityExtent &other);
		bool operator==(const DataAvailabilityExtent &other) const;


	// ------------------------------------------------------------------
	//  Setters/Getters
	// ------------------------------------------------------------------
	public:
		//! XML tag: start
		void setStart(DateTime start);
		DateTime start() const;

		//! XML tag: end
		void setEnd(DateTime end);
		DateTime end() const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		DateTime _start;
		DateTime _end;
};


}
}


#endif
