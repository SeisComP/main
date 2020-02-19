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


#ifndef __MVSTATIONSYMBOL_H___
#define __MVSTATIONSYMBOL_H___


#include <seiscomp/gui/datamodel/stationsymbol.h>


class MvStationSymbol : public Seiscomp::Gui::StationSymbol {
	public:
		MvStationSymbol(Seiscomp::Gui::Map::Decorator* decorator = NULL);
		MvStationSymbol(double latitude, double longitude,
		                Seiscomp::Gui::Map::Decorator* decorator = NULL);

	public:
		void setIdDrawingColor(const QColor& color);
		void setIdDrawingEnabled(bool val);
		bool isIdDrawingEnabled() const;
		void setDrawFullID(bool);

		void setCharacter(const QChar& c);
		void setCharacterDrawingColor(const QColor& color);
		void setCharacterDrawingEnabled(bool val);
		bool isCharacterDrawingEnabled() const;

		const std::string& networkCode() const;
		void setNetworkCode(const std::string& networkCode);

		const std::string& stationCode() const;
		void setStationCode(const std::string& stationCode);

		const std::string& locationCode() const;
		void setLocationCode(const std::string& locationCode);

		const std::string& channelCode() const;
		void setChannleCode(const std::string& channelCode);

		void setPos(const QPoint &p) { _position = p; }

	protected:
		virtual void customDraw(const Seiscomp::Gui::Map::Canvas *canvas, QPainter& painter);

	private:
		void init();

		void drawID(QPainter& painter);
		void drawCharacter(QPainter& painter);

	private:
		std::string _networkCode;
		std::string _stationCode;
		std::string _locationCode;
		std::string _channelCode;

		QColor      _idDrawingColor;
		bool        _isDrawingIdEnabled;
		bool        _drawFullId;

		QChar       _char;
		QColor      _characterDrawingColor;
		bool        _isCharacterDrawingEnabled;
};

#endif
