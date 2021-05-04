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

#include <seiscomp/gui/core/mainwindow.h>
#ifndef Q_MOC_RUN
#include <seiscomp/datamodel/databasequery.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/eventparameters.h>
#endif
#include "ui_mainframe.h"

#include <QtGui>
#include <QSystemTrayIcon>

namespace Seiscomp {
namespace Gui {


class EventListView;
class EventSummary;
class EventEdit;
class OriginLocatorView;
class MagnitudeView;
class PickerView;


class MainFrame : public MainWindow {
	Q_OBJECT

	public:
		MainFrame();
		~MainFrame();

		void setEventID(const std::string &eventID);
		void setOriginID(const std::string &originID);
		void loadEvents(float days);
		void setOffline(bool);


	protected slots:
		void showEventList();
		void showLocator();
		void originAdded();
		void objectAdded(const QString&, Seiscomp::DataModel::Object*);
		void objectRemoved(const QString&, Seiscomp::DataModel::Object*);
		void objectUpdated(const QString&, Seiscomp::DataModel::Object*);

	private slots:
		void configureAcquisition();

		void messageAvailable(Seiscomp::Core::Message*, Seiscomp::Client::Packet*);

		void eventAdded(Seiscomp::DataModel::Event*, bool);
		void updateEventTabText();
		void setOrigin(Seiscomp::DataModel::Origin*, Seiscomp::DataModel::Event*, bool, bool);
		void updateOrigin(Seiscomp::DataModel::Origin*, Seiscomp::DataModel::Event*);
		void committedNewOrigin(Seiscomp::DataModel::Origin*, Seiscomp::DataModel::Event*);
		void setArtificialOrigin(Seiscomp::DataModel::Origin*);
		void setData(Seiscomp::DataModel::Origin*, Seiscomp::DataModel::Event*);
		void setFMData(Seiscomp::DataModel::Event*);
		void originReferenceAdded(const std::string &, Seiscomp::DataModel::OriginReference*);
		void originReferenceRemoved(const std::string &, Seiscomp::DataModel::OriginReference*);
		void showMagnitude(const std::string &);
		void tabChanged(int);
		void showWaveforms();
		void publishEvent();

		void hoverEvent(const std::string &eventID);
		void selectEvent(const std::string &eventID);

		void raiseLocator();

		void fileOpen();
		void fileSave();

#if QT_VERSION >= 0x040300
		void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
		void trayIconMessageClicked();
#endif

	private:
		bool populateOrigin(Seiscomp::DataModel::Origin*, Seiscomp::DataModel::Event*, bool);

		// This creates an EventParameters instance containing copies
		// of all event attributes relevant for publication incl.
		// focal mechanisms, moment magnitude etc. if available.
		// TODO: evaluate if this fits better somewhere else.
		Seiscomp::DataModel::EventParametersPtr _createEventParametersForPublication(const Seiscomp::DataModel::Event *event);

	protected:
		void toggledFullScreen(bool);
		void closeEvent(QCloseEvent *e);

	private:
		Ui::MainFrame      _ui;
#if QT_VERSION >= 0x040300
		QSystemTrayIcon   *_trayIcon;
		std::string        _trayMessageEventID;
#endif
		QAction           *_actionConfigureAcquisition;
		EventListView     *_eventList;
		EventSummary      *_eventSmallSummary;
		EventSummary      *_eventSmallSummaryCurrent;
		EventEdit         *_eventEdit;
		OriginLocatorView *_originLocator;
		MagnitudeView     *_magnitudes;
		QWidget           *_tabEventEdit;
		PickerView        *_picker;

		DataModel::OriginPtr _currentOrigin;
		DataModel::EventParametersPtr _offlineData;

		bool               _expertMode;
		bool               _magnitudeCalculationEnabled;
		bool               _computeMagnitudesAutomatically;
		bool               _computeMagnitudesSilently;
		bool               _askForMagnitudeTypes;
		std::string        _exportScript;
		std::string        _eventID;
		bool               _exportScriptTerminate;
		QWidget           *_currentTabWidget;
		QProcess           _exportProcess;
};


}
}

#endif
