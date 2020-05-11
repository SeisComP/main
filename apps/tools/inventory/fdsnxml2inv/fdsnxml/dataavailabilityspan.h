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


#ifndef SEISCOMP_FDSNXML_DATAAVAILABILITYSPAN_H
#define SEISCOMP_FDSNXML_DATAAVAILABILITYSPAN_H


#include <fdsnxml/metadata.h>
#include <fdsnxml/date.h>
#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(DataAvailabilitySpan);


/**
 * \brief A type for describing data availability spans, with variable
 * \brief continuity. The time range described may be based on the request
 * \brief parameters that generated the document and not necessarily relate
 * \brief to continuity outside of the range. It may also be a smaller time
 * \brief window than the request depending on the data characteristics.
 */
class DataAvailabilitySpan : public Core::BaseObject {
	DECLARE_CASTS(DataAvailabilitySpan);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		DataAvailabilitySpan();

		//! Copy constructor
		DataAvailabilitySpan(const DataAvailabilitySpan &other);

		//! Destructor
		~DataAvailabilitySpan();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		DataAvailabilitySpan& operator=(const DataAvailabilitySpan &other);
		bool operator==(const DataAvailabilitySpan &other) const;


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

		//! The number of continuous time series segments contained in the
		//! specified time range. A value of 1 indicates that the time series is
		//! continuous from start to end.
		//! XML tag: numberSegments
		void setNumberSegments(int numberSegments);
		int numberSegments() const;

		//! The maximum time tear (gap or overlap) in seconds between time series
		//! segments in the specified range.
		//! XML tag: maximumTimeTear
		void setMaximumTimeTear(const OPT(double)& maximumTimeTear);
		double maximumTimeTear() const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		DateTime _start;
		DateTime _end;
		int _numberSegments;
		OPT(double) _maximumTimeTear;
};


}
}


#endif
