/***************************************************************************
 * Copyright (C) GFZ Potsdam                                               *
 * All rights reserved.                                                    *
 *                                                                         *
 * GNU Affero General Public License Usage                                 *
 * This file may be used under the terms of the GNU Affero                 *
 * Public License version 3.0 as published by the Free Software Foundation *
 * and appearing in the file LICENSE included in the packaging of this     *
 * file. Please review the following information to ensure the GNU Affero  *
 * Public License version 3.0 requirements will be met:                    *
 * https://www.gnu.org/licenses/agpl-3.0.html.                             *
 ***************************************************************************/


#include <seiscomp/io/importer.h>
#include <seiscomp/io/archive/xmlarchive.h>
#include <seiscomp/datamodel/inventory.h>
#include <fdsnxml/xml.h>
#include <fdsnxml/fdsnstationxml.h>
#include "convert2sc.h"


using namespace Seiscomp;
using namespace Seiscomp::IO;


namespace {


class ImporterFDSNStaXML : public Importer {
	public:
		ImporterFDSNStaXML() {}

		virtual Core::BaseObject *get(std::streambuf* buf) {
			FDSNXML::Importer imp;
			Core::BaseObjectPtr obj = imp.read(buf);

			// Nothing has been read
			if ( !obj ) return NULL;

			FDSNXML::FDSNStationXMLPtr msg = FDSNXML::FDSNStationXML::Cast(obj);
			// It is not a FDSNXML message
			if ( !msg ) return NULL;

			DataModel::Inventory *inv = new DataModel::Inventory;

			Convert2SC cnv(inv);
			cnv.push(msg.get());

			// Clean up the inventory after pushing all messages
			cnv.cleanUp();

			return inv;
		}
};


REGISTER_IMPORTER_INTERFACE(ImporterFDSNStaXML, "fdsnxml");


}
