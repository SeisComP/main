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


#ifndef SEISCOMP_FDSNXML_EXTERNALREFERENCE_H
#define SEISCOMP_FDSNXML_EXTERNALREFERENCE_H


#include <fdsnxml/metadata.h>
#include <string>
#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/exceptions.h>


namespace Seiscomp {
namespace FDSNXML {


DEFINE_SMARTPOINTER(ExternalReference);


/**
 * \brief This type contains a URI and description for external data that
 * \brief users may want to reference in StationXML.
 */
class ExternalReference : public Core::BaseObject {
	DECLARE_CASTS(ExternalReference);
	DECLARE_RTTI;
	DECLARE_METAOBJECT_DERIVED;

	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! Constructor
		ExternalReference();

		//! Copy constructor
		ExternalReference(const ExternalReference &other);

		//! Destructor
		~ExternalReference();


	// ------------------------------------------------------------------
	//  Operators
	// ------------------------------------------------------------------
	public:
		//! Copies the metadata of other to this
		ExternalReference& operator=(const ExternalReference &other);
		bool operator==(const ExternalReference &other) const;


	// ------------------------------------------------------------------
	//  Setters/Getters
	// ------------------------------------------------------------------
	public:
		//! XML tag: URI
		void setURI(const std::string& uRI);
		const std::string& uRI() const;

		//! XML tag: Description
		void setDescription(const std::string& description);
		const std::string& description() const;


	// ------------------------------------------------------------------
	//  Implementation
	// ------------------------------------------------------------------
	private:
		// Attributes
		std::string _uRI;
		std::string _description;
};


}
}


#endif
