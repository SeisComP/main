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


#include <seiscomp/config/config.h>
#include <seiscomp/gui/datamodel/eventlayer.h>

#include "mvmapwidget.h"


using namespace Seiscomp;


namespace {


class DummyEventLayer : public Gui::Map::Layer {
	public:
		DummyEventLayer(QObject *parent) : Gui::Map::Layer(parent) {
			_eventLegend = new Gui::EventLegend(this);
			_mapLegend = new Legend(this);

			addLegend(_eventLegend);
			addLegend(_mapLegend);
		}

		virtual void init(const Seiscomp::Config::Config &cfg) {
			try {
				std::string pos = cfg.getString("eventLegendPosition");
				if ( pos == "topleft" )
					_eventLegend->setArea(Qt::AlignLeft | Qt::AlignTop);
				else if ( pos == "topright" )
					_eventLegend->setArea(Qt::AlignRight | Qt::AlignTop);
				else if ( pos == "bottomright" )
					_eventLegend->setArea(Qt::AlignRight | Qt::AlignBottom);
				else if ( pos == "bottomleft" )
					_eventLegend->setArea(Qt::AlignLeft | Qt::AlignBottom);
			}
			catch ( ... ) {}

			try {
				std::string pos = cfg.getString("mapLegendPosition");
				if ( pos == "topleft" )
					_mapLegend->setArea(Qt::AlignLeft | Qt::AlignTop);
				else if ( pos == "topright" )
					_mapLegend->setArea(Qt::AlignRight | Qt::AlignTop);
				else if ( pos == "bottomright" )
					_mapLegend->setArea(Qt::AlignRight | Qt::AlignBottom);
				else if ( pos == "bottomleft" )
					_mapLegend->setArea(Qt::AlignLeft | Qt::AlignBottom);
			}
			catch ( ... ) {}
		}

		Legend *mapLegend() const {
			return _mapLegend;
		}


	private:
		Gui::EventLegend *_eventLegend;
		Legend           *_mapLegend;
};


}



MvMapWidget::MvMapWidget(const Seiscomp::Gui::MapsDesc &maps,
                         QWidget* parent, Qt::WindowFlags f)
: Gui::MapWidget(maps, parent, f) {
	setMouseTracking(true);

	DummyEventLayer *layer = new DummyEventLayer(this);
	canvas().addLayer(layer);

	_mapLegend = layer->mapLegend();

	connect(QCParameter::Instance(), SIGNAL(parameterChanged(int)),
	        _mapLegend, SLOT(setParameter(int)));
}



ApplicationStatus::Mode MvMapWidget::mode() const {
	return _mapLegend->mode();
}



void MvMapWidget::setMode(ApplicationStatus::Mode mode) {
	_mapLegend->setMode(mode);
}



void MvMapWidget::showMapLegend(bool val) {
	canvas().setDrawLegends(val);
	update();
}
