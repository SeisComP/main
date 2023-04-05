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


#include "eventtablewidget.h"

#include <QHeaderView>

#include <seiscomp/gui/core/application.h>


namespace {

QTableWidgetItem* createTableWidgetItem(const QString& text, bool isActiveEvent = false) {
	QTableWidgetItem* tableWidgetItem = new QTableWidgetItem(text);
	tableWidgetItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

	return tableWidgetItem;
}


} // namespace




EventTableWidget::EventTableWidget(QWidget* parent)
: QTableWidget(parent)
, _controlKeyPressed(false)
, _selectedRow(-1) {

	uiInit();

	connect(this, SIGNAL(cellDoubleClicked(int, int)), this, SLOT(handleCellDoubleClickEvent(int)));
	connect(this, SIGNAL(cellPressed(int, int)), this, SLOT(handleCellPressedEvent(int)));
}



EventTableWidget::~EventTableWidget() {
}




void EventTableWidget::removeEventTableEntries() {
	setRowCount(0);
}




void EventTableWidget::addRow(RowData& rowData) {
	insertRow(0);

	QTableWidgetItem* item = createTableWidgetItem(rowData[EVENT_ID], rowData.isActive());
	setItem(0, EVENT_ID, item);

	item = createTableWidgetItem(rowData[ORIGIN_TIME], rowData.isActive());
	setItem(0, ORIGIN_TIME, item);

	item = createTableWidgetItem(rowData[MAGNITUDE], rowData.isActive());
	setItem(0, MAGNITUDE, item);

	item = createTableWidgetItem(rowData[MAGNITUDE_TYPE], rowData.isActive());
	setItem(0, MAGNITUDE_TYPE, item);

	item = createTableWidgetItem(rowData[EVENT_REGION], rowData.isActive());
	setItem(0, EVENT_REGION, item);

	item = createTableWidgetItem(rowData[LATITUDE], rowData.isActive());
	setItem(0, LATITUDE, item);

	item = createTableWidgetItem(rowData[LONGITUDE], rowData.isActive());
	setItem(0, LONGITUDE, item);

	item = createTableWidgetItem(rowData[DEPTH], rowData.isActive());
	setItem(0, DEPTH, item);

	if ( rowData.isSelected() )
		selectRow(0);

	resizeColumnsToContents();

	QHeaderView *horizontalHeaderRef = horizontalHeader();
	horizontalHeaderRef->setSectionResizeMode(_lastVisibleSection, QHeaderView::Stretch);
}




void EventTableWidget::keyPressEvent(QKeyEvent* keyEvent) {
	if ( keyEvent->key() == Qt::Key_Control )
		_controlKeyPressed = true;
}



void EventTableWidget::keyReleaseEvent(QKeyEvent* keyEvent) {
	if ( keyEvent->key() == Qt::Key_Control )
		_controlKeyPressed = false;
}




void EventTableWidget::handleCellDoubleClickEvent(int row) {
	QString eventId =  getEventId(row);

	emit eventSelected(eventId);
	emit eventDoubleClicked(eventId);
}




void EventTableWidget::handleCellPressedEvent(int row) {
	QString eventId = getEventId(row);

	if ( isRowSelected(row) && _controlKeyPressed ) {
		setSelectedRow(-1);
		clearSelection();
		emit eventDeselected(eventId);
	}
	else {
		setSelectedRow(row);
		emit eventSelected(eventId);
	}
}




void EventTableWidget::uiInit() {
	QStringList tableHeader;
	tableHeader << "Event" << "Origin Time" << "Magnitude" << "Magnitude Type"
	            << "Region" << "Latitude" << "Longitude" << "Depth";

	QVector<bool> sectionsVisible;
	for ( int i = 0; i < tableHeader.count(); ++i )
		sectionsVisible.append(true);

	try {
		std::vector<std::string> cols = SCApp->configGetStrings("eventTable.columns");

		// Switch all section to invisible
		for ( int i = 0; i < sectionsVisible.count(); ++i )
			sectionsVisible[i] = false;

		for ( size_t i = 0; i < cols.size(); ++i ) {
			int idx = tableHeader.indexOf(cols[i].c_str());
			if ( idx < 0 ) continue;
			sectionsVisible[idx] = true;
		}
	}
	catch ( ... ) {}

	setRowCount(0);
	setColumnCount(tableHeader.size());
	setHorizontalHeaderLabels(tableHeader);

	QHeaderView *horizontalHeaderRef = horizontalHeader();

	_lastVisibleSection = -1;
	for ( int i = 0; i < horizontalHeaderRef->count(); ++i ) {
		if ( !sectionsVisible[i] )
			horizontalHeaderRef->hideSection(i);
		else
			_lastVisibleSection = i;
	}

	horizontalHeaderRef->setSectionResizeMode(_lastVisibleSection, QHeaderView::Stretch);

	resizeColumnsToContents();

	setAlternatingRowColors(true);
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::SingleSelection);

	QVBoxLayout *layout = new QVBoxLayout;
	setLayout(layout);

	QSizePolicy sizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	sizePolicy.setHorizontalStretch(1);
	sizePolicy.setVerticalStretch(0);
	sizePolicy.setHeightForWidth(true);
	setSizePolicy(sizePolicy);
}




QString EventTableWidget::getEventId(int row) {
	QTableWidgetItem* tmpItem = item(row, 0);
	QString eventId = tmpItem->text();

	return eventId;
}




int EventTableWidget::selectedRow() const {
	return _selectedRow;
}




void EventTableWidget::setSelectedRow(int row) {
	_selectedRow = row;
}




bool EventTableWidget::isRowSelected(int row) {
	return _selectedRow == row;
}


