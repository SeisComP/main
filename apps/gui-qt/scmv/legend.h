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


#ifndef __LEGEND_H__
#define __LEGEND_H__

#ifndef Q_MOC_RUN
#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/rtti.h>
#endif
#include <seiscomp/gui/map/legend.h>
#include <seiscomp/gui/map/mapwidget.h>

#ifndef Q_MOC_RUN
#include "types.h"
#endif


class Legend : public Seiscomp::Gui::Map::Legend {
	Q_OBJECT

	private:
		typedef QPair<QString, QChar> Annotation;
		typedef QPair<Annotation, QColor> ContentItem;

	private:
		class Content : public QVector<ContentItem> {
			public:
				void addItem(const QString& text, QColor c) {
					append(ContentItem(Annotation(text, ' '), c));
				}
				void addItem(const QString& text, const QChar& character, QColor c) {
					append(ContentItem(Annotation(text, character), c));
				}

				QSize getSize(const Legend& legend) const;
				void draw(const Legend& legend, QPainter& painter, const QRect& rect);

			public:
				QString title;
		};

	public:
		Legend(QObject *parent);

	public:
		ApplicationStatus::Mode mode() const;
		void setMode(ApplicationStatus::Mode mode);

		virtual void draw(const QRect &rect, QPainter &painter);

	public slots:
		void setParameter(int);

	private:
		void init();
		void drawStation(QPainter& painter, const QPoint& pos, const QColor& color,
		                 const QChar& character, const QString& annotation) const;

		void handleQcStatus(QPainter &painter, const QRect &rect);

	private:
		ApplicationStatus::Mode _mode;
		int                     _stationSize;
		int                     _offset;

		Content                *_currentContent;

		Content                 _qcDelay;
		Content                 _qcLatency;
		Content                 _qcTimingInterval;
		Content                 _qcGapsInterval;
		Content                 _qcGapsLength;
		Content                 _qcSpikesInterval;
		Content                 _qcSpikesAmplitude;
		Content                 _qcOffset;
		Content                 _qcRMS;
		Content                 _event;
		Content                 _gm;
};


#endif
