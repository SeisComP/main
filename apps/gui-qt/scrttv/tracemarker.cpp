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


#include <seiscomp/gui/core/application.h>
#include <seiscomp/gui/core/utils.h>

#include "tracemarker.h"


using namespace Seiscomp::DataModel;


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
namespace Seiscomp {
namespace Applications {
namespace TraceView {
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
TraceMarker::TraceMarker(Gui::RecordWidget *parent, Pick *pick)
: Gui::RecordMarker(parent, pick->time().value())
, pick(pick) {
	QString phaseCode;

	try {
		phaseCode = pick->phaseHint().code().c_str();
	}
	catch ( ... ) {}

	setText(phaseCode);

	setData(QString(pick->publicID().c_str()));
	setMovable(false);
	updateStyle();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool TraceMarker::setSelected(bool s) {
	if ( _isSelected == s ) {
		return false;
	}

	_isSelected = s;
	updateStyle();
	update();
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool TraceMarker::setAssociated(bool a) {
	if ( _isAssociated == a ) {
		return false;
	}

	_isAssociated = a;
	updateStyle();
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceMarker::updateStyle() {
	try {
		switch ( pick->evaluationMode() ) {
			case AUTOMATIC:
				setColor(
					_isAssociated
					?
					SCScheme.colors.arrivals.automatic
					:
					SCScheme.colors.picks.automatic
				);
				break;
			case MANUAL:
				setColor(
					_isAssociated
					?
					SCScheme.colors.arrivals.manual
					:
					SCScheme.colors.picks.manual
				);
				break;
			default:
				setColor(
					_isAssociated
					?
					SCScheme.colors.arrivals.undefined
					:
					SCScheme.colors.picks.undefined
				);
				break;
		}
	}
	catch ( ... ) {
		setColor(SCScheme.colors.picks.undefined);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
QString TraceMarker::toolTip() const {
	if ( !pick ) {
		return RecordMarker::toolTip();
	}

	QString text = pick->publicID().c_str();
	text += "\n";

	try {
		switch ( pick->evaluationMode() ) {
			case MANUAL:
				text += "manual ";
				break;
			case AUTOMATIC:
				text += "automatic ";
				break;
			default:
				break;
		}
	}
	catch ( ... ) {}

	try {
		text += pick->phaseHint().code().c_str();
		text += " ";
	}
	catch ( ... ) {
		text += "?? ";
	}

	text += "pick";

	try {
		text += QString(" created by %1").arg(pick->creationInfo().author().c_str());
		text += QString(" / %1").arg(pick->creationInfo().agencyID().c_str());
	}
	catch ( ... ) {}

	try {
		text += QString(" at %1").arg(Gui::timeToString(pick->creationInfo().creationTime(), "%F %T"));
	}
	catch ( ... ) {}

	text += QString("\ntime: %1").arg(Gui::timeToString(pick->time().value(), "%F %T.%f"));
	if ( !pick->methodID().empty() ) {
		text += QString("\nmethod: %1").arg(pick->methodID().c_str());
	}
	if ( !pick->filterID().empty() ) {
		text += QString("\nfilter: %1").arg(pick->filterID().c_str());
	}
	try {
		double baz = pick->backazimuth().value();
		text += QString("\nbackazimuth: %1°").arg(baz);
	}
	catch ( ... ) {}
	try {
		double hs = pick->horizontalSlowness().value();
		text += QString("\nhoriz. slowness: %1 deg/s").arg(hs);
	}
	catch ( ... ) {}

	text += QString("\narrival: %1").arg(isAssociated()?"yes":"no");

	return text;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceMarker::drawBackground(QPainter &painter, Gui::RecordWidget *,
                                 int x, int y1, int y2, QColor, qreal) {
	if ( !_isSelected ) {
		return;
	}

	painter.fillRect(x - 4, y1, 4, y2 - y1,
	                 SCApp->palette().color(QPalette::Highlight));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
}
}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
