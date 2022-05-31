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


#define SEISCOMP_COMPONENT QcView

#include <seiscomp/gui/core/application.h>
#include <seiscomp/logging/log.h>
#include "mainframe.h"

typedef Seiscomp::Gui::MainFrame MainWindow;
typedef Seiscomp::Gui::Kicker<MainWindow> Kicker;

class QCVApp : public Kicker {
	public:
		QCVApp(int& argc, char** argv, int flags = DEFAULT) :
			Kicker(argc, argv, flags) {
		}

	protected:
		void printUsage() const {
			std::cout << "Usage:" << std::endl
			          << "  " << name() << " [options]"
			          << std::endl << std::endl
			          << "View QC information per stream or station."
			          << std::endl;

			Seiscomp::Gui::Application::printUsage();

			std::cout << "Examples:" << std::endl;
			std::cout << "Execute scqcv printing debug output on command line"
			          << std::endl
			          << "  scqcv --debug"
			          << std::endl << std::endl;

			std::cout << "Execute scqcv in full-screen mode"
			          << std::endl
			          << "  scqcv -F"
			          << std::endl << std::endl;
		}
};

int main(int argc, char** argv) {
	int retCode;

	{
		QCVApp app(argc, argv, Seiscomp::Gui::Application::DEFAULT|Seiscomp::Gui::Application::LOAD_INVENTORY|Seiscomp::Gui::Application::LOAD_CONFIGMODULE);

		app.setPrimaryMessagingGroup("GUI");
		app.addMessagingSubscription("QC");

		retCode = app();

		SEISCOMP_DEBUG("Number of remaining objects before destroying application: %d",
		               Seiscomp::Core::BaseObject::ObjectCount());
	}

	SEISCOMP_DEBUG("Number of remaining objects after destroying application: %d",
	               Seiscomp::Core::BaseObject::ObjectCount());
	return retCode;
}
