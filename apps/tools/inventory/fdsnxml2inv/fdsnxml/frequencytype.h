/***************************************************************************
 *   Copyright (C) 2013 by gempa GmbH
 *
 *   Author: Jan Becker
 *   Email: jabe@gempa.de $
 *
 ***************************************************************************/


#ifndef SEISCOMP_FDSNXML_FREQUENCYTYPE_H__
#define SEISCOMP_FDSNXML_FREQUENCYTYPE_H__


#include <fdsnxml/metadata.h>
#include <fdsnxml/floattype.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(FrequencyType);


class FrequencyType : public FloatType {
	DECLARE_CASTS(FrequencyType);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		FrequencyType();

		//! Copy constructor
		FrequencyType(const FrequencyType &other);

		//! Destructor
		~FrequencyType();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		FrequencyType& operator=(const FrequencyType &other);
		bool operator==(const FrequencyType &other) const;

};


}
}


#endif
