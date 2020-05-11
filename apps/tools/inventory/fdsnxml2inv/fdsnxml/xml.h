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


#ifndef SEISCOMP_FDSNXML_XML_H
#define SEISCOMP_FDSNXML_XML_H


#include <seiscomp/io/xml/importer.h>
#include <seiscomp/io/xml/exporter.h>


namespace Seiscomp {
namespace FDSNXML {


class Importer : public Seiscomp::IO::XML::Importer {
	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! C'tor
		Importer();
};


class Exporter : public Seiscomp::IO::XML::Exporter {
	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! C'tor
		Exporter();
};


}
}


#endif
