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





#ifndef QCVIEW_H
#define QCVIEW_H

#include <string>

#ifndef Q_MOC_RUN
#include <seiscomp/datamodel/databasequery.h>
#endif

#include "qcmodel.h"
#include "qcviewconfig.h"
#include "qcitemview.h"

#include <QWidget>

class QTableView;

namespace Seiscomp {
namespace Applications {
namespace Qc {


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
class QcView : public QWidget {
	Q_OBJECT

	public:
		QcView(QcModel *qcModel, QString name=QString(),
		       QWidget *parent=nullptr, Qt::WindowFlags f=Qt::WindowFlags());

		~QcView();

		virtual void init();
		void setExpireTime(double time);

		void setRecordStreamURL(const std::string& recordStreamURL);
		const std::string& recordStreamURL() const ;

		void setDatabaseQueryInterface(const DataModel::DatabaseQuery* query);
		const DataModel::DatabaseQuery* databaseQueryInterface() const ;

	public slots:
		void hideFilterWidget(bool hide);
		void setFilterRegExp(const QString& expr);
		void filterRegExpChanged(const QString&);
		void updateStreamCount();

	protected:
		QcModel                        *_qcModel;
		QString                         _name;
		QSortFilterProxyModel          *_qcProxyModel;
		QVBoxLayout                    *_layout;
		QHBoxLayout                    *_layout2;
		QWidget                        *_filterWidget;
		QLabel                         *_lbVSecCount;
		QLineEdit                      *_leFilter;
		QLabel                         *_lbLeFilter;

		std::string                     _recordStreamURL;
		const DataModel::DatabaseQuery *_dbQuery;

};
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
class QcTableView : public QcView {
	Q_OBJECT

	public:
		QcTableView(QcModel *qcModel, QString name=QString(),
		            QWidget *parent=nullptr,
		            Qt::WindowFlags f=Qt::WindowFlags());

		~QcTableView();
		void init();
		QTableView* qTableView();

	protected:
		bool eventFilter(QObject* o, QEvent* e);

	public slots:
		void showStream(int);
		double streamWidgetLength() const;
		void setStreamWidgetLength(double length);

		void showDialog(const QModelIndex& inx);


	private:
		QTableView *_qcTable;
		double _streamWidgetLength;

};
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
class QcOverView : public QcView {
	Q_OBJECT

	public:
		QcOverView(QcModel *qcModel, QString name=QString(),
		           QWidget *parent=nullptr,
		           Qt::WindowFlags f=Qt::WindowFlags());
		~QcOverView();
		void init();

	private:
		QcItemView *_overView;

};
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
class QcTableCornerButton : public QPushButton {
	Q_OBJECT

	public:
		QcTableCornerButton(QWidget* parent) : QPushButton(parent) {}

		void paintEvent(QPaintEvent*) {
			QStyleOptionButton opt;
			opt.init(this);
			opt.state = QStyle::State_None;
			opt.features = QStyleOptionButton::Flat;
			opt.rect = rect();
			opt.text = text();
			QPainter painter(this);
			painter.setBackgroundMode(Qt::TransparentMode);
			style()->drawControl(QStyle::CE_PushButton, &opt, &painter, this);
		}

};
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

}
}
}

#endif
