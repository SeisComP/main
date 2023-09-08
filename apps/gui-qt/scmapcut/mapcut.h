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
#ifndef Q_MOC_RUN
#include <seiscomp/datamodel/origin.h>
#endif
#include <string.h>



class MapCut : public Seiscomp::Gui::Application {
	Q_OBJECT

	public:
		MapCut(int& argc, char **argv, Seiscomp::Gui::Application::Type);
		~MapCut();

		void createCommandLineDescription();

		bool run();


	private slots:
		void drawArrivals(QPainter *p);
		void renderingCompleted();
		void renderCanvas();


	private:
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

		Seiscomp::Gui::Map::Canvas *_canvas{nullptr};
};



#endif
