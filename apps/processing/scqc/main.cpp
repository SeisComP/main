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


#define SEISCOMP_COMPONENT SCQC
#include <seiscomp/logging/log.h>
#include <seiscomp/system/pluginregistry.h>

#include "qctool.h"


int main(int argc, char **argv) {
	int retCode = EXIT_SUCCESS;

	// Create an own block to make sure the application object
	// is destroyed when printing the overall objectcount
	{
		Seiscomp::Applications::Qc::QcTool app(argc, argv);

		// Search here for QcPlugins.
		app.addPluginPackagePath("qc");

		retCode = app.exec();
	}

	SEISCOMP_DEBUG("EXIT(%d), remaining objects: %d",
	               retCode, Seiscomp::Core::BaseObject::ObjectCount());

	return retCode;
}
