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


#include "mainwidget.h"
#include <seiscomp/datamodel/messages.h>
#include <seiscomp/gui/datamodel/originsymbol.h>
#include <iostream>

namespace Seiscomp {
namespace Gui {
namespace MessageMonitor {


namespace {


// class ArtificialOriginSymbol : public Drawable {
// 	public:
// 		ArtificialOriginSymbol(double lat, double lon)
// 			: Drawable(), _pos(lon, lat) {
// 		}
// 
// 		void setColor(const QColor& c) {
// 			_color = c;
// 		}
// 	
// 		void draw(QPainter& painter) {
// 			int x = (int)lon;
// 			int y = (int)lat;
// 			int rad = size()/2;
// 
// 			painter.setPen(_color);
// 			painter.setBrush(Qt::NoBrush);
// 			painter.drawEllipse(x-rad, y-rad, x+rad, y-rad);
// 		}
// 
// 		bool isInside(int x, int y) const {
// 			return false;
// 		}
// 
// 
// 	private:
// 		QPointF _pos;
// 		QColor _color;
// };


}


MainWidget::MainWidget(QWidget* parent, QObject* signaller, const MapsDesc &maps)
: MapWidget(maps, parent) {
	setAttribute(Qt::WA_DeleteOnClose);

	connect(signaller, SIGNAL(messageReceived(Seiscomp::Core::MessagePtr)),
	        this, SLOT(onReceivedMessage(Seiscomp::Core::MessagePtr)));
}


void MainWidget::onReceivedMessage(Seiscomp::Core::MessagePtr msg) {
	Seiscomp::DataModel::ArtificialOriginMessage* ao = Seiscomp::DataModel::ArtificialOriginMessage::Cast(msg);
	if ( ao ) {
		if ( ao->origin() ) {
			canvas().symbolCollection()->add(new OriginSymbol(ao->origin()->latitude(), ao->origin()->longitude()));
			canvas().displayRect(QRectF(ao->origin()->longitude() - 20.0, ao->origin()->latitude() - 20.0,
			                   40.0, 40.0));
			update();
		}
	}
}


}
}
}
