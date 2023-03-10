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


#ifndef __TRACEWIDGET_H__
#define __TRACEWIDGET_H__

#include <QtGui>
#include <QFrame>

namespace Seiscomp {
namespace Gui {

class TraceWidget : public QFrame {
	Q_OBJECT

	public:
		TraceWidget(QWidget *parent = 0, Qt::WindowFlags f = Qt::WindowFlags());

	public:
		void setValueCount(int count);
		int valueCount() const;

		void addValue(int value);
		int value(int index);

		void clear();

	protected:
		void paintEvent(QPaintEvent *event);

	private:
		QVector<int> _values;
		int _max;
		int _offset;
		int _count;
		int _sum;
		int _collectedValues;
};


}
}

#endif
