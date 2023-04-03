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


#ifndef SEISCOMP_GUI_SCRTTV_ASSOCIATOR
#define SEISCOMP_GUI_SCRTTV_ASSOCIATOR


#ifndef Q_MOC_RUN
#include <seiscomp/gui/core/recordview.h>
#include <seiscomp/gui/datamodel/originlocatormap.h>
#include <seiscomp/seismology/locatorinterface.h>
#endif
#include <QWidget>

#include "ui_associator.h"


namespace Seiscomp {
namespace Applications {
namespace TraceView {


class TraceMarker;


class Associator : public QWidget {
	Q_OBJECT

	public:
		Associator(QWidget *parent = 0);

	public:
		bool push(const QVector<TraceMarker*>,
		          Seiscomp::Gui::RecordView::SelectionOperation op);
		bool updatedMarker(TraceMarker *marker);

	private:
		void unsetMessage();
		void setMessage(const QString &message, StatusLabel::Level);
		void setSuccess(const QString &message);
		void setInfo(const QString &message);
		void setWarning(const QString &message);
		void setError(const QString &message);

		void setupOriginInfo();
		void updateOriginInfo();

		void syncPicksView();

	private slots:
		void locatorChanged(QString locator);
		void profileChanged(QString profile);
		void arrivalChanged(int id, bool state);
		void configureLocator();
		void removePick();
		void relocate();
		void showOrigin();
		void inspect();
		void commit();

	private:
		using MarkerBadge = QPair<TraceMarker*, QWidget*>;

		Ui::Associator                          _ui;
		QVector<MarkerBadge>                    _markers;
		Seismology::LocatorInterfacePtr         _locator;
		QWidget                                *_pickContainer;
		Gui::OriginLocatorMap                  *_mapWidget;
		DataModel::OriginPtr                    _origin;
};


}
}
}


#endif
