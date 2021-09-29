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


#ifndef MVSTATIONSYMBOL_H
#define MVSTATIONSYMBOL_H


#include <seiscomp/gui/datamodel/stationsymbol.h>
#include <seiscomp/gui/map/annotations.h>


class MvStationSymbol : public Seiscomp::Gui::StationSymbol {
	public:
		MvStationSymbol(double latitude, double longitude,
		                Seiscomp::Gui::Map::AnnotationItem *annotation,
		                Seiscomp::Gui::Map::Decorator* decorator = nullptr);

	public:
		static void setDrawFullID(bool);
		static void setCharacterDrawingColor(const QColor& color);

		void setCharacter(const QChar& c);

		const std::string &networkCode() const;
		void setNetworkCode(const std::string &networkCode);

		const std::string &stationCode() const;
		void setStationCode(const std::string &stationCode);

		void setPos(const QPoint &p) { _position = p; }

		Seiscomp::Gui::Map::AnnotationItem *annotation() const;
		void updateAnnotation();

	protected:
		void calculateMapPosition(const Seiscomp::Gui::Map::Canvas *canvas) override;
		void customDraw(const Seiscomp::Gui::Map::Canvas *canvas, QPainter& painter) override;

	private:
		void drawCharacter(QPainter& painter);

	private:
		Seiscomp::Gui::Map::AnnotationItem *_annotation;
		std::string                         _networkCode;
		std::string                         _stationCode;

		QChar                               _char;

		static QColor                       _characterDrawingColor;
		static bool                         _drawFullId;
};


inline Seiscomp::Gui::Map::AnnotationItem *MvStationSymbol::annotation() const {
	return _annotation;
}


#endif
