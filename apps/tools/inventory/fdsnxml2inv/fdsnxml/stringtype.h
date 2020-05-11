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


#ifndef SEISCOMP_FDSNXML_STRINGTYPE_H
#define SEISCOMP_FDSNXML_STRINGTYPE_H


#include <fdsnxml/metadata.h>
#include <string>
#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(StringType);


class StringType : public Core::BaseObject {
	DECLARE_CASTS(StringType);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		StringType();

		//! Copy constructor
		StringType(const StringType &other);

		//! Custom constructor
		StringType(const std::string& text);

		//! Destructor
		~StringType();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		operator std::string&();
		operator const std::string&() const;

		//! Copies the metadata of other to this
		StringType& operator=(const StringType &other);
		bool operator==(const StringType &other) const;


	// ------------------------------------------------------------------
	//  Setters/Getters
	// ------------------------------------------------------------------
	public:
		//! XML tag: text
		void setText(const std::string& text);
		const std::string& text() const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		std::string _text;
};


}
}


#endif
