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
#include <seiscomp/geo/boundingbox.h>
#include <seiscomp/gui/core/application.h>
#include <seiscomp/gui/map/projection.h>
#include <QPainter>
#include <QPolygon>
#include <QFont>
#include <QFontMetrics>
#include <QDateTime>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <sstream>

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
void parseTimeRange(const string &str, Core::Time *start, Core::Time *end) {
	size_t pos = str.find(',');
	if ( pos == string::npos ) return;
	
	string startStr = str.substr(0, pos);
	string endStr = str.substr(pos + 1);
	
	if ( start && !startStr.empty() ) {
		if ( !start->fromString(startStr.c_str(), "%Y-%m-%d") ) {
			start->fromString(startStr.c_str(), "%Y-%m-%dT%H:%M:%S");
		}
	}
	
	if ( end && !endStr.empty() ) {
		if ( !end->fromString(endStr.c_str(), "%Y-%m-%d") ) {
			end->fromString(endStr.c_str(), "%Y-%m-%dT%H:%M:%S");
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void setupView(Map::Canvas *canvas, const QRectF &rect) {
	canvas->displayRect(rect);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
class OriginSymbol2 : public OriginSymbol {
	using OriginSymbol::OriginSymbol;

	public:
		DataModel::Origin *origin{nullptr};
		std::string magnitudeType;
		std::string eventID;
		Core::Time originTime;
		double magnitudeValue{0.0};
};

MapCut::MapCut(int& argc, char **argv, Application::Type type)
: Application(argc, argv, 0, type) {
	setMessagingEnabled(false);
	setDatabaseEnabled(false, false);
	setRecordStreamEnabled(false);
	setLoadRegionsEnabled(true);

	_dim = QSize(0,0);
	_depth = 10;
	_magnitude = 0;
	_margins = QSizeF(0,0);
	_showMagnitudeInfo = false;
	_triangleStations = false;
	_showScale = false;
	_showStationCodes = false;
	_distanceRings = false;
	_depthColors = false;
	_eventLabels = false;
	_minMagnitude = -10.0;
	_maxMagnitude = 10.0;
	_eventParameters = nullptr;
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
QColor MapCut::getMagnitudeColor(double magnitude) {
	if ( magnitude < 2.0 ) return QColor(0, 255, 0);
	if ( magnitude < 3.0 ) return QColor(128, 255, 0);
	if ( magnitude < 4.0 ) return QColor(255, 255, 0);
	if ( magnitude < 5.0 ) return QColor(255, 128, 0);
	if ( magnitude < 6.0 ) return QColor(255, 0, 0);
	if ( magnitude < 7.0 ) return QColor(128, 0, 128);
	return QColor(64, 0, 64);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
QColor MapCut::getDepthColor(double depth) {
	if ( depth < 35 ) return QColor(255, 0, 0);
	if ( depth < 70 ) return QColor(255, 165, 0);
	if ( depth < 300 ) return QColor(255, 255, 0);
	if ( depth < 500 ) return QColor(0, 255, 0);
	return QColor(0, 0, 255);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int MapCut::getMagnitudeRadius(double magnitude) {
	int baseSize = 6;
	int scaledSize = static_cast<int>(baseSize + magnitude * 2);
	return max(4, min(scaledSize, 30));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool MapCut::isEventInTimeRange(const Core::Time &eventTime) {
	if ( _timeRange.empty() ) return true;
	return eventTime >= _timeStart && eventTime <= _timeEnd;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool MapCut::isEventInMagnitudeRange(double magnitude) {
	return magnitude >= _minMagnitude && magnitude <= _maxMagnitude;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
std::string MapCut::extractStationCode(DataModel::Arrival *arr, DataModel::EventParameters *ep) {
	std::string stationCode;
	std::string networkCode;
	
	try {
		// Method 1: Try to get pick and extract network.station from waveform ID
		if (ep) {
			DataModel::Pick *pick = ep->findPick(arr->pickID());
			if (pick) {
				try {
					networkCode = pick->waveformID().networkCode();
					stationCode = pick->waveformID().stationCode();
					
					if (!stationCode.empty()) {
						// Clean and format codes
						std::transform(networkCode.begin(), networkCode.end(), networkCode.begin(), ::toupper);
						std::transform(stationCode.begin(), stationCode.end(), stationCode.begin(), ::toupper);
						
						// Create network.station format
						if (!networkCode.empty() && networkCode != "XX" && networkCode != "--") {
							std::string fullCode = networkCode + "." + stationCode;
							if (fullCode.length() <= 10) {  // Reasonable length limit
								return fullCode;
							}
						}
						
						// Fallback to just station if network+station too long
						if (stationCode.length() <= 8) {
							return stationCode;
						}
					}
				} catch (...) {}
			}
		}
		
		// Method 2: Parse pick ID for standard network.station.location.channel format
		std::string pickID = arr->pickID();
		size_t firstDot = pickID.find('.');
		if (firstDot != std::string::npos) {
			size_t secondDot = pickID.find('.', firstDot + 1);
			if (secondDot != std::string::npos) {
				// Extract network and station
				networkCode = pickID.substr(0, firstDot);
				stationCode = pickID.substr(firstDot + 1, secondDot - firstDot - 1);
				
				if (!networkCode.empty() && !stationCode.empty()) {
					std::transform(networkCode.begin(), networkCode.end(), networkCode.begin(), ::toupper);
					std::transform(stationCode.begin(), stationCode.end(), stationCode.begin(), ::toupper);
					
					// Create network.station format
					std::string fullCode = networkCode + "." + stationCode;
					if (fullCode.length() <= 10 && networkCode != "XX" && networkCode != "--") {
						return fullCode;
					}
					
					// Fallback to just station
					if (stationCode.length() <= 8) {
						return stationCode;
					}
				}
			} else {
				// Only one dot - could be network.station
				networkCode = pickID.substr(0, firstDot);
				stationCode = pickID.substr(firstDot + 1);
				
				if (!networkCode.empty() && !stationCode.empty()) {
					std::transform(networkCode.begin(), networkCode.end(), networkCode.begin(), ::toupper);
					std::transform(stationCode.begin(), stationCode.end(), stationCode.begin(), ::toupper);
					
					// If both parts look reasonable, use network.station
					if (networkCode.length() <= 4 && stationCode.length() <= 6 && 
					    networkCode != "PICK" && networkCode != "XX") {
						std::string fullCode = networkCode + "." + stationCode;
						if (fullCode.length() <= 10) {
							return fullCode;
						}
					}
					
					// Otherwise try just the station part
					if (stationCode.length() <= 8) {
						return stationCode;
					} else if (networkCode.length() <= 8) {
						return networkCode;
					}
				}
			}
		}
		
		// Method 3: Handle Pick/timestamp format by using distance/azimuth to create identifier
		if (pickID.find("Pick/") == 0) {
			try {
				double distance = arr->distance();
				double azimuth = arr->azimuth();
				int distKm = static_cast<int>(distance * 111.0);
				
				std::string dirCode;
				if (azimuth < 22.5 || azimuth >= 337.5) dirCode = "N";
				else if (azimuth < 67.5) dirCode = "NE";
				else if (azimuth < 112.5) dirCode = "E"; 
				else if (azimuth < 157.5) dirCode = "SE";
				else if (azimuth < 202.5) dirCode = "S";
				else if (azimuth < 247.5) dirCode = "SW";
				else if (azimuth < 292.5) dirCode = "W";
				else dirCode = "NW";
				
				// For Pick/timestamp format, create a network-like identifier
				std::string pseudoNetwork = "ST";  // "Station" prefix
				std::string pseudoStation;
				
				if (distKm < 100) {
					pseudoStation = dirCode + std::to_string(distKm);
				} else {
					pseudoStation = dirCode + std::to_string(distKm / 10) + "0";
				}
				
				std::string fullCode = pseudoNetwork + "." + pseudoStation;
				if (fullCode.length() <= 10) {
					return fullCode;
				} else if (pseudoStation.length() <= 6) {
					return pseudoStation;
				}
			} catch (...) {}
		}
		
		// Method 4: Enhanced fallback parsing with network detection
		std::string cleanPickID;
		for (char c : pickID) {
			if (std::isalnum(c)) {
				cleanPickID += c;
			} else if (c == '.' || c == '_' || c == '-') {
				cleanPickID += '_';
			}
		}
		
		std::vector<std::string> segments;
		std::stringstream ss(cleanPickID);
		std::string segment;
		
		while (std::getline(ss, segment, '_')) {
			if (!segment.empty() && segment.length() >= 2 && segment.length() <= 8) {
				bool isTimestamp = (segment.length() > 6 && std::all_of(segment.begin(), segment.end(), ::isdigit));
				if (!isTimestamp) {
					segments.push_back(segment);
				}
			}
		}
		
		if (!segments.empty()) {
			// Sort segments to prefer network-like codes
			std::sort(segments.begin(), segments.end(), [](const std::string& a, const std::string& b) {
				// Prefer 2-3 character segments (typical network codes)
				bool aIsNetworkLike = (a.length() >= 2 && a.length() <= 3);
				bool bIsNetworkLike = (b.length() >= 2 && b.length() <= 3);
				
				if (aIsNetworkLike != bIsNetworkLike) return aIsNetworkLike;
				
				// Then prefer shorter segments
				if (a.length() != b.length()) return a.length() < b.length();
				
				// Prefer segments with letters over pure numbers
				bool aHasLetter = std::any_of(a.begin(), a.end(), ::isalpha);
				bool bHasLetter = std::any_of(b.begin(), b.end(), ::isalpha);
				if (aHasLetter != bHasLetter) return aHasLetter;
				
				return a < b;
			});
			
			// Try to create network.station from multiple segments
			if (segments.size() >= 2) {
				std::string possibleNetwork = segments[0];
				std::string possibleStation = segments[1];
				
				// Check if first segment could be a network code
				if (possibleNetwork.length() <= 4 && possibleStation.length() <= 6) {
					std::string combinedCode = possibleNetwork + "." + possibleStation;
					if (combinedCode.length() <= 10) {
						std::transform(combinedCode.begin(), combinedCode.end(), combinedCode.begin(), ::toupper);
						return combinedCode;
					}
				}
			}
			
			// Fallback to best single segment
			stationCode = segments[0];
		} else {
			// Last resort - use first part of pickID
			stationCode = pickID.substr(0, min(size_t(8), pickID.length()));
		}
		
	} catch (...) {
		stationCode = "UNK";
	}
	
	// Final cleanup for single station code
	std::string finalCode;
	for (char c : stationCode) {
		if (std::isalnum(c)) {
			finalCode += std::toupper(c);
		}
	}
	
	// Ensure reasonable length
	if (finalCode.length() > 8) {
		finalCode = finalCode.substr(0, 8);
	}
	
	return finalCode.empty() ? "STA" : finalCode;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MapCut::createCommandLineDescription() {
	commandline().addGroup("Options");
	commandline().addOption("Options", "region,r", "cut region ([lat_dim]x[lon_dim]+lat0+lon0 or +lat0+lon+lat1+lon1)", &_region, false);
	commandline().addOption("Options", "margin,m", "margin in degrees around origin (margin|margin_latxmargin_lon)", &_margin, false);
	commandline().addOption("Options", "dimension,d", "output image dimension (wxh)", &_dimension, false);
	commandline().addOption("Options", "output,o", "output image", &_output, false);
	commandline().addOption("Options", "lat", "latitude of symbol", &_latitude, false);
	commandline().addOption("Options", "lon", "longitude of symbol", &_longitude, false);
	commandline().addOption("Options", "depth", "depth of event", &_depth, false);
	commandline().addOption("Options", "mag", "magnitude of event", &_magnitude, false);
	commandline().addOption("Options", "layers", "draw polygonal layers");
	commandline().addOption("Options", "ep", "EventParameters (XML) to load", &_epFile, false);
	commandline().addOption("Options", "event-id,E", "event to plot", &_eventID, false);
	commandline().addOption("Options", "without-arrivals", "do not render arrivals / stations");
	commandline().addOption("Options", "html-area", "print html/area section");
	
	// Enhanced visualization options
	commandline().addOption("Options", "show-magnitude-info", "display magnitude and type information on map");
	commandline().addOption("Options", "triangle-stations", "use upside-down triangles for stations instead of circles");
	commandline().addOption("Options", "show-scale", "display magnitude scale legend");
	commandline().addOption("Options", "show-station-codes", "display station codes near symbols");
	commandline().addOption("Options", "distance-rings", "draw distance rings around epicenter");
	commandline().addOption("Options", "depth-colors", "use depth-based coloring for events");
	commandline().addOption("Options", "event-labels", "show event IDs as labels");
	
	// Filtering options
	commandline().addOption("Options", "time-range", "time range filter (YYYY-MM-DD,YYYY-MM-DD)", &_timeRange, false);
	commandline().addOption("Options", "min-magnitude", "minimum magnitude to display", &_minMagnitude, false);
	commandline().addOption("Options", "max-magnitude", "maximum magnitude to display", &_maxMagnitude, false);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool MapCut::run() {
	SCScheme.map.vectorLayerAntiAlias = true;
	DataModel::EventParametersPtr ep;

	if ( commandline().hasOption("without-arrivals") ) {
		_withArrivals = false;
	}

	_htmlArea = commandline().hasOption("html-area");
	_showMagnitudeInfo = commandline().hasOption("show-magnitude-info");
	_triangleStations = commandline().hasOption("triangle-stations");
	_showScale = commandline().hasOption("show-scale");
	_showStationCodes = commandline().hasOption("show-station-codes");
	_distanceRings = commandline().hasOption("distance-rings");
	_depthColors = commandline().hasOption("depth-colors");
	_eventLabels = commandline().hasOption("event-labels");

	if ( !_timeRange.empty() ) {
		parseTimeRange(_timeRange, &_timeStart, &_timeEnd);
	}

	Map::ImageTreePtr mapTree = new Map::ImageTree(mapsDesc());
	_canvas = new Map::Canvas(mapTree.get());
	_canvas->setParent(this);

	OPT(double) centerLat, centerLon;

	if ( !_epFile.empty() ) {
		IO::XMLArchive ar;
		if ( !ar.open(_epFile.c_str()) ) {
			cerr << "Unable to open file '" << _epFile << "'" << endl;
			return false;
		}

		ar >> ep;

		if ( !ep ) {
			cerr << "File '" << _epFile << "' does not contain event parameters" << endl;
			return false;
		}
		
		_eventParameters = ep.get();
	}

	if ( !_eventID.empty() ) {
		if ( !ep ) {
			cerr << "No event parameters available, see --ep" << endl;
			return false;
		}

		DataModel::Event *evt = ep->findEvent(_eventID);
		if ( !evt ) {
			cerr << "Event '" << _eventID << "' not found" << endl;
			return false;
		}

		DataModel::Origin *org{nullptr};
		for ( size_t i = 0; i < ep->originCount(); ++i ) {
			if ( ep->origin(i)->publicID() == evt->preferredOriginID() ) {
				org = ep->origin(i);
				break;
			}
		}

		if ( !org ) {
			cerr << "Preferred origin for event '" << _eventID << "' not found" << endl;
			return false;
		}

		bool includeEvent = true;
		double eventMag = _magnitude;
		
		try {
			if ( !isEventInTimeRange(org->time().value()) ) {
				includeEvent = false;
			}
		} catch ( ... ) {}

		if ( includeEvent && !evt->preferredMagnitudeID().empty() ) {
			for ( size_t i = 0; i < org->magnitudeCount(); ++i ) {
				if ( org->magnitude(i)->publicID() == evt->preferredMagnitudeID() ) {
					try { 
						eventMag = org->magnitude(i)->magnitude().value(); 
						if ( !isEventInMagnitudeRange(eventMag) ) {
							includeEvent = false;
						}
					}
					catch ( ... ) {}
					break;
				}
			}
		}

		if ( includeEvent ) {
			auto symbol = new OriginSymbol2(org->latitude(), org->longitude());
			symbol->setDepth(org->depth().value());
			symbol->origin = org;
			symbol->eventID = _eventID;
			symbol->originTime = org->time().value();
			symbol->magnitudeValue = eventMag;

			int symbolSize = getMagnitudeRadius(eventMag);
			symbol->setSize(QSize(symbolSize * 2, symbolSize * 2));

			if ( !evt->preferredMagnitudeID().empty() ) {
				for ( size_t i = 0; i < org->magnitudeCount(); ++i ) {
					if ( org->magnitude(i)->publicID() == evt->preferredMagnitudeID() ) {
						try { 
							eventMag = org->magnitude(i)->magnitude().value(); 
							symbol->magnitudeType = org->magnitude(i)->type();
							symbol->magnitudeValue = eventMag;
						}
						catch ( ... ) {}
						break;
					}
				}
			}
			symbol->setPreferredMagnitudeValue(eventMag);

			if ( _depthColors ) {
				try {
					QColor depthColor = getDepthColor(org->depth().value());
					symbol->setColor(depthColor);
				} catch ( ... ) {}
			}

			_canvas->symbolCollection()->add(symbol);
		}

		centerLat = org->latitude().value();
		centerLon = org->longitude().value();
	}
	else if ( ep ) {
		OPT(Geo::GeoBoundingBox) bbox;

		for ( size_t i = 0; i < ep->eventCount(); ++i ) {
			auto evt = ep->event(i);
			DataModel::Origin *org{nullptr};

			for ( size_t j = 0; j < ep->originCount(); ++j ) {
				if ( ep->origin(j)->publicID() == evt->preferredOriginID() ) {
					org = ep->origin(j);
					break;
				}
			}

			if ( !org ) {
				cerr << "Preferred origin for event '" << evt->publicID() << "' not found" << endl;
				continue;
			}

			bool includeEvent = true;
			double eventMag = _magnitude;

			try {
				if ( !isEventInTimeRange(org->time().value()) ) {
					includeEvent = false;
				}
			} catch ( ... ) {}

			if ( includeEvent && !evt->preferredMagnitudeID().empty() ) {
				for ( size_t j = 0; j < org->magnitudeCount(); ++j ) {
					if ( org->magnitude(j)->publicID() == evt->preferredMagnitudeID() ) {
						try { 
							eventMag = org->magnitude(j)->magnitude().value(); 
							if ( !isEventInMagnitudeRange(eventMag) ) {
								includeEvent = false;
							}
						}
						catch ( ... ) {}
						break;
					}
				}
			}

			if ( includeEvent ) {
				auto symbol = new OriginSymbol2(org->latitude(), org->longitude());
				symbol->setDepth(org->depth().value());
				symbol->origin = org;
				symbol->eventID = evt->publicID();
				symbol->originTime = org->time().value();
				symbol->magnitudeValue = eventMag;

				int symbolSize = getMagnitudeRadius(eventMag);
				symbol->setSize(QSize(symbolSize * 2, symbolSize * 2));

				if ( !evt->preferredMagnitudeID().empty() ) {
					for ( size_t j = 0; j < org->magnitudeCount(); ++j ) {
						if ( org->magnitude(j)->publicID() == evt->preferredMagnitudeID() ) {
							try { 
								eventMag = org->magnitude(j)->magnitude().value(); 
								symbol->magnitudeType = org->magnitude(j)->type();
								symbol->magnitudeValue = eventMag;
							}
							catch ( ... ) {}
							break;
						}
					}
				}
				symbol->setPreferredMagnitudeValue(eventMag);

				if ( _depthColors ) {
					try {
						QColor depthColor = getDepthColor(org->depth().value());
						symbol->setColor(depthColor);
					} catch ( ... ) {}
				}

				_canvas->symbolCollection()->add(symbol);

				if ( !bbox ) {
					bbox = Geo::GeoBoundingBox(
						org->latitude().value(), org->longitude().value(),
						org->latitude().value(), org->longitude().value()
					);
				}
				else {
					*bbox += Geo::GeoBoundingBox(
						org->latitude().value(), org->longitude().value(),
						org->latitude().value(), org->longitude().value()
					);
				}
			}
		}

		if ( bbox ) {
			centerLat = bbox->center().latitude();
			centerLon = bbox->center().longitude();
		}
	}

	if ( commandline().hasOption("lat") && commandline().hasOption("lon") ) {
		if ( _canvas->symbolCollection()->count() == 0 ) {
			if ( isEventInMagnitudeRange(_magnitude) ) {
				auto symbol = new OriginSymbol2(_latitude, _longitude);
				symbol->setDepth(DataModel::RealQuantity(_depth));
				symbol->setPreferredMagnitudeValue(_magnitude);
				symbol->magnitudeType = "ML";
				symbol->eventID = "manual";
				symbol->magnitudeValue = _magnitude;

				int symbolSize = getMagnitudeRadius(_magnitude);
				symbol->setSize(QSize(symbolSize * 2, symbolSize * 2));

				if ( _depthColors ) {
					QColor depthColor = getDepthColor(_depth);
					symbol->setColor(depthColor);
				}

				_canvas->symbolCollection()->add(symbol);
			}
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
			cerr << "No region and no margins given" << endl;
			return false;
		}

		_reg = QRectF(*centerLon - _margins.width(),
		              *centerLat - _margins.height(),
		              _margins.width() * 2, _margins.height() * 2);
	}

	setupView(_canvas, QRectF(_reg.left(),_reg.top(),_reg.width(),_reg.height()));

	_canvas->setPreviewMode(false);

	if ( commandline().hasOption("layers") ) {
		_canvas->setDrawLayers(true);
	}

	// Connect custom drawing slots
	if ( _withArrivals ) {
		connect(_canvas, SIGNAL(customLayer(QPainter*)),
		        this, SLOT(drawArrivals(QPainter*)));
	}

	if ( _showMagnitudeInfo ) {
		connect(_canvas, SIGNAL(customLayer(QPainter*)),
		        this, SLOT(drawMagnitudeInfo(QPainter*)));
	}

	if ( _showScale ) {
		connect(_canvas, SIGNAL(customLayer(QPainter*)),
		        this, SLOT(drawMagnitudeScale(QPainter*)));
	}

	if ( _distanceRings ) {
		connect(_canvas, SIGNAL(customLayer(QPainter*)),
		        this, SLOT(drawDistanceRings(QPainter*)));
	}

	if ( _eventLabels ) {
		connect(_canvas, SIGNAL(customLayer(QPainter*)),
		        this, SLOT(drawEventLabels(QPainter*)));
	}

	if ( _htmlArea ) {
		connect(_canvas, SIGNAL(customLayer(QPainter*)),
		        this, SLOT(generateEnhancedHtmlAreas(QPainter*)));
	}

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
void MapCut::drawMagnitudeScale(QPainter *p) {
	QRect legendRect(20, 20, 200, 250);
	
	p->setPen(QPen(Qt::black, 2));
	p->setBrush(QColor(255, 255, 255, 230));
	p->drawRect(legendRect);
	
	QFont titleFont = p->font();
	titleFont.setPointSize(11);
	titleFont.setBold(true);
	p->setFont(titleFont);
	p->setPen(Qt::black);
	p->drawText(25, 40, "Magnitude Scale");
	
	QFont itemFont = p->font();
	itemFont.setPointSize(9);
	itemFont.setBold(false);
	p->setFont(itemFont);
	
	for (int i = 1; i <= 7; i++) {
		int radius = getMagnitudeRadius(i);
		int y = 50 + i * 22;
		
		QColor symbolColor;
		if ( _depthColors ) {
			symbolColor = getDepthColor(10);
		} else {
			symbolColor = getMagnitudeColor(i);
		}
		
		p->setBrush(symbolColor);
		p->setPen(QPen(Qt::black, 1));
		p->drawEllipse(35, y, radius * 2, radius * 2);
		p->setPen(Qt::black);
		p->drawText(75, y + 6, QString("M %1").arg(i));
	}
	
	if ( _depthColors ) {
		p->drawText(25, 200, "Depth Colors:");
		
		QFont depthFont = p->font();
		depthFont.setPointSize(8);
		p->setFont(depthFont);
		
		std::vector<std::pair<QString, QColor>> depthInfo = {
			{"< 35 km", QColor(255, 0, 0)},
			{"35-70 km", QColor(255, 165, 0)},
			{"70-300 km", QColor(255, 255, 0)},
			{"> 300 km", QColor(0, 255, 0)}
		};
		
		for (size_t i = 0; i < depthInfo.size(); i++) {
			int y = 210 + i * 15;
			p->setBrush(depthInfo[i].second);
			p->setPen(QPen(Qt::black, 1));
			p->drawRect(25, y, 12, 10);
			p->setPen(Qt::black);
			p->drawText(42, y + 8, depthInfo[i].first);
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MapCut::drawMagnitudeInfo(QPainter *p) {
	Map::Canvas *c = (Map::Canvas*)sender();

	QFont font = p->font();
	font.setPointSize(10);
	font.setBold(true);
	p->setFont(font);

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	QFontMetrics fm(font);
	auto getTextWidth = [&](const QString &text) { return fm.horizontalAdvance(text); };
#else
	QFontMetrics fm(font);
	auto getTextWidth = [&](const QString &text) { return fm.width(text); };
#endif

	for ( auto symbol : *c->symbolCollection() ) {
		auto originSymbol = static_cast<OriginSymbol2*>(symbol);
		if ( !originSymbol->origin ) {
			continue;
		}

		QPoint originLocation;
		c->projection()->project(
			originLocation, QPointF(symbol->longitude(), symbol->latitude())
		);

		QString magText;
		QString magType = QString::fromStdString(originSymbol->magnitudeType);
		if ( magType.isEmpty() ) magType = "M";
		magText = QString("%1 %2").arg(magType).arg(originSymbol->magnitudeValue, 0, 'f', 1);

		int textWidth = getTextWidth(magText);
		int textHeight = fm.height();
		int symbolRadius = symbol->size().width() / 2;
		
		QPoint textPos(
			originLocation.x() - textWidth / 2,
			originLocation.y() - symbolRadius - 8
		);

		QRect textRect(textPos.x() - 3, textPos.y() - textHeight + fm.descent(),
		               textWidth + 6, textHeight + 2);
		
		p->setPen(Qt::NoPen);
		p->setBrush(QColor(255, 255, 255, 200));
		p->drawRect(textRect);
		
		p->setPen(QPen(Qt::black, 1));
		p->setBrush(Qt::NoBrush);
		p->drawRect(textRect);

		p->setPen(Qt::black);
		p->drawText(textPos, magText);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MapCut::drawDistanceRings(QPainter *p) {
	Map::Canvas *c = (Map::Canvas*)sender();
	
	for ( auto symbol : *c->symbolCollection() ) {
		QPoint originLocation;
		c->projection()->project(
			originLocation, QPointF(symbol->longitude(), symbol->latitude())
		);
		
		p->setPen(QPen(Qt::gray, 1, Qt::DashLine));
		p->setBrush(Qt::NoBrush);
		
		std::vector<double> distances = {50, 100, 200, 500};
		
		for ( double dist : distances ) {
			double degreeRadius = dist / 111.0;
			
			QPoint topLeft, bottomRight;
			bool validProjection = true;
			
			validProjection &= c->projection()->project(
				topLeft, 
				QPointF(symbol->longitude() - degreeRadius, symbol->latitude() + degreeRadius)
			);
			validProjection &= c->projection()->project(
				bottomRight, 
				QPointF(symbol->longitude() + degreeRadius, symbol->latitude() - degreeRadius)
			);
			
			if ( validProjection ) {
				int screenRadius = abs(bottomRight.x() - topLeft.x()) / 2;
				if ( screenRadius > 10 && screenRadius < 2000 ) {
					p->drawEllipse(originLocation.x() - screenRadius, originLocation.y() - screenRadius,
					               screenRadius * 2, screenRadius * 2);
					
					QFont labelFont = p->font();
					labelFont.setPointSize(8);
					labelFont.setBold(false);
					p->setFont(labelFont);
					p->setPen(QPen(Qt::darkGray, 1));
					
					int labelX = originLocation.x() + (int)(screenRadius * 0.7);
					int labelY = originLocation.y() - (int)(screenRadius * 0.7);
					
					QString distLabel = QString("%1km").arg(dist);
					QFontMetrics labelFm(labelFont);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
					int labelWidth = labelFm.horizontalAdvance(distLabel);
#else
					int labelWidth = labelFm.width(distLabel);
#endif
					QRect labelRect(labelX - 2, labelY - labelFm.ascent(), labelWidth + 4, labelFm.height());
					
					p->setPen(Qt::NoPen);
					p->setBrush(QColor(255, 255, 255, 180));
					p->drawRect(labelRect);
					
					p->setPen(QPen(Qt::darkGray, 1));
					p->drawText(labelX, labelY, distLabel);
					p->setPen(QPen(Qt::gray, 1, Qt::DashLine));
				}
			}
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MapCut::drawEventLabels(QPainter *p) {
	Map::Canvas *c = (Map::Canvas*)sender();
	
	QFont font = p->font();
	font.setPointSize(8);
	font.setBold(false);
	p->setFont(font);
	QFontMetrics fm(font);
	
	for ( auto symbol : *c->symbolCollection() ) {
		auto originSymbol = static_cast<OriginSymbol2*>(symbol);
		if ( originSymbol->eventID.empty() ) continue;
		
		QPoint originLocation;
		c->projection()->project(
			originLocation, QPointF(symbol->longitude(), symbol->latitude())
		);
		
		std::string eventID = originSymbol->eventID;
		size_t lastDot = eventID.find_last_of('.');
		size_t lastSlash = eventID.find_last_of('/');
		size_t lastSep = max(lastDot, lastSlash);
		
		if ( lastSep != std::string::npos && lastSep < eventID.length() - 1 ) {
			eventID = eventID.substr(lastSep + 1);
		}
		
		if ( eventID.length() > 12 ) {
			eventID = eventID.substr(0, 9) + "...";
		}
		
		QString labelText = QString::fromStdString(eventID);
		
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
		int textWidth = fm.horizontalAdvance(labelText);
#else
		int textWidth = fm.width(labelText);
#endif
		int symbolRadius = symbol->size().width() / 2;
		
		QPoint textPos(
			originLocation.x() - textWidth / 2,
			originLocation.y() + symbolRadius + 18
		);
		
		QRect textRect(textPos.x() - 2, textPos.y() - fm.ascent(),
		               textWidth + 4, fm.height());
		
		p->setPen(Qt::NoPen);
		p->setBrush(QColor(255, 255, 255, 190));
		p->drawRect(textRect);
		
		p->setPen(QPen(Qt::black, 1));
		p->setBrush(Qt::NoBrush);
		p->drawRect(textRect);
		
		p->setPen(Qt::black);
		p->drawText(textPos, labelText);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MapCut::generateEnhancedHtmlAreas(QPainter *p) {
	Map::Canvas *c = (Map::Canvas*)sender();
	
	for ( auto symbol : *c->symbolCollection() ) {
		auto originSymbol = static_cast<OriginSymbol2*>(symbol);
		if ( !originSymbol->origin ) continue;
		
		QPoint originLocation;
		c->projection()->project(
			originLocation, QPointF(symbol->longitude(), symbol->latitude())
		);
		
		int symbolRadius = symbol->size().width() / 2;
		
		cout << "<area shape=\"circle\" coords=\"" 
		     << originLocation.x() << "," << originLocation.y() << "," << symbolRadius 
		     << "\" title=\"";
		
		cout << "Event: " << originSymbol->eventID << "; ";
		
		if ( !originSymbol->magnitudeType.empty() ) {
			cout << "Magnitude: " << originSymbol->magnitudeType << " " << originSymbol->magnitudeValue << "; ";
		} else {
			cout << "Magnitude: " << originSymbol->magnitudeValue << "; ";
		}
		
		try {
			cout << "Depth: " << originSymbol->origin->depth().value() << " km; ";
		} catch ( ... ) {
			cout << "Depth: unknown; ";
		}
		
		try {
			cout << "Time: " << originSymbol->originTime.toString("%Y-%m-%d %H:%M:%S UTC") << "; ";
		} catch ( ... ) {
			cout << "Time: unknown; ";
		}
		
		cout << "Location: " << symbol->latitude() << "°, " << symbol->longitude() << "°";
		cout << "\" id=\"origin_" << originSymbol->eventID << "\"/>" << endl;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MapCut::drawArrivals(QPainter *p) {
	Map::Canvas *c = (Map::Canvas*)sender();

	for ( auto symbol : *c->symbolCollection() ) {
		auto org = static_cast<OriginSymbol2*>(symbol)->origin;
		if ( !org ) {
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

		p->setPen(QPen(SCScheme.colors.map.lines, 1));
		for ( size_t i = 0; i < org->arrivalCount(); ++i ) {
			DataModel::Arrival *arr = org->arrival(i);

			try {
				double lat, lon;
				Math::Geo::delandaz2coord(
					arr->distance(), arr->azimuth(),
					org->latitude(), org->longitude(),
					&lat, &lon
				);

				c->drawLine(
					*p,
					QPointF(symbol->longitude(), symbol->latitude()),
					QPointF(lon, lat)
				);
			}
			catch ( ... ) {
			}
		}

		if ( cutOff )
			p->setClipping(false);

		int r = SCScheme.map.stationSize / 2;
		QRect screen = p->window().adjusted(-r, -r, r, r);

		p->setPen(QPen(SCScheme.colors.map.outlines, 1));
		for ( int i = static_cast<int>(org->arrivalCount()) - 1; i >= 0; --i ) {
			DataModel::Arrival *arr = org->arrival(i);

			try {
				double lat, lon;
				Math::Geo::delandaz2coord(
					arr->distance(), arr->azimuth(),
					org->latitude(), org->longitude(),
					&lat, &lon
				);

				bool enabled;
				try {
					enabled = arr->weight() > 0;
				}
				catch ( ... ) {
					enabled = true;
				}

				if ( enabled ) {
					try {
						p->setBrush(SCScheme.colors.arrivals.residuals.colorAt(
						arr->timeResidual()));
					}
					catch ( ... ) {
						p->setBrush(SCScheme.colors.arrivals.undefined);
					}
				}
				else {
					p->setBrush(SCScheme.colors.arrivals.disabled);
				}

				QPoint pp;
				if ( c->projection()->project(pp, QPointF(lon, lat)) ) {
					if ( _triangleStations ) {
						int triangleSize = SCScheme.map.stationSize;
						QPolygon triangle;
						triangle << QPoint(pp.x(), pp.y() + triangleSize/2)
						         << QPoint(pp.x() - triangleSize/2, pp.y() - triangleSize/2)
						         << QPoint(pp.x() + triangleSize/2, pp.y() - triangleSize/2);
						
						p->drawPolygon(triangle);
					}
					else {
						p->drawEllipse(
							pp.x() - r, pp.y() - r,
							SCScheme.map.stationSize,
							SCScheme.map.stationSize
						);
					}

					if ( _showStationCodes ) {
						std::string stationCode = extractStationCode(arr, _eventParameters);
						
						if ( !stationCode.empty() ) {
							QFont stationFont = p->font();
							stationFont.setPointSize(7);
							stationFont.setBold(true);
							p->setFont(stationFont);
							
							int textX = pp.x() + (_triangleStations ? 10 : 12);
							int textY = pp.y() - 3;
							
							QString stationText = QString::fromStdString(stationCode);
							QFontMetrics stationFm(stationFont);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
							int stationWidth = stationFm.horizontalAdvance(stationText);
#else
							int stationWidth = stationFm.width(stationText);
#endif
							QRect stationRect(textX - 1, textY - stationFm.ascent(), 
							                 stationWidth + 2, stationFm.height());
							
							p->setPen(Qt::NoPen);
							p->setBrush(QColor(255, 255, 255, 220));
							p->drawRect(stationRect);
							
							p->setPen(QPen(Qt::black, 1));
							p->drawText(textX, textY, stationText);
						}
					}

					if ( _htmlArea && screen.contains(pp) ) {
						std::string stationCode = extractStationCode(arr, _eventParameters);
						
						if ( _triangleStations ) {
							cout << "<area shape=\"poly\" coords=\"";
							cout << pp.x() << "," << (pp.y() + SCScheme.map.stationSize/2) << ",";
							cout << (pp.x() - SCScheme.map.stationSize/2) << "," << (pp.y() - SCScheme.map.stationSize/2) << ",";
							cout << (pp.x() + SCScheme.map.stationSize/2) << "," << (pp.y() - SCScheme.map.stationSize/2);
							cout << "\" title=\"Station: " << stationCode << "; ";
							
							try {
								cout << "Distance: " << arr->distance() << "°; ";
								cout << "Azimuth: " << arr->azimuth() << "°; ";
								cout << "Residual: " << arr->timeResidual() << "s";
							} catch ( ... ) {}
							
							cout << "\" id=\"" << arr->pickID() << "\"/>" << endl;
						}
						else {
							cout << "<area shape=\"circle\" coords=\"" << pp.x()
							     << "," << pp.y() << "," << r << "\" "
							     << "title=\"Station: " << stationCode << "; ";
							
							try {
								cout << "Distance: " << arr->distance() << "°; ";
								cout << "Azimuth: " << arr->azimuth() << "°; ";
								cout << "Residual: " << arr->timeResidual() << "s";
							} catch ( ... ) {}
							
							cout << "\" id=\"" << arr->pickID() << "\"/>" << endl;
						}
					}
				}
			}
			catch ( ... ) {	}
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int main(int argc, char ** argv) {
	Application::Type type = Application::Tty;
	if ( !Application::minQtVersion("4.3.0") )
		type = Application::GuiClient;

	MapCut app(argc, argv,type);
	return app();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<