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

#ifndef SEISCOMP_GUI_SCMAPCUT_H__
#define SEISCOMP_GUI_SCMAPCUT_H__

#include <seiscomp/gui/core/application.h>
#include <seiscomp/gui/datamodel/originsymbol.h>
#include <seiscomp/gui/map/canvas.h>
#include <seiscomp/core/datetime.h>

#ifndef Q_MOC_RUN
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/arrival.h>
#endif

#include <string>
#include <QColor>

class MapCut : public Seiscomp::Gui::Application {
	Q_OBJECT

	public:
		MapCut(int& argc, char **argv, Seiscomp::Gui::Application::Type);
		~MapCut();
		void createCommandLineDescription();
		bool run();

	private slots:
		void drawArrivals(QPainter *p);
		void drawMagnitudeInfo(QPainter *p);
		void drawMagnitudeScale(QPainter *p);
		void drawDistanceRings(QPainter *p);
		void drawEventLabels(QPainter *p);
		void generateEnhancedHtmlAreas(QPainter *p);
		void renderingCompleted();
		void renderCanvas();

	private:
		// Helper functions for enhanced features
		QColor getMagnitudeColor(double magnitude);
		QColor getDepthColor(double depth);
		int getMagnitudeRadius(double magnitude);
		bool isEventInTimeRange(const Seiscomp::Core::Time &eventTime);
		bool isEventInMagnitudeRange(double magnitude);
		std::string extractStationCode(Seiscomp::DataModel::Arrival *arr, Seiscomp::DataModel::EventParameters *ep);

		// Original parameters
		std::string                 _region;
		std::string                 _margin;
		std::string                 _output;
		std::string                 _dimension;
		double                      _latitude;
		double                      _longitude;
		double                      _depth{10};
		double                      _magnitude{3};
		std::string                 _epFile;
		std::string                 _eventID;
		QRectF                      _reg;
		QSizeF                      _margins;
		QSize                       _dim;
		bool                        _withArrivals{true};
		bool                        _htmlArea{false};

		// Enhanced visualization parameters
		bool                        _showMagnitudeInfo{false};
		bool                        _triangleStations{false};
		bool                        _showScale{false};
		bool                        _showStationCodes{false};
		bool                        _distanceRings{false};
		bool                        _depthColors{false};
		bool                        _eventLabels{false};
		
		// Filtering parameters
		std::string                 _timeRange;
		Seiscomp::Core::Time        _timeStart;
		Seiscomp::Core::Time        _timeEnd;
		double                      _minMagnitude{-10.0};
		double                      _maxMagnitude{10.0};

		// Canvas and event parameters
		Seiscomp::Gui::Map::Canvas *_canvas{nullptr};
		Seiscomp::DataModel::EventParameters *_eventParameters{nullptr};
};

#endif