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


#ifndef SEISCOMP_GUI_PLUGIN_MAPVIEWWIDGET_H__
#define SEISCOMP_GUI_PLUGIN_MAPVIEWWIDGET_H__

#include <QtGui>
#ifndef Q_MOC_RUN
#include <seiscomp/core/message.h>
#endif
#include <seiscomp/gui/map/mapwidget.h>

namespace Seiscomp {
namespace Gui {
namespace MessageMonitor {


class MainWidget : public Seiscomp::Gui::MapWidget {
	Q_OBJECT

	public:
		MainWidget(QWidget* parent, QObject* signaller,
		           const MapsDesc &maps);

	public slots:
		void onReceivedMessage(Seiscomp::Core::MessagePtr);
};


}
}
}

#endif

