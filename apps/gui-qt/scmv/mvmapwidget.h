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


#ifndef __MVMAPWIDGET_H___
#define __MVMAPWIDGET_H___

#include <seiscomp/gui/map/mapwidget.h>

#include "legend.h"


class MvMapWidget : public Seiscomp::Gui::MapWidget {
	Q_OBJECT

	// ----------------------------------------------------------------------
	// X'struction
	// ----------------------------------------------------------------------
	public:
		MvMapWidget(const Seiscomp::Gui::MapsDesc &,
		            QWidget* parent = 0, Qt::WindowFlags f = Qt::WindowFlags());

		~MvMapWidget() {}

	// ----------------------------------------------------------------------
	// Public interface
	// ----------------------------------------------------------------------
	public:
		ApplicationStatus::Mode mode() const;
		void setMode(ApplicationStatus::Mode mode);

	// ----------------------------------------------------------------------
	// Public slots
	// ----------------------------------------------------------------------
	public slots:
		void showMapLegend(bool val);

	// ----------------------------------------------------------------------
	// Private data members
	// ----------------------------------------------------------------------
	private:
		Legend *_mapLegend;
};


#endif
