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

#ifndef SEISCOMP_SEARCHWIDGET_H__
#define SEISCOMP_SEARCHWIDGET_H__


#include <vector>

#include <QString>
#include <QWidget>

#include "ui_searchwidget.h"

class SearchWidget : public QWidget {
	Q_OBJECT

	// ----------------------------------------------------------------------
	// Nested Types
	// ----------------------------------------------------------------------
	public:
		typedef std::vector<std::string> Matches;

	private:
		typedef std::vector<std::string> StationNames;

	// ----------------------------------------------------------------------
	// X'struction
	// ----------------------------------------------------------------------
	private:
		SearchWidget(QWidget* parent = 0, Qt::WindowFlags f = Qt::WindowFlags());
		~SearchWidget();

	// ----------------------------------------------------------------------
	// Public Interface
	// ----------------------------------------------------------------------
	public:
		void addStationName(const std::string& name);
		const Matches& matches();
		const Matches& previousMatches() const;

		static SearchWidget* Create(QWidget* parent = 0, Qt::WindowFlags f = Qt::WindowFlags());

	// ----------------------------------------------------------------------
	// Protected Interface
	// ----------------------------------------------------------------------
	protected:
		virtual void keyPressEvent(QKeyEvent* evt);

	// ----------------------------------------------------------------------
	// Private Interface
	// ----------------------------------------------------------------------
	private:
		void updateContent();

		void matchString(const std::string& str);

		void addTableWidgetItem(const std::string& name);

		void removeContent();
		void removeMatches();

		void addMatch(const std::string& station);

	// ----------------------------------------------------------------------
	// Private Slots
	// ----------------------------------------------------------------------
	private slots:
		void findStation(const QString& str);
		void findStation();
		void getSelectedStation();

	signals:
		void stationSelected();

	// ----------------------------------------------------------------------
	// Private Datamember
	// ----------------------------------------------------------------------
	private:
		Ui::SearchWidget _ui;

		Matches      _matches;
		Matches      _previousMatches;
		StationNames _stationNames;

		static int _ObjectCount;
};


#endif
