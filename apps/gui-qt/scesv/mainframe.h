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




#ifndef __MAINFRAME_H__
#define __MAINFRAME_H__

#include <QtGui>
#include <seiscomp/gui/core/mainwindow.h>
#ifndef Q_MOC_RUN
#include <seiscomp/datamodel/databasequery.h>
#endif
#include "ui_mainframe.h"
#include <seiscomp/gui/datamodel/magnitudeview.h>


namespace Seiscomp {

namespace Gui {

class EventSummaryView;
class EventListView;

}

namespace Applications {
namespace EventSummaryView {


class MainFrame : public Gui::MainWindow {
	Q_OBJECT

	public:
		MainFrame();
		~MainFrame();

		Gui::EventSummaryView* eventSummaryView() const;

		void loadEvents(float days);


	protected slots:
		void toggleDock();
		void showLocator();
		void toggledFullScreen(bool);
		void toggleEventList();
		void showInStatusbar(QString text, int time);
		void setLastNonFakeEvent();
		void clearStatusbar();
		void showESVTab();
		void updateEventTabText();

	private:
		Ui::MainFrame _ui;
		QLabel *_wt;
		Gui::EventSummaryView *_eventSummary;
		Gui::EventListView    *_listPage;
};


}
}
}

#endif
