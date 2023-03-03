/***************************************************************************
 * Copyright (C) gempa GmbH                                                *
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


#ifndef SEISCOMP_GUI_SCRTTV_TRACEMARKER
#define SEISCOMP_GUI_SCRTTV_TRACEMARKER


#include <seiscomp/datamodel/pick.h>
#include <seiscomp/gui/core/recordwidget.h>


namespace Seiscomp {
namespace Applications {
namespace TraceView {


class TraceMarker : public Gui::RecordMarker {
	public:
		TraceMarker(Gui::RecordWidget *parent, DataModel::Pick *pick);

	public:
		bool setSelected(bool s);
		bool isSelected() const {
			return _isSelected;
		}

		bool setAssociated(bool a);
		bool isAssociated() const {
			return _isAssociated;
		}

	public:
		QString toolTip() const override;
		void drawBackground(QPainter &painter, Gui::RecordWidget *context,
		                    int x, int y1, int y2,
		                    QColor color, qreal lineWidth) override;

	private:
		void updateStyle();

	public:
		DataModel::PickPtr pick;

	private:
		bool _isSelected{false};
		bool _isAssociated{false};
};


}
}
}


#endif
