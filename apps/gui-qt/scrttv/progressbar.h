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




#ifndef SEISCOMP_GUI_PROGRESSBAR_H__
#define SEISCOMP_GUI_PROGRESSBAR_H__

#include <QPainter>
#include <QPaintEvent>
#include <QFrame>

class QWidget;


namespace Seiscomp {
namespace Gui {


class ProgressBar : public QFrame
{
  Q_OBJECT

	public:
		ProgressBar(QWidget *parent=0)
		: QFrame(parent)
		{
			setMinimumSize(50,10);
			_value = 0;
		}

	public slots:
		void reset()
		{
			_value = 0;
			update();
		}

		void setValue(int val)
		{
			_value = val;
			if (_value>100)
				_value = 100;
			repaint();
		}

	protected:
		void paintEvent(QPaintEvent *)
		{
			int w=width(), h=height();
			QPainter paint(this);
			paint.fillRect(0, 0, int(w*_value / 100), h, QColor(0,0,128));
		}

	private:
		int _value;
};


}
}


#endif
