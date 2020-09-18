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




#include <seiscomp/gui/core/application.h>
#include "mainframe.h"

using namespace Seiscomp::Gui;


class MMApp : public Kicker<MessageMonitor::MainFrame> {
	public:
		MMApp(int& argc, char** argv, int flags = DEFAULT) :
		  Kicker<MessageMonitor::MainFrame>(argc, argv, flags) {}

	protected:
		bool init() {
			if ( !Kicker<MessageMonitor::MainFrame>::init() )
				return false;

			if ( connection() )
				connection()->subscribe("STATUS_GROUP");

			return true;
		}
};


int main(int argc, char *argv[])
{
	int retCode;

	{
		MMApp app(argc, argv, Application::SHOW_SPLASH | Application::WANT_MESSAGING | Application::OPEN_CONNECTION_DIALOG);
		app.setPrimaryMessagingGroup(Seiscomp::Client::Protocol::LISTENER_GROUP);
		app.setMembershipMessagesEnabled(true);
		app.setMessagingUsername("");
		app.addMessagingSubscription("*");
		retCode = app();
		SEISCOMP_DEBUG("Number of remaining objects before destroying application: %d", Seiscomp::Core::BaseObject::ObjectCount());
	}

	SEISCOMP_DEBUG("Number of remaining objects after destroying application: %d", Seiscomp::Core::BaseObject::ObjectCount());
	return retCode;
}
