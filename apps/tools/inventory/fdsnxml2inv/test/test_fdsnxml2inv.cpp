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

#define SEISCOMP_COMPONENT TEST_FDSNXML2INV
#define SEISCOMP_TEST_MODULE SeisComP

#include <seiscomp/client/inventory.h>
#include <seiscomp/io/archive/xmlarchive.h>
#include <seiscomp/datamodel/publicobject.h>
#include <seiscomp/unittest/unittests.h>

#include <iostream>

#include <boost/regex.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

#include "../fdsnxml/xml.h"
#include "../fdsnxml/fdsnstationxml.h"
#include "../convert2fdsnxml.h"
#include "../convert2sc.h"

using namespace std;
using namespace Seiscomp;

namespace {

BOOST_AUTO_TEST_SUITE(seiscomp_main_fdsnxml2inv)

BOOST_AUTO_TEST_CASE(CheckConvert2FDSNXML) {
	DataModel::InventoryPtr inv;

	IO::XMLArchive ar;
	BOOST_REQUIRE(ar.open("data/sc.xml"));
	ar >> inv;
	BOOST_REQUIRE(inv);

	FDSNXML::FDSNStationXML msg;
	Convert2FDSNStaXML cnv(&msg);
	cnv.push(inv.get());

	FDSNXML::Exporter out;
	out.setFormattedOutput(true);

	string result;

	{
		boost::iostreams::stream_buffer<boost::iostreams::back_insert_device<string> > buf(result);
		out.write(&buf, &msg);
	}

	ifstream ifs("data/fdsn.xml");
	ostringstream oss;
	oss << ifs.rdbuf();
	string expected = oss.str();

	BOOST_CHECK_EQUAL(result, expected);
}

BOOST_AUTO_TEST_CASE(CheckConvert2SCXML) {
	DataModel::InventoryPtr inv = new DataModel::Inventory;

	FDSNXML::Importer imp;
	Core::BaseObjectPtr obj = imp.read("data/fdsn.xml");
	BOOST_REQUIRE(obj);

	FDSNXML::FDSNStationXMLPtr msg = FDSNXML::FDSNStationXML::Cast(obj);
	BOOST_REQUIRE(msg);

	Convert2SC cnv(inv.get());
	cnv.push(msg.get());
	cnv.cleanUp();

	string result;

	{
		boost::iostreams::stream_buffer<boost::iostreams::back_insert_device<string> > buf(result);
		IO::XMLArchive ar;
		BOOST_REQUIRE(ar.create(&buf));

		ar.setFormattedOutput(true);
		ar << inv;
		BOOST_REQUIRE(ar.success());
	}

	ifstream ifs("data/sc.xml");
	ostringstream oss;
	oss << ifs.rdbuf();
	string expected = oss.str();

	// remove dynamically generated publicIDs
	boost::regex rx("publicID=\"[^\"]*\"|sensor=\"[^\"]*\"|datalogger=\"[^\"]*\"");
	BOOST_CHECK_EQUAL(boost::regex_replace(result, rx, string("")), boost::regex_replace(expected, rx, string("")));
}

BOOST_AUTO_TEST_SUITE_END()

}

