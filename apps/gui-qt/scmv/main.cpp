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


#define SEISCOMP_COMPONENT MapView
#include <seiscomp/logging/log.h>
#include <seiscomp/gui/core/application.h>

#include "mvmainwindow.h"

using namespace Seiscomp;

class MvKicker : public Gui::Kicker<MvMainWindow> {
	public:
		MvKicker(int& argc, char **argv, int flags = DEFAULT)
		 : Gui::Kicker<MvMainWindow>(argc, argv, flags) {
			setLoadRegionsEnabled(true);
		}

	protected:
		void printUsage() const {
			std::cout << "Usage:" << std::endl
			          << "  " << name() << " [options]"
			          << std::endl << std::endl
			          << "View stations and events on a map."
			          << std::endl;

			Seiscomp::Gui::Application::printUsage();

			std::cout << "Examples:" << std::endl;
			std::cout << "Execute scmv printing debug output on command line"
			          << std::endl
			          << "  scmv --debug"
			          << std::endl << std::endl;

			std::cout << "Execute scmv in full-screen mode"
			          << std::endl
			          << "  scmv -F"
			          << std::endl << std::endl;
		}

		virtual void createCommandLineDescription() {
			Gui::Kicker<MvMainWindow>::createCommandLineDescription();

			commandline().addGroup("Mapview");
			commandline().addOption("Mapview",
			                        "displaymode",
			                        "Start mapview as walldisplay. Modes: groundmotion, qualitycontrol",
			                        (std::string*)0,
			                        false);
			commandline().addOption("Mapview",
			                        "with-legend",
			                        "Shows the map legend if started as walldisplay");
		}

		virtual bool initUi(MvMainWindow* mvw) {
			return mvw->init();
		}

};




int main(int argc, char* argv[]) {
	int retCode;

	{
		Client::Application::HandleSignals(false, false);
		MvKicker app(argc, argv,
			Gui::Application::DEFAULT |
			Gui::Application::LOAD_STATIONS |
			Gui::Application::LOAD_CONFIGMODULE);
		app.setPrimaryMessagingGroup("GUI");
		app.addMessagingSubscription("AMPLITUDE");
		app.addMessagingSubscription("PICK");
		app.addMessagingSubscription("EVENT");
		app.addMessagingSubscription("LOCATION");
		app.addMessagingSubscription("MAGNITUDE");
		app.addMessagingSubscription("QC");
		app.addMessagingSubscription("CONFIG");
		app.addPluginPackagePath("qc");
		retCode = app();
		SEISCOMP_DEBUG("Number of remaining objects before destroying application: %d", Seiscomp::Core::BaseObject::ObjectCount());
	}

	SEISCOMP_DEBUG("Number of remaining objects after destroying application: %d", Seiscomp::Core::BaseObject::ObjectCount());
	return retCode;
}
