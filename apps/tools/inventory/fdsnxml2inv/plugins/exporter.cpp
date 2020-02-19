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


#include <seiscomp/io/exporter.h>
#include <seiscomp/datamodel/inventory.h>
#include <seiscomp/datamodel/dataavailability.h>
#include <seiscomp/client/application.h>
#include <fdsnxml/xml.h>
#include <fdsnxml/fdsnstationxml.h>
#include "convert2fdsnxml.h"


using namespace Seiscomp;
using namespace Seiscomp::IO;


namespace {


class ExporterFDSNStaXML : public Exporter {
	public:
		ExporterFDSNStaXML() {}

		virtual bool put(std::streambuf* buf, Core::BaseObject *obj) {
			DataModel::Inventory *inv = DataModel::Inventory::Cast(obj);
			if ( inv == NULL ) return false;

			FDSNXML::FDSNStationXML msg;

			if ( SCCoreApp )
				msg.setSender(SCCoreApp->agencyID());

			msg.setCreated(Core::Time::GMT());
			msg.setSource("SeisComP");
			Convert2FDSNStaXML cnv(&msg);
			cnv.push(inv);

			FDSNXML::Exporter out;
			out.setFormattedOutput(_prettyPrint);
			out.setIndent(_indentation);

			return out.write(buf, &msg);
		}

		/**
		 * @brief Writes the given object list to the output stream.
		 *
		 * In contrast to the default XML export implementation, the input
		 * list must hold an inventory. Additionally a DataAvailability object
		 * might be present which is injected into the inventory FDSN
		 * StationXML representation.
		 *
		 * @param buf The output stream buffer
		 * @param objects The object list, in particular Inventory and
		 *                optionally DataAvailability
		 * @return Success flag
		 */
		virtual bool put(std::streambuf* buf, const ExportObjectList &objects) {
			DataModel::Inventory *inv = NULL;
			DataModel::DataAvailability *avail = NULL;
			ExportObjectList::const_iterator it;

			for ( it = objects.begin(); it != objects.end(); ++it ) {
				if ( inv == NULL )
					inv = DataModel::Inventory::Cast(*it);
				if ( avail == NULL )
					avail = DataModel::DataAvailability::Cast(*it);
			}

			if ( inv == NULL ) return false;

			FDSNXML::FDSNStationXML msg;

			if ( SCCoreApp )
				msg.setSender(SCCoreApp->agencyID());

			msg.setCreated(Core::Time::GMT());
			msg.setSource("SeisComP");

			Convert2FDSNStaXML cnv(&msg);
			cnv.setAvailability(avail);
			cnv.push(inv);

			FDSNXML::Exporter out;
			out.setFormattedOutput(_prettyPrint);
			out.setIndent(_indentation);

			return out.write(buf, &msg);
		}

};


REGISTER_EXPORTER_INTERFACE(ExporterFDSNStaXML, "fdsnxml");


}
