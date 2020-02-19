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

#define SEISCOMP_COMPONENT SendEvent


#ifndef Q_MOC_RUN
#include <seiscomp/datamodel/event.h>
#include <seiscomp/gui/core/messages.h>
#include <seiscomp/logging/log.h>
#endif

#include "sendevent.h"


using namespace std;
using namespace Seiscomp;
using namespace Seiscomp::DataModel;


SendEvent::SendEvent(int &argc, char **argv)
: Application(argc, argv, 0, Application::Tty) {
	setMessagingEnabled(true);
	setMessagingUsername("scsendevt");
	setDatabaseEnabled(true,false);
	setRecordStreamEnabled(false);
	setLoadRegionsEnabled(false);
	setLoadCitiesEnabled(false);
	addLoggingComponentSubscription("Application");
	setPrimaryMessagingGroup(Client::Protocol::LISTENER_GROUP);
}


bool SendEvent::run() {
	if ( !_eventID.empty() ) {
		PublicObjectPtr po = SCApp->query()->loadObject(Event::TypeInfo(), _eventID);
		EventPtr e = Event::Cast(po);
		if ( !e ) {
			SEISCOMP_WARNING("Event %s has not been found.\n", _eventID.c_str());
			cerr << "Warning: EventID " << _eventID.c_str() << " has not been found.\n";
			return false;
		}

		// Workaround for not to open window QMessageBox in the Application::sendCommand
		// when application is commandline
		if ( commandTarget().empty() && type() == Tty ) {
			cerr << "WARNING: \n"
			        "\tVariable <commands.target> is not set. To disable sending commands \n"
			        "\tto all connected clients, set a proper target. You can use \n"
			        "\tregular expressions to specify a group of clients (HINT: all = '.*$').\n\n";
			return false;
		}

		sendCommand(Gui::CM_SHOW_ORIGIN, e->preferredOriginID());
		return true;
	}

	cerr << "must specify event using '-E eventID'\n";
	return false;
}


void SendEvent::createCommandLineDescription() {
	commandline().addGroup("Options");
	commandline().addOption("Options", "event-id,E", "eventID to show details", &_eventID, false);
}
