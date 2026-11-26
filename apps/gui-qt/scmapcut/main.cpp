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

#include <seiscomp/math/geo.h>
#include <seiscomp/io/archive/xmlarchive.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/event.h>
#include <seiscomp/datamodel/magnitude.h>
#include <seiscomp/datamodel/arrival.h>
#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/network.h>
#include <seiscomp/datamodel/station.h>
#include <seiscomp/geo/boundingbox.h>
#include <seiscomp/seismology/regions.h>
#include <seiscomp/gui/core/application.h>
#include <seiscomp/gui/core/compat.h>
#include <seiscomp/gui/map/projection.h>

#include <QPolygon>
#include <QLinearGradient>

#include "mapcut.h"


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
using namespace std;
using namespace Seiscomp;
using namespace Seiscomp::Gui;
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void parseDim(const string &str, QSize *size) {
	size_t pos = str.find('x');

	if ( size ) *size = QSize(0,0);

	if ( pos == string::npos )
		return;

	int w,h;

	if ( !Core::fromString(w, str.substr(0,pos)) )
		return;

	size_t end = str.find('+', pos);
	if ( end == string::npos ) end = str.size();

	if ( !Core::fromString(h, str.substr(pos+1,end-pos-1)) )
		return;

	if ( w <= 0 || h <= 0 )
		return;

	if ( size ) *size = QSize(w,h);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool parseMargin(const string &str, QSizeF *size) {
	bool symmetric = false;

	size_t pos = str.find('x');

	if ( size ) *size = QSizeF(0,0);

	if ( pos == string::npos )
		symmetric = true;

	qreal w,h;

	if ( !Core::fromString(h, str.substr(0,pos)) )
		return false;

	if ( symmetric )
		w = h;
	else if ( !Core::fromString(w, str.substr(pos+1)) )
		return false;

	if ( size ) *size = QSizeF(w,h);
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void parseRegion(const string &str, QRectF *rect) {
	size_t pos = str.find('x');

	if ( rect ) *rect = QRectF();

	float w,h;
	float lat0, lon0;

	if ( pos == string::npos ) {
		// 4 values
		size_t end = min(str.find('+', 1), str.find('-', 1));
		if ( end == string::npos )
			return;

		if ( !Core::fromString(lat0, str.substr(0,end)) )
			return;

		size_t end2 = min(str.find('+', end+1), str.find('-', end+1));
		if ( end2 == string::npos )
			return;

		if ( !Core::fromString(lon0, str.substr(end,end2-end)) )
			return;

		end = end2;
		end2 = min(str.find('+', end+1), str.find('-', end+1));
		if ( end2 == string::npos )
			return;

		if ( !Core::fromString(w, str.substr(end,end2-end)) )
			return;

		end = end2;

		if ( !Core::fromString(h, str.substr(end)) )
			return;

		if ( rect ) *rect = QRectF(lon0, lat0, w-lon0, h-lat0);
		return;
	}

	if ( !Core::fromString(w, str.substr(0,pos)) )
		return;

	size_t end = min(str.find('+', pos), str.find('-', pos));
	if ( end == string::npos ) end = str.size();

	if ( !Core::fromString(h, str.substr(pos+1,end-pos-1)) )
		return;

	size_t end2 = min(str.find('+', end+1), str.find('-', end+1));
	if ( end2 == string::npos ) end2 = str.size();

	if ( !Core::fromString(lat0, str.substr(end,end2-end)) )
		return;

	if ( end2 == str.size() ) return;

	if ( !Core::fromString(lon0, str.substr(end2)) )
		return;

	if ( rect ) *rect = QRectF(lon0, lat0, h, w);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void setupView(Map::Canvas *canvas, const QRectF &rect) {
	canvas->displayRect(rect);

	/*
	QPointF center = rect.center();

	if ( canvas->width() <= 0 || canvas->height() <= 0 ) {
		canvas->setView(center, 1);
		return;
	}

	double dt = max(rect.width()/canvas->width(), rect.height()/canvas->height());
	if ( dt <= 0 ) {
		canvas->setView(center, 1);
		return;
	}

	canvas->setView(center, 360.0 / (canvas->width()*dt));
	*/
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
namespace {


struct StationEntry {
	double               lat{0};
	double               lon{0};
	string               streamID;
	string               phase;
	string               pickID;
	OPT(double)          residual;
	bool                 enabled{true};
	Map::AnnotationItem *annotation{nullptr};
};


}


class OriginSymbol2 : public OriginSymbol {
	using OriginSymbol::OriginSymbol;

	public:
		DataModel::Origin    *origin{nullptr};
		DataModel::Event     *event{nullptr};
		DataModel::Magnitude *magnitude{nullptr};
		vector<StationEntry>  stations;
};

MapCut::MapCut(int& argc, char **argv, Application::Type type)
: Application(argc, argv, 0, type) {
	setMessagingEnabled(false);
	setDatabaseEnabled(false, false);
	setRecordStreamEnabled(false);
	setLoadRegionsEnabled(true);

	_dim = QSize(0,0);
	_margins = QSizeF(0,0);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
MapCut::~MapCut() {
	if ( _canvas ) {
		delete _canvas;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MapCut::createCommandLineDescription() {
	commandline().addGroup("Options");
	commandline().addOption("Options", "region,r", "cut region ([lat_dim]x[lon_dim]+lat0+lon0 or +lat0+lon+lat1+lon1)", &_region, false);
	commandline().addOption("Options", "margin,m", "margin in degrees around origin (margin|margin_latxmargin_lon", &_margin, false);
	commandline().addOption("Options", "dimension,d", "output image dimension (wxh)", &_dimension, false);
	commandline().addOption("Options", "output,o", "output image", &_output, false);
	commandline().addOption("Options", "lat", "latitude of symbol", &_latitude, false);
	commandline().addOption("Options", "lon", "longitude of symbol", &_longitude, false);
	commandline().addOption("Options", "depth", "depth of event", &_depth, false);
	commandline().addOption("Options", "mag", "magnitude of event", &_magnitude, false);
	commandline().addOption("Options", "layers", "draw polygonal layers");
	commandline().addOption("Options", "ep", "EventParameters (XML) to load", &_epFile, false);
	commandline().addOption("Options", "event-id,E", "event to plot", &_eventID, false);
	commandline().addOption("Options", "inventory-xml", "inventory (XML) to read station coordinates from", &_inventoryFile, false);
	commandline().addOption("Options", "without-arrivals", "do not render arrivals / stations");
	commandline().addOption("Options", "station-labels", "annotate stations with their station code");
	commandline().addOption("Options", "station-symbol", "station symbol: circle or triangle", &_stationSymbol, false);
	commandline().addOption("Options", "info", "draw a box with origin information");
	commandline().addOption("Options", "title", "title of the information box, implies --info", &_title, false);
	commandline().addOption("Options", "legend", "draw a travel time residual legend");
	commandline().addOption("Options", "html-area", "print html/area section");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
OriginSymbol2 *MapCut::addEvent(DataModel::Event *evt) {
	DataModel::Origin *org = _ep->findOrigin(evt->preferredOriginID());
	if ( !org ) {
		return nullptr;
	}

	auto symbol = new OriginSymbol2(org->latitude(), org->longitude());
	symbol->setDepth(org->depth().value());
	symbol->origin = org;
	symbol->event = evt;
	_canvas->symbolCollection()->add(symbol);

	if ( !evt->preferredMagnitudeID().empty() ) {
		auto mag = _magnitude;
		symbol->magnitude = org->findMagnitude(evt->preferredMagnitudeID());
		if ( symbol->magnitude ) {
			try { mag = symbol->magnitude->magnitude().value(); }
			catch ( ... ) {}
		}
		symbol->setPreferredMagnitudeValue(mag);
	}

	resolveStations(symbol);

	return symbol;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MapCut::resolveStations(OriginSymbol2 *symbol) {
	auto org = symbol->origin;

	for ( size_t i = 0; i < org->arrivalCount(); ++i ) {
		DataModel::Arrival *arr = org->arrival(i);
		StationEntry entry;

		entry.pickID = arr->pickID();
		entry.phase = arr->phase().code();
		try { entry.residual = arr->timeResidual(); } catch ( ... ) {}
		try { entry.enabled = arr->weight() > 0; } catch ( ... ) {}

		bool located = false;
		DataModel::Pick *pick = _ep->findPick(arr->pickID());
		if ( pick ) {
			const auto &wid = pick->waveformID();
			entry.streamID = wid.networkCode() + "." + wid.stationCode() + "." +
			                 wid.locationCode() + "." + wid.channelCode();
			located = findStationLocation(wid, org->time().value(), entry.lat, entry.lon);

			if ( _stationLabels ) {
				entry.annotation = _annotations->add(wid.stationCode().c_str());
			}
		}

		if ( !located ) {
			// No inventory: derive the location from distance and azimuth
			try {
				Math::Geo::delandaz2coord(
					arr->distance(), arr->azimuth(),
					org->latitude().value(), org->longitude().value(),
					&entry.lat, &entry.lon
				);
			}
			catch ( ... ) {
				continue;
			}
		}

		symbol->stations.push_back(entry);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool MapCut::findStationLocation(const DataModel::WaveformStreamID &wid,
                                 const Core::Time &time,
                                 double &lat, double &lon) const {
	if ( !_inventory ) {
		return false;
	}

	for ( size_t n = 0; n < _inventory->networkCount(); ++n ) {
		auto net = _inventory->network(n);
		if ( net->code() != wid.networkCode() ) {
			continue;
		}

		for ( size_t s = 0; s < net->stationCount(); ++s ) {
			auto sta = net->station(s);
			if ( sta->code() != wid.stationCode() ) {
				continue;
			}

			if ( time < sta->start() ) {
				continue;
			}

			try {
				if ( time >= sta->end() ) {
					continue;
				}
			}
			catch ( ... ) {}

			try {
				lat = sta->latitude();
				lon = sta->longitude();
				return true;
			}
			catch ( ... ) {}
		}
	}

	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool MapCut::fitRegion() {
	if ( _canvas->symbolCollection()->count() == 0 ) {
		return false;
	}

	// Longitudes are unwrapped relative to the first symbol to handle
	// regions crossing the dateline
	double refLon = (*_canvas->symbolCollection()->begin())->longitude();
	double minLat = 90, maxLat = -90;
	double minLon = refLon, maxLon = refLon;

	auto extend = [&](double lat, double lon) {
		while ( lon - refLon > 180 ) lon -= 360;
		while ( lon - refLon < -180 ) lon += 360;
		minLat = min(minLat, lat);
		maxLat = max(maxLat, lat);
		minLon = min(minLon, lon);
		maxLon = max(maxLon, lon);
	};

	for ( auto symbol : *_canvas->symbolCollection() ) {
		extend(symbol->latitude(), symbol->longitude());

		if ( !_withArrivals ) {
			continue;
		}

		for ( const auto &entry : static_cast<OriginSymbol2*>(symbol)->stations ) {
			extend(entry.lat, entry.lon);
		}
	}

	double padLat = max((maxLat - minLat) * 0.1, 0.5);
	double padLon = max((maxLon - minLon) * 0.1, 0.5);

	minLat = max(minLat - padLat, -90.0);
	maxLat = min(maxLat + padLat, 90.0);
	minLon -= padLon;
	maxLon += padLon;

	_reg = QRectF(minLon, minLat, maxLon - minLon, maxLat - minLat);
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool MapCut::run() {
	SCScheme.map.vectorLayerAntiAlias = true;

	if ( commandline().hasOption("without-arrivals") ) {
		_withArrivals = false;
	}

	_htmlArea = commandline().hasOption("html-area");
	_stationLabels = commandline().hasOption("station-labels");
	_infoBox = commandline().hasOption("info") || !_title.empty();
	_legend = commandline().hasOption("legend");

	if ( _stationSymbol == "triangle" ) {
		_stationTriangles = true;
	}
	else if ( _stationSymbol != "circle" ) {
		cerr << "Invalid station symbol: " << _stationSymbol << endl;
		return false;
	}

	Map::ImageTreePtr mapTree = new Map::ImageTree(mapsDesc());

	_canvas = new Map::Canvas(mapTree.get());
	_canvas->setParent(this);
	_annotations = new Map::Annotations(this);

	OPT(double) centerLat, centerLon;

	if ( !_epFile.empty() ) {
		IO::XMLArchive ar;
		if ( !ar.open(_epFile.c_str()) ) {
			cerr << "Unable to open file '" << _epFile << "'" << endl;
			return false;
		}

		ar >> _ep;

		if ( !_ep ) {
			cerr << "File '" << _epFile << "' does not contain event parameters" << endl;
			return false;
		}
	}

	if ( !_inventoryFile.empty() ) {
		IO::XMLArchive ar;
		if ( !ar.open(_inventoryFile.c_str()) ) {
			cerr << "Unable to open file '" << _inventoryFile << "'" << endl;
			return false;
		}

		ar >> _inventory;

		if ( !_inventory ) {
			cerr << "File '" << _inventoryFile << "' does not contain an inventory" << endl;
			return false;
		}
	}

	if ( !_eventID.empty() ) {
		if ( !_ep ) {
			cerr << "No event parameters available, see --ep" << endl;
			return false;
		}

		DataModel::Event *evt = _ep->findEvent(_eventID);
		if ( !evt ) {
			cerr << "Event '" << _eventID << "' not found" << endl;
			return false;
		}

		auto symbol = addEvent(evt);
		if ( !symbol ) {
			cerr << "Preferred origin for event '" << _eventID << "' not found" << endl;
			return false;
		}

		centerLat = symbol->origin->latitude().value();
		centerLon = symbol->origin->longitude().value();
	}
	// Grab all events
	else if ( _ep ) {
		OPT(Geo::GeoBoundingBox) bbox;

		for ( size_t i = 0; i < _ep->eventCount(); ++i ) {
			auto evt = _ep->event(i);
			auto symbol = addEvent(evt);

			if ( !symbol ) {
				cerr << "Preferred origin for event '" << evt->publicID()
				     << "' not found: skipping" << endl;
				continue;
			}

			auto org = symbol->origin;
			Geo::GeoBoundingBox orgBox(
				org->latitude().value(), org->longitude().value(),
				org->latitude().value(), org->longitude().value()
			);

			if ( !bbox ) {
				bbox = orgBox;
			}
			else {
				*bbox += orgBox;
			}
		}

		if ( bbox ) {
			centerLat = bbox->center().latitude();
			centerLon = bbox->center().longitude();
		}
	}

	if ( commandline().hasOption("lat") && commandline().hasOption("lon") ) {
		if ( _canvas->symbolCollection()->count() == 0 ) {
			auto symbol = new OriginSymbol2(_latitude, _longitude);
			symbol->setDepth(DataModel::RealQuantity(_depth));
			symbol->setPreferredMagnitudeValue(_magnitude);
			_canvas->symbolCollection()->add(symbol);
		}

		centerLat = _latitude;
		centerLon = _longitude;
	}

	if ( _output.empty() ) {
		cerr << "No output image given" << endl;
		return false;
	}

	parseDim(_dimension, &_dim);

	if ( _dim.isEmpty() ) {
		cerr << "Wrong output dimensions" << endl;
		return false;
	}

	_canvas->setSize(_dim.width(), _dim.height());

	if ( !_region.empty() ) {
		parseRegion(_region, &_reg);
		if ( _reg.isNull() || _reg.isEmpty() ) {
			cerr << "Invalid region: " << _region << endl;
			return false;
		}
	}

	if ( !_margin.empty() && !parseMargin(_margin, &_margins) ) {
		cerr << "Invalid margins: " << _margin << endl;
		return false;
	}

	if ( _region.empty() ) {
		if ( !centerLat || !centerLon ) {
			cerr << "No region, no origin or not lat/lon given" << endl;
			return false;
		}

		if ( _margins.isEmpty() ) {
			// Fit the region to all origins and stations
			if ( !fitRegion() ) {
				cerr << "No region and no margins given" << endl;
				return false;
			}
		}
		else {
			_reg = QRectF(*centerLon - _margins.width(),
			              *centerLat - _margins.height(),
			              _margins.width() * 2, _margins.height() * 2);
		}
	}

	setupView(_canvas, QRectF(_reg.left(),_reg.top(),_reg.width(),_reg.height()));

	_canvas->setPreviewMode(false);

	if ( commandline().hasOption("layers") ) {
		_canvas->setDrawLayers(true);
	}

	connect(_canvas, SIGNAL(customLayer(QPainter*)),
	        this, SLOT(drawCustomLayer(QPainter*)));

	connect(_canvas, SIGNAL(updateRequested()), this, SLOT(renderCanvas()));
	renderCanvas();

	if ( !_canvas->renderingComplete() ) {
		connect(_canvas, SIGNAL(renderingCompleted()), this, SLOT(renderingCompleted()));
		cerr << "Requests in progress: waiting" << endl;

		return Application::run();
	}

	if ( !_canvas->buffer().save(_output.c_str(), NULL, 100) ) {
		cerr << "Saving the image failed" << endl;
		return false;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MapCut::renderCanvas() {
	QPainter p(&_canvas->buffer());
	_canvas->draw(p);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MapCut::renderingCompleted() {
	cerr << "Rendering finished" << endl;

	if ( !_canvas->buffer().save(_output.c_str(), NULL, 100) ) {
		cerr << "Saving the image failed" << endl;
		Client::Application::exit(1);
	}
	else {
		Client::Application::quit();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MapCut::drawCustomLayer(QPainter *p) {
	// Station labels must not be placed below the overlay boxes
	QList<QRect> reserved;

	if ( _legend ) {
		reserved << drawLegend(p, false);
	}

	if ( _infoBox ) {
		reserved << drawInfoBox(p, false);
	}

	if ( _withArrivals ) {
		drawArrivals(p, reserved);
	}

	if ( _legend ) {
		drawLegend(p, true);
	}

	if ( _infoBox ) {
		drawInfoBox(p, true);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MapCut::drawStationSymbol(QPainter *p, const QPoint &pos) const {
	int size = SCScheme.map.stationSize;
	int r = size / 2;

	if ( _stationTriangles ) {
		QPolygon triangle;
		triangle << QPoint(pos.x(), pos.y() - r)
		         << QPoint(pos.x() + r, pos.y() + r)
		         << QPoint(pos.x() - r, pos.y() + r);
		p->drawPolygon(triangle);
	}
	else {
		p->drawEllipse(pos.x() - r, pos.y() - r, size, size);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MapCut::drawArrivals(QPainter *p, QList<QRect> labelRects) {
	Map::Canvas *c = _canvas;

	for ( auto symbol : *c->symbolCollection() ) {
		auto symbol2 = static_cast<OriginSymbol2*>(symbol);
		if ( !symbol2->origin ) {
			continue;
		}

		QPoint originLocation;
		c->projection()->project(
			originLocation, QPointF(symbol->longitude(), symbol->latitude())
		);

		int cutOff = symbol->size().width();
		if ( cutOff ) {
			p->setClipping(true);
			p->setClipRegion(
				QRegion(p->window())
				- QRegion(
					QRect(
						originLocation.x() - cutOff / 2,
						originLocation.y() - cutOff / 2, cutOff, cutOff
					),
					QRegion::Ellipse
				)
			);
		}

		p->setPen(SCScheme.colors.map.lines);
		for ( const auto &entry : symbol2->stations ) {
			c->drawLine(
				*p,
				QPointF(symbol->longitude(), symbol->latitude()),
				QPointF(entry.lon, entry.lat)
			);
		}

		if ( cutOff )
			p->setClipping(false);

		int r = SCScheme.map.stationSize / 2;
		QRect screen = p->window().adjusted(-r, -r, r, r);

		p->setPen(SCScheme.colors.map.outlines);
		for ( auto it = symbol2->stations.rbegin(); it != symbol2->stations.rend(); ++it ) {
			const auto &entry = *it;

			if ( !entry.enabled ) {
				p->setBrush(SCScheme.colors.arrivals.disabled);
			}
			else if ( entry.residual ) {
				p->setBrush(SCScheme.colors.arrivals.residuals.colorAt(*entry.residual));
			}
			else {
				p->setBrush(SCScheme.colors.arrivals.undefined);
			}

			QPoint pp;
			if ( !c->projection()->project(pp, QPointF(entry.lon, entry.lat)) ) {
				continue;
			}

			drawStationSymbol(p, pp);

			if ( _htmlArea && screen.contains(pp) ) {
				cout << "<area shape=\"circle\" coords=\"" << pp.x()
				     << "," << pp.y() << "," << r << "\" "
				     << "id=\"" << entry.pickID << "\"";
				if ( !entry.streamID.empty() ) {
					cout << " title=\"" << entry.streamID << " " << entry.phase;
					if ( entry.residual ) {
						cout << " " << QString::number(*entry.residual, 'f', 2).toStdString() << " s";
					}
					cout << "\"";
				}
				cout << "/>" << endl;
			}
		}

		if ( !_stationLabels ) {
			continue;
		}

		// Labels are placed above the symbol and skipped if they would
		// overlap an already placed label
		p->save();
		for ( const auto &entry : symbol2->stations ) {
			if ( !entry.annotation ) {
				continue;
			}

			QPoint pp;
			if ( !c->projection()->project(pp, QPointF(entry.lon, entry.lat)) ) {
				continue;
			}

			if ( !screen.contains(pp) ) {
				continue;
			}

			entry.annotation->updateLabelRect(*p, QPoint(pp.x(), pp.y() - r));

			bool overlaps = false;
			for ( const QRect &rect : labelRects ) {
				if ( rect.intersects(entry.annotation->labelRect) ) {
					overlaps = true;
					break;
				}
			}

			if ( overlaps ) {
				continue;
			}

			labelRects.append(entry.annotation->labelRect);
			entry.annotation->style->draw(*p, *entry.annotation);
		}
		p->restore();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
QRect MapCut::drawInfoBox(QPainter *p, bool paint) {
	QStringList lines;

	if ( !_title.empty() ) {
		lines << _title.c_str();
	}

	// Origin information is only shown if exactly one symbol is plotted
	OriginSymbol2 *symbol = nullptr;
	if ( _canvas->symbolCollection()->count() == 1 ) {
		symbol = static_cast<OriginSymbol2*>(*_canvas->symbolCollection()->begin());
	}

	if ( symbol && symbol->origin ) {
		auto org = symbol->origin;
		double lat = org->latitude().value();
		double lon = org->longitude().value();

		if ( _title.empty() && symbol->event ) {
			lines << symbol->event->publicID().c_str();
		}

		lines << Regions::getRegionName(lat, lon).c_str();
		lines << org->time().value().toString("%F %T UTC").c_str();

		QString line;
		if ( symbol->magnitude ) {
			try {
				line = QString("M %1 %2  ")
				       .arg(symbol->magnitude->magnitude().value(), 0, 'f', 1)
				       .arg(symbol->magnitude->type().c_str());
			}
			catch ( ... ) {}
		}

		try {
			line += QString("Depth %1 km").arg(org->depth().value(), 0, 'f', 0);
		}
		catch ( ... ) {}

		if ( !line.isEmpty() ) {
			lines << line;
		}

		lines << QString("Lat %1  Lon %2").arg(lat, 0, 'f', 2).arg(lon, 0, 'f', 2);

		line.clear();
		try {
			line = QString("Phases %1").arg(org->quality().usedPhaseCount());
		}
		catch ( ... ) {}

		try {
			if ( !line.isEmpty() ) line += "  ";
			line += QString("RMS %1 s").arg(org->quality().standardError(), 0, 'f', 2);
		}
		catch ( ... ) {}

		if ( !line.trimmed().isEmpty() ) {
			lines << line.trimmed();
		}
	}
	else if ( symbol && commandline().hasOption("lat") ) {
		lines << Regions::getRegionName(_latitude, _longitude).c_str();
		lines << QString("M %1  Depth %2 km")
		         .arg(_magnitude, 0, 'f', 1).arg(_depth, 0, 'f', 0);
		lines << QString("Lat %1  Lon %2")
		         .arg(_latitude, 0, 'f', 2).arg(_longitude, 0, 'f', 2);
	}

	if ( lines.isEmpty() ) {
		return QRect();
	}

	QFont font = p->font();
	font.setPointSize(SCScheme.colors.map.annotations.textSize + 2);
	QFont boldFont = font;
	boldFont.setBold(true);

	QFontMetrics fm(font), bfm(boldFont);
	int em = fm.height();
	int pad = em / 2;
	int width = QT_FM_WIDTH(bfm, lines[0]);
	for ( int i = 1; i < lines.size(); ++i ) {
		width = max(width, QT_FM_WIDTH(fm, lines[i]));
	}

	QRect box(pad, pad, width + 2 * pad, lines.size() * em + 2 * pad);

	if ( !paint ) {
		return box;
	}

	p->save();

	p->setPen(SCScheme.colors.map.annotations.normalBorder);
	p->setBrush(SCScheme.colors.map.annotations.normalBackground);
	p->drawRoundedRect(box, 4, 4);

	p->setPen(SCScheme.colors.map.annotations.normalText.color().lighter(130));
	int y = box.top() + pad;
	for ( int i = 0; i < lines.size(); ++i ) {
		p->setFont(i == 0 ? boldFont : font);
		p->drawText(QRect(box.left() + pad, y, width, em),
		            Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
		            lines[i]);
		y += em;
	}

	p->restore();

	return box;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
QRect MapCut::drawLegend(QPainter *p, bool paint) {
	const Gradient &gradient = SCScheme.colors.arrivals.residuals;
	if ( gradient.isEmpty() ) {
		return QRect();
	}

	qreal minValue = gradient.firstKey();
	qreal maxValue = gradient.lastKey();
	if ( maxValue <= minValue ) {
		return QRect();
	}

	QFont font = p->font();
	font.setPointSize(SCScheme.colors.map.annotations.textSize);

	QFontMetrics fm(font);
	int em = fm.height();
	int pad = em / 2;
	int barWidth = max(160, em * 12);
	int barHeight = em;
	int size = SCScheme.map.stationSize;

	QString caption = "Residual [s]";
	QString unused = "not used";

	int width = barWidth + 2 * pad;
	int height = pad + em + pad / 2 + barHeight + em + pad / 2 + max(em, size) + pad;

	QRect box(pad, _canvas->height() - height - pad, width, height);

	if ( !paint ) {
		return box;
	}

	p->save();
	p->setFont(font);

	p->setPen(SCScheme.colors.map.annotations.normalBorder);
	p->setBrush(SCScheme.colors.map.annotations.normalBackground);
	p->drawRoundedRect(box, 4, 4);

	QPen textPen(SCScheme.colors.map.annotations.normalText.color().lighter(130));
	int x = box.left() + pad;
	int y = box.top() + pad;

	p->setPen(textPen);
	p->drawText(QRect(x, y, barWidth, em), Qt::AlignLeft | Qt::AlignVCenter, caption);
	y += em + pad / 2;

	QLinearGradient linear(x, 0, x + barWidth, 0);
	for ( auto it = gradient.begin(); it != gradient.end(); ++it ) {
		linear.setColorAt((it.key() - minValue) / (maxValue - minValue), it.value().first);
	}

	QRect bar(x, y, barWidth, barHeight);
	p->setPen(SCScheme.colors.map.outlines);
	p->setBrush(linear);
	p->drawRect(bar);
	y += barHeight;

	// Tick labels at the gradient stops, skipping those which would overlap
	p->setPen(textPen);
	int lastRight = x - em;
	for ( auto it = gradient.begin(); it != gradient.end(); ++it ) {
		int tx = x + static_cast<int>((it.key() - minValue) / (maxValue - minValue) * barWidth);
		QString text = QString::number(it.key());
		int tw = QT_FM_WIDTH(fm, text);
		QRect textRect(tx - tw / 2, y, tw, em);

		if ( textRect.left() < x - pad || textRect.right() > x + barWidth + pad ) {
			continue;
		}

		if ( textRect.left() <= lastRight + pad / 2 ) {
			continue;
		}

		p->drawLine(tx, y - barHeight / 4, tx, y);
		p->drawText(textRect, Qt::AlignCenter, text);
		lastRight = textRect.right();
	}
	y += em + pad / 2;

	// Swatch for arrivals not used in the location
	p->setPen(SCScheme.colors.map.outlines);
	p->setBrush(SCScheme.colors.arrivals.disabled);
	drawStationSymbol(p, QPoint(x + size / 2, y + max(em, size) / 2));
	p->setPen(textPen);
	p->drawText(QRect(x + size + pad, y, barWidth - size - pad, max(em, size)),
	            Qt::AlignLeft | Qt::AlignVCenter, unused);

	p->restore();

	return box;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int main(int argc, char ** argv) {
	Application::Type type = Application::Tty;
	// Qt 4.2.x crashes when rendering text with console
	// applications so we enable console application only if
	// at least Qt 4.3.0 is installed.
	if ( !Application::minQtVersion("4.3.0") )
		type = Application::GuiClient;

	MapCut app(argc, argv,type);
	return app();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
