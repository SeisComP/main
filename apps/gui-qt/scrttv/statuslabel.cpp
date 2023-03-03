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


#include "statuslabel.h"

#include <QPainter>


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
namespace {


struct Style {
	QColor text;
	QColor background;
	QColor border;
};


Style styles[] = {

	{ QColor(0x2c, 0x66, 0x2d), QColor(0xfc, 0xff, 0xf5), QColor(0xa3, 0xc2, 0x93) },
	{ QColor(0x27, 0x6f, 0x86), QColor(0xf8, 0xff, 0xff), QColor(0xa9, 0xd5, 0xde) },
	{ QColor(0x57, 0x3a, 0x08), QColor(0xff, 0xfa, 0xf3), QColor(0xc9, 0xba, 0x9b) },
	{ QColor(0x9f, 0x3a, 0x38), QColor(0xff, 0xf6, 0xf6), QColor(0xe0, 0xb4, 0xb4) }

};


}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
namespace Seiscomp {
namespace Applications {
namespace TraceView {
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
StatusLabel::StatusLabel(QWidget *parent) : QLabel(parent) {
	setMargin(9);
	setLevel(Info);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void StatusLabel::setLevel(Level level) {
	_level = level;
	QPalette p = palette();
	p.setColor(QPalette::WindowText, styles[_level].text);
	setPalette(p);
	update();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void StatusLabel::paintEvent(QPaintEvent *event) {
	QPainter p(this);
	int borderRadius = 4;

	p.setRenderHint(QPainter::Antialiasing, true);
	p.setPen(styles[_level].border);
	p.setBrush(styles[_level].background);
	p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), borderRadius, borderRadius);

	QLabel::paintEvent(event);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
}
}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
