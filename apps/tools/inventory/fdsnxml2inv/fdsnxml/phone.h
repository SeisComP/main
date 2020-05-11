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


#ifndef SEISCOMP_FDSNXML_PHONE_H
#define SEISCOMP_FDSNXML_PHONE_H


#include <fdsnxml/metadata.h>
#include <string>
#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(Phone);


class Phone : public Core::BaseObject {
	DECLARE_CASTS(Phone);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		Phone();

		//! Copy constructor
		Phone(const Phone &other);

		//! Destructor
		~Phone();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		Phone& operator=(const Phone &other);
		bool operator==(const Phone &other) const;


	// ------------------------------------------------------------------
	//  Setters/Getters
	// ------------------------------------------------------------------
	public:
		//! XML tag: CountryCode
		void setCountryCode(const OPT(int)& countryCode);
		int countryCode() const;

		//! XML tag: AreaCode
		void setAreaCode(int areaCode);
		int areaCode() const;

		//! XML tag: PhoneNumber
		void setPhoneNumber(const std::string& phoneNumber);
		const std::string& phoneNumber() const;

		//! XML tag: description
		void setDescription(const std::string& description);
		const std::string& description() const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		OPT(int) _countryCode;
		int _areaCode;
		std::string _phoneNumber;
		std::string _description;
};


}
}


#endif
