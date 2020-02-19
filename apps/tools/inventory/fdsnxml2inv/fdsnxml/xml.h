/***************************************************************************
 *   Copyright (C) 2013 by gempa GmbH
 *
 *   Author: Jan Becker
 *   Email: jabe@gempa.de $
 *
 ***************************************************************************/


#ifndef SEISCOMP_FDSNXML_XML_H__
#define SEISCOMP_FDSNXML_XML_H__


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
