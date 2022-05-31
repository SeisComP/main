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




#define SEISCOMP_COMPONENT EventSummaryView

#include "mainframe.h"
#include <seiscomp/gui/core/application.h>
#include <seiscomp/gui/datamodel/eventsummaryview.h>
#include <seiscomp/logging/log.h>

typedef Seiscomp::Applications::EventSummaryView::MainFrame MainWindow;
typedef Seiscomp::Gui::Kicker<MainWindow> Kicker;

class ESVApp : public Kicker {
	public:
		ESVApp(int& argc, char** argv, int flags = DEFAULT) :
		  Kicker(argc, argv, flags) {
			setRecordStreamEnabled(false);
			setLoadRegionsEnabled(true);
			_preloadDays = 1;
		}


	protected:
		void printUsage() const {
			std::cout << "Usage:" << std::endl
			          << "  " << name() << " [options]"
			          << std::endl << std::endl
			          << "View summary information for last or any loaded event."
			          << std::endl;

			Seiscomp::Gui::Application::printUsage();

			std::cout << "Examples:" << std::endl;
			std::cout << "Execute scesv printing debug output on command line"
			          << std::endl
			          << "  scesv --debug"
			          << std::endl << std::endl;

			std::cout << "Execute scesv in full-screen mode"
			          << std::endl
			          << "  scesv -F"
			          << std::endl << std::endl;
		}

		void createCommandLineDescription() {
			Kicker::createCommandLineDescription();

			commandline().addOption(
				"Generic", "script0",
				"path to the script called when configurable button0 is pressed; "
				"EventID, arrival count, magnitude and the additional location information string are passed as parameters $1, $2, $3 and $4",
				&_script0);

			commandline().addOption(
				"Generic", "script1",
				"path to the script called when configurable button1 is pressed; "
				"EventID, arrival count, magnitude and the additional location information string are passed as parameters $1, $2, $3 and $4",
				&_script1);

			commandline().addOption(
				"Generic", "load-event-db",
				"Number of days to load from database",
				&_preloadDays);
		}

		bool initConfiguration() {
			if ( !Kicker::initConfiguration() ) return false;

			try { _preloadDays = configGetDouble("loadEventDB"); }
			catch ( ... ) {}

			try {
				_script0 = Seiscomp::Environment::Instance()->absolutePath(configGetString("scripts.script0"));
			}
			catch ( ... ) {}

			try {
				_scriptStyle0 = configGetBool("scripts.script0.oldStyle");
			}
			catch ( ... ) {
				_scriptStyle0 = true;
			}

			try {
				_scriptExportMap0 = configGetBool("scripts.script0.exportMap");
			}
			catch ( ... ) {
				_scriptExportMap0 = false;
			}

			try {
				_script1 = Seiscomp::Environment::Instance()->absolutePath(configGetString("scripts.script1"));
			}
			catch ( ... ) {}

			try {
				_scriptStyle1 = configGetBool("scripts.script1.oldStyle");
			}
			catch ( ... ) {
				_scriptStyle1 = true;
			}

			try {
				_scriptExportMap1 = configGetBool("scripts.script1.exportMap");
			}
			catch ( ... ) {
				_scriptExportMap1 = false;
			}

			return true;
		}

		void setupUi(MainWindow *mw) {
			mw->loadEvents(_preloadDays);
			mw->eventSummaryView()->setScript0(_script0, _scriptStyle0,
			                                   _scriptExportMap0);
			mw->eventSummaryView()->setScript1(_script1, _scriptStyle1,
			                                   _scriptExportMap1);
		}


	public:
		float       _preloadDays;
		std::string _script0;
		std::string _script1;
		bool        _scriptStyle0;
		bool        _scriptStyle1;
		bool        _scriptExportMap0;
		bool        _scriptExportMap1;
};


int main(int argc, char** argv) {
	int retCode;

	{
		ESVApp app(argc, argv, Seiscomp::Gui::Application::DEFAULT | Seiscomp::Gui::Application::LOAD_STATIONS);
		app.setPrimaryMessagingGroup("GUI");
		app.addMessagingSubscription("EVENT");
		app.addMessagingSubscription("MAGNITUDE");
		app.addMessagingSubscription("LOCATION");
		app.addMessagingSubscription("FOCMECH");
		retCode = app();
		SEISCOMP_DEBUG("Number of remaining objects before destroying application: %d", Seiscomp::Core::BaseObject::ObjectCount());
	}

	SEISCOMP_DEBUG("Number of remaining objects after destroying application: %d", Seiscomp::Core::BaseObject::ObjectCount());
	return retCode;
}
