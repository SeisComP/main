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
#ifndef Q_MOC_RUN
#include <seiscomp/datamodel/databasequery.h>
#include <seiscomp/core/message.h>
#include <seiscomp/datamodel/configstation.h>
#endif
#include <seiscomp/gui/core/mainwindow.h>
#include <seiscomp/gui/core/messages.h>

#include <utility>

#ifndef Q_MOC_RUN
#include "qcview.h"
#endif

#include "ui_mainframe.h"


namespace Seiscomp {

namespace Applications {
namespace Qc {

class QcConfig;
class QcPlugin;

DEFINE_SMARTPOINTER(QcPlugin);
typedef std::vector<QcPluginPtr> QcPlugins;

}}

using namespace Seiscomp::Applications::Qc;

namespace Gui {


class MainFrame : public MainWindow {
	Q_OBJECT

	public:
		MainFrame();
		~MainFrame();

	protected slots:
		void toggleDock();
		void toggledFullScreen(bool);
		std::list<std::pair<std::string, bool> > streams();

	public slots:
		void readMessage(Seiscomp::Core::Message*, Seiscomp::Client::Packet*);
		bool sendStationState(QString, bool);

	private:
		void prepareNotifier(QString streamID, bool enable);
		DataModel::ConfigStation *configStation(const std::string& net, const std::string& sta) const;

	private:
		Ui::MainFrame  _ui;
		QLabel        *_wt;

		QcViewConfig  *_config;
		QcModel       *_qcModel;
		QcView        *_qcReportView;
		QcView        *_qcOverView;
};


}
}

#endif
