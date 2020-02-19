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


#ifndef SEISCOMP_GUI_SENDEVENT_H__
#define SEISCOMP_GUI_SENDEVENT_H__


#include <seiscomp/gui/core/application.h>
#ifndef Q_MOC_RUN
#include <seiscomp/datamodel/origin.h>
#endif
#include <string.h>


class SendEvent : public Seiscomp::Gui::Application {
	Q_OBJECT


	public:
		SendEvent(int &argc, char **argv);

	public:
		void createCommandLineDescription();
		bool run();

	private:
		std::string _eventID;
};


#endif
