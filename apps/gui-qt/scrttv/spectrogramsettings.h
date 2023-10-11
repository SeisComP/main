/***************************************************************************
 * Copyright (C) gempa GmbH                                                *
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


#ifndef SEISCOMP_GUI_SCRTTV_SPECTROGRAMSETTINGS
#define SEISCOMP_GUI_SCRTTV_SPECTROGRAMSETTINGS


#ifndef Q_MOC_RUN
#include <seiscomp/gui/core/recordview.h>
#include <seiscomp/gui/datamodel/originlocatormap.h>
#include <seiscomp/seismology/locatorinterface.h>
#endif
#include <QWidget>

#include "ui_spectrogramsettings.h"


namespace Seiscomp {
namespace Applications {
namespace TraceView {


class TraceMarker;


class SpectrogramSettings : public QWidget {
	Q_OBJECT

	public:
		SpectrogramSettings(QWidget *parent = 0);

	signals:
		void apply();

	public:
		Ui::SpectrogramSettings ui;
};


}
}
}


#endif
