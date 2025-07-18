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


#define SEISCOMP_COMPONENT RTTV

#include <seiscomp/logging/log.h>
#include <seiscomp/gui/core/application.h>

#include "settings.h"
#include "mainwindow.h"


using namespace std;
using namespace Seiscomp::Gui;


namespace Seiscomp {
namespace Applications {
namespace TraceView {


Settings Settings::global;


class App : public Kicker<MainWindow> {
	public:
		App(int& argc, char** argv, int flags = DEFAULT)
		: Kicker<MainWindow>(argc, argv, flags) {
			setPrimaryMessagingGroup("GUI");
			addMessagingSubscription("PICK");
			addMessagingSubscription("EVENT");
			addMessagingSubscription("LOCATION");
			addMessagingSubscription("GUI");
			addMessagingSubscription("CONFIG");
			setLoadCitiesEnabled(false);

			bindSettings(&Settings::global);
		}

	protected:
		void printUsage() const override {
			cout << "Usage:" << endl
			     << "  " << name() << " [options] [miniSEED file]"
			     << endl << endl
			     << "View waveforms from RecordStream or miniSEED file."
			     << endl;

			Seiscomp::Gui::Application::printUsage();

			cout << "Examples:" << endl;
			cout << "View waveforms with default settings printing debug information"
			     << endl
			     << "  scrttv --debug"
			     << endl << endl;
			cout << "View 3C data from default recordstream 3 hours before "
			        "midnight. All available picks are mapped and displayed."
			     << endl
			     << "  scrttv --buffer-size 10800 --end-time 2026-05-01 --map-picks"
			     << endl << endl;
			cout << "View data from a miniSEED file in offline mode without messaging"
			     << endl
			     << "  scrttv file.mseed"
			     << endl << endl;
			cout << "View all HH streams from stations CX.PB01 and CX.PB02 "
			        "without messaging and inventory"
			     << endl
			     << "  scrttv --offline --no-inventory --channels CX.PB01.*.HH? --channels CX.PB02.*.HH?"
			     << endl << endl;
			cout << "View the miniSEED data from all file ending with .mseed "
			        "which are read from stdin"
			     << endl
			     << "  cat *.mseed | scrttv -"
			     << endl << endl;
			cout << "View miniSEED data played back from archive at normal speed "
			        "as in real time using scart"
			     << endl
			     << "  scart -dmv -t 2026-05-01~2026-05-02 /archive | scrttv -I - --offline --no-inventory"
			     << endl;
		}

		bool validateParameters() override {
			if ( !Kicker<Seiscomp::Applications::TraceView::MainWindow>::validateParameters() ) {
				return false;
			}

			bool hasPositionals = false;
			for ( const auto & opt : commandline().unrecognizedOptions() ) {
				if ( !opt.empty() && (opt.at(0) != '-' || opt.length() == 1 ) ) {
					hasPositionals = true;
					break;
				}
			}

			if ( hasPositionals ) {
				Settings::global.offline = true;
			}

			if ( Settings::global.offline ) {
				setMessagingEnabled(false);
				setLoadConfigModuleEnabled(false);
				setDatabaseEnabled(false, false);
			}

			// Disable database access if an inventory xml file is given
			// as a command line argument.
			if ( Settings::global.inventoryDisabled
			  || (!isInventoryDatabaseEnabled() && !isConfigDatabaseEnabled()) ) {
				setDatabaseEnabled(false, false);
			}

			if ( commandline().hasOption("record-file") ) {
				//setMessagingEnabled(false);
				setDatabaseEnabled(false, false);
			}

			if ( !Settings::global.filter.empty() ) {
				Settings::global.filters.clear();
				Settings::global.filters.push_back(Settings::global.filter);
			}

			if ( Settings::global.endTime ) {
				cout << "Set defined endtime: " << Settings::global.endTime->toString("%F %T") << endl;
			}

			return true;
		}

		void setupUi(MainWindow *w) override {
			w->start();
			if ( !Settings::global.inputFile.empty() ) {
				w->openFile(Settings::global.inputFile);
			}
		}
};


}
}
}



int main(int argc, char *argv[]) {
	int retCode;

	{
		Seiscomp::Applications::TraceView::App app(
			argc, argv,
			Application::DEFAULT |
			Application::LOAD_STATIONS |
			Application::LOAD_CONFIGMODULE
		);

		retCode = app();
		SEISCOMP_DEBUG("Number of remaining objects before destroying application: %d", Seiscomp::Core::BaseObject::ObjectCount());
	}

	SEISCOMP_DEBUG("Number of remaining objects after destroying application: %d", Seiscomp::Core::BaseObject::ObjectCount());
	return retCode;
}
