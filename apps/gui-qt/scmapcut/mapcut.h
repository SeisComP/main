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
#include <seiscomp/gui/map/annotations.h>
#ifndef Q_MOC_RUN
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/inventory.h>
#include <seiscomp/datamodel/origin.h>
#endif
#include <string.h>


class OriginSymbol2;


class MapCut : public Seiscomp::Gui::Application {
	Q_OBJECT

	public:
		MapCut(int& argc, char **argv, Seiscomp::Gui::Application::Type);
		~MapCut();

		void createCommandLineDescription();

		bool run();


	private slots:
		void drawCustomLayer(QPainter *p);
		void renderingCompleted();
		void renderCanvas();


	private:
		OriginSymbol2 *addEvent(Seiscomp::DataModel::Event *evt);
		void resolveStations(OriginSymbol2 *symbol);
		bool findStationLocation(const Seiscomp::DataModel::WaveformStreamID &wid,
		                         const Seiscomp::Core::Time &time,
		                         double &lat, double &lon) const;
		bool fitRegion();

		void drawArrivals(QPainter *p, QList<QRect> labelRects);
		void drawStationSymbol(QPainter *p, const QPoint &pos) const;
		QRect drawInfoBox(QPainter *p, bool paint);
		QRect drawLegend(QPainter *p, bool paint);


	private:
		std::string                 _region;
		std::string                 _margin;
		std::string                 _output;
		std::string                 _dimension;

		double                      _latitude;
		double                      _longitude;
		double                      _depth{10};
		double                      _magnitude{0};

		std::string                 _epFile;
		std::string                 _eventID;
		std::string                 _inventoryFile;
		std::string                 _stationSymbol{"circle"};
		std::string                 _title;

		QRectF                      _reg;
		QSizeF                      _margins;
		QSize                       _dim;

		bool                        _withArrivals{true};
		bool                        _htmlArea{false};
		bool                        _stationLabels{false};
		bool                        _stationTriangles{false};
		bool                        _infoBox{false};
		bool                        _legend{false};

		Seiscomp::DataModel::EventParametersPtr _ep;
		Seiscomp::DataModel::InventoryPtr       _inventory;

		Seiscomp::Gui::Map::Annotations        *_annotations{nullptr};
		Seiscomp::Gui::Map::Canvas             *_canvas{nullptr};
};



#endif
