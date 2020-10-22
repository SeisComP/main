/***************************************************************************
 * Copyright (C) 2017 by gempa GmbH                                        *
 *                                                                         *
 * All Rights Reserved.                                                    *
 *                                                                         *
 * NOTICE: All information contained herein is, and remains                *
 * the property of gempa GmbH and its suppliers, if any. The intellectual  *
 * and technical concepts contained herein are proprietary to gempa GmbH   *
 * and its suppliers.                                                      *
 * Dissemination of this information or reproduction of this material      *
 * is strictly forbidden unless prior written permission is obtained       *
 * from gempa GmbH.                                                        *
 *                                                                         *
 * Author: Enrico Ellguth                                                  *
 * Email: enrico.ellguth@gempa.de                                          *
 ***************************************************************************/

#define SEISCOMP_COMPONENT TEST_SCEVENT_REGIONCHECK
#define SEISCOMP_TEST_MODULE test_scevent_region_check


#include "../regioncheck.h"

#include <seiscomp/unittest/unittests.h>

#include <cstdio>
#include <iostream>

#include <seiscomp3/client/inventory.h>
#include <seiscomp3/datamodel/event.h>
#include <seiscomp3/datamodel/origin.h>
#include <seiscomp3/datamodel/eventparameters.h>
#include <seiscomp3/io/archive/xmlarchive.h>

using namespace std;
using namespace Seiscomp;
using namespace Seiscomp::Core;
using namespace Seiscomp::DataModel;
using namespace Seiscomp::IO;

namespace {

bool readEventParameters(EventParameters &ep,
                         const string &filename) {
	XMLArchive ar;

	if ( !ar.open(filename.c_str()) ) {
		cerr << "failed to open file" << endl;
		return false;
	}

	ar >> NAMED_OBJECT("EventParameters", ep);
	return ar.success();
}

}

BOOST_AUTO_TEST_CASE(CheckOrigin) {
	EventParameters ep;
	BOOST_CHECK(readEventParameters(ep, "data/20201015070038.624597.1087.xml"));
	RegionCheckProcessor proc;
	Seiscomp::Config::Config config;
	config.setString("TODO", "data/regions.bna");

	proc.setup(config);

	// TODO get event from data set
	EventPtr event;
	BOOST_CHECK(proc.process(event.get()));
}
