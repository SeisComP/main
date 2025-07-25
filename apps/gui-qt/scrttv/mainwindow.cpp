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

#define SEISCOMP_COMPONENT Gui::TraceView

#include "mainwindow.h"
#include <seiscomp/logging/log.h>
#include <seiscomp/core/record.h>
#include <seiscomp/core/datamessage.h>
#include <seiscomp/io/recordinput.h>
#include <seiscomp/io/recordstream/file.h>
#include <seiscomp/io/archive/xmlarchive.h>
#include <seiscomp/datamodel/databasequery.h>
#include <seiscomp/datamodel/inventory_package.h>
#include <seiscomp/datamodel/config_package.h>
#include <seiscomp/datamodel/eventparameters_package.h>
#include <seiscomp/datamodel/messages.h>
#include <seiscomp/datamodel/utils.h>
#include <seiscomp/utils/keyvalues.h>
#include <seiscomp/math/geo.h>
#include <seiscomp/utils/timer.h>
#include <seiscomp/seismology/regions.h>
#include <seiscomp/gui/core/recordviewitem.h>
#include <seiscomp/gui/core/application.h>
#include <seiscomp/gui/core/scheme.h>
#include <seiscomp/gui/core/infotext.h>
#include <seiscomp/gui/core/flowlayout.h>
#include <seiscomp/gui/datamodel/origindialog.h>
#include <seiscomp/gui/datamodel/inventorylistview.h>

#include <QActionGroup>
#include <QDockWidget>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>

#include <map>
#include <set>
#include <fstream>

#include "associator.h"
#include "settings.h"
#include "spectrogramsettings.h"
#include "tracemarker.h"


using namespace std;
using namespace Seiscomp;
using namespace Seiscomp::Gui;
using namespace Seiscomp::IO;
using namespace Seiscomp::DataModel;


#define DATA_INDEX 0
#define DATA_DELTA 1
#define DATA_GROUP 2
#define DATA_GROUP_MEMBER 3
#define DATA_GROUP_INDEX 4


#define MODE_STANDARD 0
#define MODE_PICKS    1
#define MODE_ZOOM     2
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
namespace {


bool isWildcard(const string &s) {
	return s.find('*') != string::npos || s.find('?') != string::npos;
}


WaveformStreamID makeWaveformID(const std::string &net,
                                const std::string &sta,
                                const std::string &loc = std::string(),
                                const std::string &cha = std::string()) {
	return WaveformStreamID(net, sta, loc, cha, "");
}


bool compare(const WaveformStreamID &pattern, const WaveformStreamID &id) {
	return Core::wildcmp(pattern.networkCode(), id.networkCode())
	   and Core::wildcmp(pattern.stationCode(), id.stationCode())
	   and Core::wildcmp(pattern.locationCode(), id.locationCode())
	   and Core::wildcmp(pattern.channelCode(), id.channelCode());
}


QString waveformIDToString(const WaveformStreamID& id) {
	return (id.networkCode() + "." + id.stationCode() + "." +
	        id.locationCode() + "." + id.channelCode()).c_str();
}

string waveformIDToStdString(const WaveformStreamID& id) {
	return (id.networkCode() + "." + id.stationCode() + "." +
	        id.locationCode() + "." + id.channelCode());
}


QString strTimeSpan(long seconds) {
	long days = seconds / 86400;
	long secondsPerDay = seconds % 86400;

	long hours = secondsPerDay / 3600;
	long secondsPerHour = secondsPerDay % 3600;

	long minutes = secondsPerHour / 60;
	seconds = secondsPerHour % 60;

	/*
	if ( days > 0 )
		return QString("%1 days and %2 hours").arg(days).arg(hours);
	else if ( hours > 0 )
		return QString("%1 hours and %2 minutes").arg(hours).arg(minutes);
	else if ( minutes > 0 )
		return QString("%1 minutes and %2 seconds").arg(minutes).arg(seconds);
	else
		return QString("%1 seconds").arg(seconds);
	*/

	return QString("%1:%2:%3:%4")
	        .arg(days,4,10,QChar(' '))
	        .arg(hours,2,10,QChar('0'))
	        .arg(minutes,2,10,QChar('0'))
	        .arg(seconds,2,10,QChar('0'));
}


QString prettyTimeRange(int seconds) {
	QString str;

	auto days = seconds / 86400;
	if ( days > 0 ) {
		if ( !str.isEmpty() ) {
			str += " ";
		}
		str += QString("%1 day%2").arg(days).arg(days > 1 ? "s" : "");
	}

	seconds = seconds % 86400;

	auto hours = seconds / 3600;
	if ( hours > 0 ) {
		if ( !str.isEmpty() ) {
			str += " ";
		}
		str += QString("%1 hour%2").arg(hours).arg(hours > 1 ? "s" : "");
	}

	seconds = seconds % 3600;

	auto minutes = seconds / 60;
	if ( minutes > 0 ) {
		if ( !str.isEmpty() ) {
			str += " ";
		}
		str += QString("%1 minute%2").arg(minutes).arg(minutes > 1 ? "s" : "");
	}

	seconds = seconds % 60;
	if ( seconds > 0 ) {
		if ( !str.isEmpty() ) {
			str += " ";
		}
		str += QString("%1 second%2").arg(seconds).arg(seconds > 1 ? "s" : "");
	}

	return str;
}


bool isStreamBlacklisted(const vector<string> &blacklist,
                         const string &net, const string &sta,
                         const string &loc, const string &cha) {
	if ( blacklist.empty() )
		return false;

	string sid = net + "." + sta + "." + loc + "." + cha;

	for ( size_t i = 0; i < blacklist.size(); ++i )
		if ( Core::wildcmp(blacklist[i], sid) )
			return true;

	return false;
}



#define _T(name) q->driver()->convertColumnName(name)
DatabaseIterator loadPicks(const Core::Time &start, const OPT(Core::Time) &end) {
	SEISCOMP_DEBUG("Loading picks from %s to %s",
	               start.iso(), (end ? *end : Core::Time()).iso());

	// Assumed to be valid => check at caller
	auto q = SCApp->query();

	std::ostringstream oss;
	oss << "select PPick." << _T("publicID") << ",Pick.*,count(Origin._oid) as refCount "
	       "from Pick join PublicObject as PPick"
	       " on Pick._oid=PPick._oid"
	       " and Pick." << _T("time_value") << ">='" << q->toString(start) << "' ";
	if ( end ) {
		oss << "and Pick." << _T("time_value") << "<='" << q->toString(*end) << "' ";
	}
	oss << "left join Arrival"
	       " on Arrival." << _T("pickID") << "=PPick." << _T("publicID") << " "
	       "left join Origin"
	       " on Origin._oid=Arrival._parent_oid"
	       " and (Origin." << _T("evaluationStatus") << "!='" << EvaluationStatus(REJECTED).toString() << "' or Origin." << _T("evaluationStatus") << " is null) "
	       "group by Pick._oid, PPick." << _T("publicID");

	return q->getObjectIterator(oss.str(), Pick::TypeInfo());
}


DatabaseIterator loadPickRefCounts(const string &originID) {
	auto q = SCApp->query();

	std::ostringstream oss;
	oss << "select PPick." << _T("publicID") << " as pickID,count(Origin._oid) as refCount "
	       "from Pick join PublicObject as PPick"
	       " on Pick._oid=PPick._oid "
	       "join Arrival as OAr on OAr." << _T("pickID") << "=PPick." << _T("publicID") << " "
	       "join Origin as O on O._oid=OAr._parent_oid "
	       "join PublicObject as OPo on OPo._oid=O._oid and OPo." << _T("publicID") << "='" << q->toString(originID) << "' "
	       "left join Arrival"
	       " on Arrival." << _T("pickID") << "=PPick." << _T("publicID") << " "
	       "left join Origin"
	       " on Origin._oid=Arrival._parent_oid "
	       "group by PPick." << _T("publicID");

	return q->getObjectIterator(oss.str(), nullptr);
}


}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Q_DECLARE_METATYPE(Seiscomp::Applications::TraceView::TraceState)
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
namespace Seiscomp {
namespace Applications {
namespace TraceView {
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
namespace {


class TabEditWidget : public QLineEdit {
	public:
		TabEditWidget(int index, QWidget *parent = 0) : QLineEdit(parent), _index(index) {
			setAttribute(Qt::WA_DeleteOnClose);
		}

		int index() const { return _index; }

	protected:
		void focusOutEvent(QFocusEvent *e) {
			close();
		}

	private:
		int _index;
};


}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
class TraceDecorator : public Gui::RecordWidgetDecorator {
	public:
		TraceDecorator(QObject *parent, const MainWindow::DecorationDesc *desc)
		: Gui::RecordWidgetDecorator(parent), _desc(desc) {}

		void drawLine(QPainter *painter, Gui::RecordWidget *widget, float y, bool fillAbove,
		              const QBrush &brush) {
			QPair<double,double> range = widget->amplitudeRange(0);
			range.first *= *widget->recordScale(0);
			range.second *= *widget->recordScale(0);
			float amplRange = range.second-range.first;
			if ( amplRange > 0 ) {
				int y0 = widget->streamYPos(0);
				int h = widget->streamHeight(0);
				int y1 = y0+h;
				int py = y1-(y-range.first)*h / (range.second-range.first);
				painter->drawLine(0,py,painter->window().width(),py);

				if ( brush != Qt::NoBrush ) {
					if ( fillAbove ) {
						if ( py > y1) py = y1;
						if ( py > y0 )
							painter->fillRect(0,y0,painter->window().width(),py-y0,brush);
					}
					else {
						if ( py <= y0) py = y0;
						if ( py <= y1 )
							painter->fillRect(0,py,painter->window().width(),y1-py,brush);
					}
				}
			}
		}

		void drawDecoration(QPainter *painter, Gui::RecordWidget *widget) {
			if ( _desc->minValue ) {
				painter->setPen(_desc->minPen);
				drawLine(painter, widget, *_desc->minValue, false, _desc->minBrush);
			}

			if ( _desc->maxValue ) {
				painter->setPen(_desc->maxPen);
				drawLine(painter, widget, *_desc->maxValue, true, _desc->maxBrush);
			}

			int yofs = 0;
			if ( !_desc->description.isEmpty() ) {
				painter->setFont(SCScheme.fonts.highlight);
				QRect r = painter->boundingRect(painter->window(), Qt::AlignLeft | Qt::AlignTop, _desc->description);
				r.adjust(-4,-4,4,4);
				r.moveTo(0,0);
				painter->fillRect(r, widget->palette().color(QPalette::Text));
				painter->setPen(widget->palette().color(QPalette::Window));
				painter->drawText(r, Qt::AlignCenter, _desc->description);
				yofs += r.height();
			}

			QPair<double,double> amplRange = widget->amplitudeDataRange(0);
			QString amps = QString("max: %1").arg(amplRange.second, 0, 'f', 1);
			if ( !_desc->unit.isEmpty() ) {
			    amps += " ";
			    amps += _desc->unit;
			}
			amps += "\n";
			amps += QString("min: %1").arg(amplRange.first, 0, 'f', 1);
			if ( !_desc->unit.isEmpty() ) {
			   amps += " ";
			   amps += _desc->unit;
			}
			painter->setFont(SCScheme.fonts.base);
			painter->setPen(widget->palette().color(QPalette::Text));
			QRect r = painter->boundingRect(painter->window(), Qt::AlignLeft | Qt::AlignTop, amps);
			r.adjust(-4,-4,4,4);
			r.moveTo(0,yofs);
			painter->fillRect(r, widget->palette().color(QPalette::Base));
			r.translate(4,4);
			painter->drawText(r, Qt::AlignLeft | Qt::AlignTop, amps);
		}

	private:
		const MainWindow::DecorationDesc *_desc;
};
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
TraceViewTabBar::TraceViewTabBar(QWidget *parent) : QTabBar(parent) {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int TraceViewTabBar::findIndex(const QPoint& p) {
	for ( int i = 0; i < count(); ++i )
		if ( tabRect(i).contains(p) )
			return i;

	return -1;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceViewTabBar::mousePressEvent(QMouseEvent *e) {
	QTabBar::mousePressEvent(e);
	return;

	// Disable renaming of tabs for now
	/*
	if ( e->button() != Qt::LeftButton ) {
		QTabBar::mousePressEvent(e);
		return;
	}

	int pressedTab = findIndex(e->pos());
	if ( pressedTab != currentIndex() ) {
		QTabBar::mousePressEvent(e);
		return;
	}

	TabEditWidget *edit = new TabEditWidget(currentIndex(), this);

	connect(edit, SIGNAL(returnPressed()),
	        this, SLOT(textChanged()));

	connect(this, SIGNAL(currentChanged(int)),
	        edit, SLOT(close()));

	edit->setGeometry(tabRect(currentIndex()));
	edit->setText(tabText(currentIndex()));
	edit->selectAll();
	edit->setFocus();
	edit->show();
	*/
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceViewTabBar::textChanged() {
	TabEditWidget *editor = (TabEditWidget*)sender();
	if ( !editor ) return;

	QTabWidget* tw = dynamic_cast<QTabWidget*>(parent());
	if ( tw )
		tw->setTabText(editor->index(), editor->text());
	editor->close();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
TraceWidget::TraceWidget(const WaveformStreamID &sid)
: RecordWidget(sid) {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
TraceWidget::~TraceWidget() {
	if ( _spectrogram ) {
		delete _spectrogram;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Gui::SpectrogramRenderer *TraceWidget::spectrogram() {
	if ( !_spectrogram ) {
		_spectrogram = new Gui::SpectrogramRenderer;
		_spectrogram->setNormalizationMode(Gui::SpectrogramRenderer::NormalizationMode::Frequency);
		_spectrogram->setSmoothTransform(false);
		// Force the spectralizer to be created.
		_spectrogram->setOptions(_spectrogram->options());
	}

	return _spectrogram;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceWidget::setShowSpectrogram(bool enable) {
	if ( _showSpectrogram == enable ) {
		return;
	}

	_showSpectrogram = enable;

	if ( _showSpectrogram ) {
		spectrogram();
	}

	resetSpectrogram();
	update();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceWidget::setShowSpectrogramAxis(bool enable) {
	if ( !enable ) {
		if ( _spectrogramAxis ) {
			delete _spectrogramAxis;
			_spectrogramAxis = nullptr;
		}
	}
	else {
		if ( !_spectrogramAxis ) {
			_spectrogramAxis = new Gui::Axis(this);
			_spectrogramAxis->setLabel(tr("f [1/T] in Hz"));
			_spectrogramAxis->setPosition(Seiscomp::Gui::Axis::Left);
		}
	}
	update();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceWidget::fed(int slot, const Seiscomp::Record *rec) {
	RecordWidget::fed(slot, rec);

	if ( _showSpectrogram && (slot == 0) ) {
		_spectrogram->setTimeWindow(records(slot)->timeWindow());
		if ( isFilteringEnabled() ) {
			auto seq = filteredRecords(slot);
			if ( seq ) {
				auto frec = seq->back();
				if ( frec ) {
					_spectrogram->feed(frec.get());
				}
			}
		}
		else {
			_spectrogram->feed(rec);
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceWidget::resetSpectrogram() {
	if ( _showSpectrogram ) {
		qApp->setOverrideCursor(Qt::WaitCursor);
		const double *scale = recordScale(0);
		if ( scale ) {
			_spectrogram->setScale(*scale);
		}
		else {
			_spectrogram->setScale(1.0);
		}
		auto seq = isFilteringEnabled() ? filteredRecords(0) : records(0);
		if ( seq ) {
			_spectrogram->setTimeWindow(seq->timeWindow());
		}
		_spectrogram->setRecords(seq);
		update();
		qApp->restoreOverrideCursor();
	}
	else {
		_spectrogram->reset();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceWidget::drawSpectrogram(QPainter &painter) {
	QRect r(canvasRect());

	_spectrogram->setAlignment(alignment());
	_spectrogram->setTimeRange(tmin(), tmax());
	painter.save();
	painter.setClipRect(r);
	_spectrogram->render(painter, r, false, false);
	painter.restore();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceWidget::drawSpectrogramAxis(QPainter &painter) {
	if ( !_spectrogramAxis ) {
		return;
	}

	QRect r(canvasRect());

	painter.save();

	QPair<double, double> range = _spectrogram->range();
	_spectrogramAxis->setRange(Seiscomp::Gui::Range(range.first, range.second));

	r.setRight(r.left());
	r.setWidth(axisWidth());
	_spectrogramAxis->setLogScale(_spectrogram->logScale());
	_spectrogramAxis->updateLayout(painter, r);
	r.setLeft(canvasRect().left());
	painter.fillRect(r.adjusted(-axisSpacing(),0,0,0), palette().color(backgroundRole()));
	_spectrogramAxis->draw(painter, r, true);

	painter.restore();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceWidget::paintEvent(QPaintEvent *event) {
	if ( _showSpectrogram ) {
		QPainter painter(this);
		QFont font;

		drawSpectrogram(painter);

		painter.setPen(SCScheme.colors.records.alignment);
		int x = static_cast<int>(-tmin() * timeScale());
		painter.drawLine(x, 0, x, height());

		drawMarkers(painter, font, palette().color(foregroundRole()));
		drawSpectrogramAxis(painter);
	}
	else {
		RecordWidget::paintEvent(event);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
TraceView::TraceView(const Seiscomp::Core::TimeSpan& span,
                     QWidget *parent, Qt::WindowFlags f)
: Seiscomp::Gui::RecordView(span, parent, f) {
	_timeSpan = (double)span;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
TraceView::~TraceView() {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceView::toggleFilter(bool) {
	for ( int i = 0; i < rowCount(); ++i ) {
		auto item = itemAt(i);
		static_cast<TraceWidget*>(item->widget())->resetSpectrogram();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceView::toggleSpectrogram(bool enable) {
	for ( int i = 0; i < rowCount(); ++i ) {
		auto item = itemAt(i);
		static_cast<TraceWidget*>(item->widget())->setShowSpectrogram(enable);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Gui::RecordWidget *TraceView::createRecordWidget(
	const DataModel::WaveformStreamID &streamID
) const {
	return new TraceWidget(streamID);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
TraceTabWidget::TraceTabWidget(QWidget* parent)
	: QTabWidget(parent) {

	_nonInteractive = true;

	_tabBar = new TraceViewTabBar;
	setTabBar(_tabBar);

	setAcceptDrops(true);
	_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(_tabBar, SIGNAL(customContextMenuRequested(const QPoint&)),
	        this, SLOT(showContextMenu(const QPoint&)));

	QAction *closeTab = new QAction("Close tab", this);
	closeTab->setShortcut(Qt::Key_F4 | Qt::CTRL);

	addAction(closeTab);
	_tabActions.append(closeTab);

	connect(closeTab, SIGNAL(triggered()),
	        this, SLOT(closeTab()));

	_tabToClose = -1;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceTabWidget::closeTab() {
	if ( _nonInteractive ) return;

	int tab = _tabToClose;
	if ( tab == -1 )
		tab = currentIndex();

	emit tabRemovalRequested(tab);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceTabWidget::checkActiveTab(const QPoint& p) {
	int tab = _tabBar->findIndex(_tabBar->mapFromParent(p));
	if ( tab != -1 )
		setCurrentIndex(tab);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool TraceTabWidget::checkDraging(QDropEvent *event) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	checkActiveTab(event->pos());
#else
	checkActiveTab(event->position().toPoint());
#endif

	Seiscomp::Gui::RecordView* view = dynamic_cast<Seiscomp::Gui::RecordView*>(event->source());
	if ( view == widget(currentIndex()) ) {
		return false;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceTabWidget::dragEnterEvent(QDragEnterEvent *event) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	checkActiveTab(event->pos());
#else
	checkActiveTab(event->position().toPoint());
#endif
	event->acceptProposedAction();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceTabWidget::showContextMenu(const QPoint& p) {
	if ( _nonInteractive ) return;
	_tabToClose = _tabBar->findIndex(p);
	_tabToClose = -1;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceTabWidget::dragMoveEvent(QDragMoveEvent *event) {
	if ( !checkDraging(event) ) return;
	event->acceptProposedAction();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceTabWidget::dropEvent(QDropEvent *event) {
	if ( !checkDraging(event) ) return;

	if (event->possibleActions() & Qt::MoveAction) {
		Seiscomp::Gui::RecordView* source = dynamic_cast<Seiscomp::Gui::RecordView*>(event->source());
		Seiscomp::Gui::RecordView* target = static_cast<Seiscomp::Gui::RecordView*>(widget(currentIndex()));
		if ( source && target ) {
			//source->moveSelectionTo(target);
			emit moveSelectionRequested(target, source);
			event->acceptProposedAction();
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void TraceTabWidget::showEvent(QShowEvent *) {
	for ( int i = 0; i < count(); ++i )
		widget(i)->setVisible(widget(i) == currentWidget());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
#define TRACEVIEWS(method)\
	foreach ( TraceView* view, _traceViews ) view->method

#define CURRENT_TRACEVIEW(method)\
	if ( _tabWidget ) \
		static_cast<TraceView*>(_tabWidget->currentWidget())->method;\
	else\
		_traceViews.front()->method

#define CURRENT_TRACEVIEW_RET(var, method)\
	if ( _tabWidget ) \
		var = static_cast<TraceView*>(_tabWidget->currentWidget())->method;\
	else\
		var = _traceViews.front()->method
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
MainWindow::MainWindow() : _questionApplyChanges(this) {
	_ui.setupUi(this);
	_ui.actionLoadDataBack->setText(tr("Load data - %1").arg(prettyTimeRange(Settings::global.bufferSize)));
	_ui.actionLoadDataBack->setIconText(_ui.actionLoadDataBack->text());
	_ui.actionLoadDataNext->setText(tr("Load data + %1").arg(prettyTimeRange(Settings::global.bufferSize)));
	_ui.actionLoadDataNext->setIconText(_ui.actionLoadDataNext->text());

	_questionApplyChanges.setText("You are about to enable/disable one or more streams.\n"
	                              "As a result all streams of the station(s) the stream(s)\n"
	                              "belong(s) to will be changed as well.\n"
	                              "This information will be sent as a message\n"
	                              "to tell all clients adding/removing this station(s).\n"
	                              "Do you want to continue changing the state?");

	_recordStreamThread = nullptr;
	_tabWidget = nullptr;
	_needColorUpdate = false;
	_bufferSize = Core::TimeSpan(Settings::global.bufferSize, 0);

	connect(_ui.actionLoadDataBack, SIGNAL(triggered()), this, SLOT(reverse()));
	connect(_ui.actionLoadDataNext, SIGNAL(triggered()), this, SLOT(advance()));

	_statusBarSelectMode = new QComboBox;
	_statusBarSelectMode->addItem(tr("Standard mode"));
	_statusBarSelectMode->addItem(tr("Associate picks"));
	_statusBarSelectMode->addItem(tr("Zoom mode"));
	connect(_statusBarSelectMode, SIGNAL(currentIndexChanged(int)),
	        this, SLOT(selectMode(int)));

	_statusBarFile       = new QLabel;
	_statusBarFilter     = new QComboBox;
	_statusBarFilter->addItem(tr("No filter"));
	for ( const auto &filter : Settings::global.filters ) {
		QStringList tokens = QString(filter.c_str()).split(";");
		QString name, def;

		if ( tokens.size() == 1 ) {
			name = tokens[0];
			def = tokens[0];
		}
		else {
			name = tokens[0];
			def = tokens[1];
		}

		if ( name.isEmpty() ) {
			name = def;
		}

		if ( def.isEmpty() ) {
			SEISCOMP_WARNING("Ignoring invalid filter: %s", filter);
			continue;
		}

		if ( name[0] == '@' ) {
			name.remove(0, 1);
		}

		_statusBarFilter->addItem(name, def);
	}

	connect(_statusBarFilter, SIGNAL(currentIndexChanged(int)),
	        this, SLOT(filterSelectionChanged()));

	_statusBarSearch     = new QLineEdit;
	_statusBarProg       = new Seiscomp::Gui::ProgressBar;

	if ( !SCApp->nonInteractive() ) {
		_associator = new Associator(this);
		_dockAssociator = new QDockWidget(tr("Associator/locator control"), this);
		_dockAssociator->setObjectName("Dock" + _associator->objectName());
		_dockAssociator->setWidget(_associator);
		_dockAssociator->setAllowedAreas(Qt::AllDockWidgetAreas);
		_dockAssociator->setVisible(false);
		addDockWidget(Qt::LeftDockWidgetArea, _dockAssociator);
		_dockAssociator->toggleViewAction()->setShortcut(QKeySequence("ctrl+shift+a"));
		_ui.menuWindow->addAction(_dockAssociator->toggleViewAction());

		_spectrogramSettings = new SpectrogramSettings(this);
		connect(_spectrogramSettings, SIGNAL(apply()), this, SLOT(applySpectrogramSettings()));
		auto dockSpec = new QDockWidget(tr("Spectrogram settings"), this);
		dockSpec->setObjectName("Dock" + _spectrogramSettings->objectName());
		dockSpec->setWidget(_spectrogramSettings);
		dockSpec->setAllowedAreas(Qt::AllDockWidgetAreas);
		dockSpec->setVisible(false);
		addDockWidget(Qt::LeftDockWidgetArea, dockSpec);
		dockSpec->toggleViewAction()->setShortcut(QKeySequence("ctrl+shift+s"));
		_ui.menuWindow->addAction(dockSpec->toggleViewAction());

		try {
			_spectrogramSettings->ui.cbSmoothing->setChecked(
				SCApp->configGetBool("spectrogram.smoothing")
			);
		}
		catch ( ... ) {}

		try {
			_spectrogramSettings->ui.cbLogScale->setChecked(
				SCApp->configGetBool("spectrogram.logScale")
			);
		}
		catch ( ... ) {}

		try {
			auto mode = SCApp->configGetString("spectrogram.normalization");
			if ( mode == "fixed" ) {
				_spectrogramSettings->ui.cbNormalization->setCurrentIndex(0);
			}
			else if ( mode == "frequency" ) {
				_spectrogramSettings->ui.cbNormalization->setCurrentIndex(1);
			}
			else if ( mode == "time" ) {
				_spectrogramSettings->ui.cbNormalization->setCurrentIndex(2);
			}
		}
		catch ( ... ) {}

		try {
			_spectrogramSettings->ui.cbShowAxis->setChecked(
				SCApp->configGetBool("spectrogram.axis")
			);
		}
		catch ( ... ) {}

		try {
			_spectrogramSettings->ui.spinMinAmp->setValue(
				SCApp->configGetDouble("spectrogram.minimumAmplitude")
			);
		}
		catch ( ... ) {}

		try {
			_spectrogramSettings->ui.spinMaxAmp->setValue(
				SCApp->configGetDouble("spectrogram.maximumAmplitude")
			);
		}
		catch ( ... ) {}

		try {
			_spectrogramSettings->ui.spinMinFrequency->setValue(
				SCApp->configGetDouble("spectrogram.minimumFrequency")
			);
		}
		catch ( ... ) {}

		try {
			_spectrogramSettings->ui.spinMaxFrequency->setValue(
				SCApp->configGetDouble("spectrogram.maximumFrequency")
			);
		}
		catch ( ... ) {}

		try {
			_spectrogramSettings->ui.spinTimeWindow->setValue(
				SCApp->configGetDouble("spectrogram.timeSpan")
			);
		}
		catch ( ... ) {}

		try {
			_spectrogramSettings->ui.spinOverlap->setValue(
				SCApp->configGetDouble("spectrogram.overlap") * 100
			);
		}
		catch ( ... ) {}
	}
	else {
		_statusBarSelectMode->setVisible(false);
		_statusBarFilter->setVisible(false);
	}

	statusBar()->addPermanentWidget(_statusBarFilter);
	statusBar()->addPermanentWidget(_statusBarSelectMode);
	statusBar()->addPermanentWidget(_statusBarSearch,     1);
	statusBar()->addPermanentWidget(_statusBarFile,       5);
	statusBar()->addPermanentWidget(_statusBarProg,       1);

	_statusBarSearch->setVisible(false);

	_searchBase = _statusBarSearch->palette().color(QPalette::Base);
	_searchError = Gui::blend(Qt::red, _searchBase, 50);

	_ui.menuView->addAction(_actionToggleFullScreen);

	if ( SCApp->isMessagingEnabled() || SCApp->isDatabaseEnabled() ) {
		_ui.menuSettings->addAction(_actionShowSettings);
	}

	connect(_statusBarSearch, SIGNAL(textChanged(const QString&)),
	        this, SLOT(search(const QString&)));

	connect(_statusBarSearch, SIGNAL(returnPressed()),
	        this, SLOT(nextSearch()));

	//setCentralWidget(createTraceView());

	connect(_ui.actionSelectStreams, SIGNAL(triggered()), this, SLOT(selectStreams()));
	connect(_ui.actionListHiddenStreams, SIGNAL(triggered()), this, SLOT(listHiddenStreams()));
	//connect(_ui.actionAddTabulator, SIGNAL(triggered()), this, SLOT(addTabulator()));

	connect(_ui.actionOpenXMLFile, SIGNAL(triggered()), this, SLOT(openXML()));
	connect(_ui.actionQuit, SIGNAL(triggered()), this, SLOT(close()));

	connect(_ui.actionNextFilter, SIGNAL(triggered()), this, SLOT(nextFilter()));
	connect(_ui.actionPreviousFilter, SIGNAL(triggered()), this, SLOT(previousFilter()));
	connect(_ui.actionToggleFilter, SIGNAL(triggered()), this, SLOT(toggleFilter()));
	connect(_ui.actionApplyGain, SIGNAL(toggled(bool)), this, SLOT(showScaledValues(bool)));
	connect(_ui.actionRestoreConfigOrder, SIGNAL(triggered()), this, SLOT(sortByConfig()));
	connect(_ui.actionSortDistance, SIGNAL(triggered()), this, SLOT(sortByDistance()));
	connect(_ui.actionSortStaCode, SIGNAL(triggered()), this, SLOT(sortByStationCode()));
	connect(_ui.actionSortNetStaCode, SIGNAL(triggered()), this, SLOT(sortByNetworkStationCode()));
	connect(_ui.actionSortGroup, SIGNAL(triggered()), this, SLOT(sortByGroup()));

	connect(_ui.actionAlignLeft, SIGNAL(triggered()), this, SLOT(alignLeft()));
	connect(_ui.actionAlignRight, SIGNAL(triggered()), this, SLOT(alignRight()));
	connect(_ui.actionJumpToLastRecord, SIGNAL(triggered()), this, SLOT(jumpToLastRecord()));
	connect(_ui.actionClearPickMarkers, SIGNAL(triggered()), this, SLOT(clearPickMarkers()));
	connect(_ui.actionClearPickCart, SIGNAL(triggered()), this, SLOT(clearPickCart()));

	connect(_ui.actionAlignOriginTime, SIGNAL(triggered()), this, SLOT(alignOriginTime()));

	connect(_ui.actionLineUp, SIGNAL(triggered()), this, SLOT(scrollLineUp()));
	connect(_ui.actionLineDown, SIGNAL(triggered()), this, SLOT(scrollLineDown()));
	connect(_ui.actionPageUp, SIGNAL(triggered()), this, SLOT(scrollPageUp()));
	connect(_ui.actionPageDown, SIGNAL(triggered()), this, SLOT(scrollPageDown()));
	connect(_ui.actionToTop, SIGNAL(triggered()), this, SLOT(scrollToTop()));
	connect(_ui.actionToBottom, SIGNAL(triggered()), this, SLOT(scrollToBottom()));
	connect(_ui.actionScrollLeft, SIGNAL(triggered()), this, SLOT(scrollLeft()));
	connect(_ui.actionScrollRight, SIGNAL(triggered()), this, SLOT(scrollRight()));

	connect(_ui.actionViewPicks, SIGNAL(toggled(bool)), this, SLOT(updateMarkerVisibility()));
	connect(_ui.actionViewArrivals, SIGNAL(toggled(bool)), this, SLOT(updateMarkerVisibility()));

	connect(_ui.actionSearch, SIGNAL(triggered()), this, SLOT(enableSearch()));
	connect(_ui.actionAbortSearch, SIGNAL(triggered()), this, SLOT(abortSearch()));

	connect(SCApp, SIGNAL(messageAvailable(Seiscomp::Core::Message*, Seiscomp::Client::Packet*)),
	        this, SLOT(messageArrived(Seiscomp::Core::Message*, Seiscomp::Client::Packet*)));

	connect(SCApp, SIGNAL(addObject(const QString&, Seiscomp::DataModel::Object*)),
	        this, SLOT(objectAdded(const QString&, Seiscomp::DataModel::Object*)));

	connect(SCApp, SIGNAL(updateObject(const QString&, Seiscomp::DataModel::Object*)),
	        this, SLOT(objectUpdated(const QString&, Seiscomp::DataModel::Object*)));

	connect(_ui.actionModeNone, SIGNAL(triggered()), this, SLOT(selectModeNone()));
	connect(_ui.actionModeZoom, SIGNAL(triggered()), this, SLOT(selectModeZoom()));
	connect(_ui.actionModePicks, SIGNAL(triggered()), this, SLOT(selectModePicks()));

	connect(_ui.actionReload, SIGNAL(triggered()), this, SLOT(reload()));
	connect(_ui.actionSwitchToRealtime, SIGNAL(triggered()), this, SLOT(switchToRealtime()));

	_timer = new QTimer(this);
	_timer->setInterval(1000);

	_switchBack = new QTimer(this);
	_switchBack->setSingleShot(true);

	connect(_timer, SIGNAL(timeout()), this, SLOT(step()));
	connect(_switchBack, SIGNAL(timeout()), this, SLOT(restoreDefaultSorting()));

	connect(&RecordStreamState::Instance(), SIGNAL(connectionClosed(RecordStreamThread*)),
	        this, SLOT(recordStreamClosed(RecordStreamThread*)));

	addAction(_ui.actionAddTabulator);
	addAction(_ui.actionSearch);
	addAction(_ui.actionAbortSearch);

	_ui.actionAbortSearch->setEnabled(false);

	_rowSpacing = 0;
	_withFrames = false;
	_frameMargin = 0;
	_rowHeight = -1;
	_numberOfRows = -1;

	try { _rowSpacing = SCApp->configGetInt("streams.rowSpacing"); }
	catch ( ... ) {}
	try { _withFrames = SCApp->configGetBool("streams.withFrames"); }
	catch ( ... ) {}
	try { _frameMargin = SCApp->configGetInt("streams.frameMargin"); }
	catch ( ... ) {}

	try { _rowHeight = SCApp->configGetInt("streams.height"); }
	catch ( ... ) {}

	try { _numberOfRows = SCApp->configGetInt("streams.rows"); }
	catch ( ... ) {}

	QPen defaultMinPen, defaultMaxPen;
	QBrush defaultMinBrush, defaultMaxBrush;
	OPT(double) defaultMinMaxMargin;

	try {
		defaultMinMaxMargin = SCApp->configGetDouble("streams.defaults.minMaxMargin");
	}
	catch ( ... ) {}

	try {
		defaultMinPen = SCApp->configGetPen("streams.defaults.minimum.pen", defaultMinPen);
	}
	catch ( ... ) {}
	try {
		defaultMaxPen = SCApp->configGetPen("streams.defaults.maximum.pen", defaultMaxPen);
	}
	catch ( ... ) {}

	try {
		defaultMinBrush = SCApp->configGetBrush("streams.defaults.minimum.brush", defaultMinBrush);
	}
	catch ( ... ) {}
	try {
		defaultMaxBrush = SCApp->configGetBrush("streams.defaults.maximum.brush", defaultMaxBrush);
	}
	catch ( ... ) {}

	try {
		vector<string> profiles = SCApp->configGetStrings("streams.profiles");
		for ( size_t i = 0; i < profiles.size(); ++i ) {
			string match;
			string prefix = "streams.profile." + profiles[i] + ".";

			try {
				match = SCApp->configGetString(prefix + "match");
			}
			catch ( ... ) {
				continue;
			}

			_decorationDescs.resize(_decorationDescs.size()+1);
			DecorationDesc &desc = _decorationDescs.back();
			desc.matchID = match;
			desc.minMaxMargin = defaultMinMaxMargin;
			try { desc.description = SCApp->configGetString(prefix + "description").c_str(); }
			catch ( ... ) {}
			try { desc.minMaxMargin = SCApp->configGetDouble(prefix + "minMaxMargin"); }
			catch ( ... ) {}
			try { desc.unit = SCApp->configGetString(prefix + "unit").c_str(); }
			catch ( ... ) {}
			try { desc.gain = SCApp->configGetDouble(prefix + "gain"); }
			catch ( ... ) {}
			try { desc.minValue = SCApp->configGetDouble(prefix + "minimum.value"); }
			catch ( ... ) {}
			try { desc.fixedScale = SCApp->configGetBool(prefix + "fixedScale"); }
			catch ( ... ) { desc.fixedScale = false; }
			try { desc.minPen = SCApp->configGetPen(prefix + "minimum.pen", defaultMinPen); }
			catch ( ... ) {}
			try { desc.minBrush = SCApp->configGetBrush(prefix + "minimum.brush", defaultMinBrush); }
			catch ( ... ) {}
			try { desc.maxValue = SCApp->configGetDouble(prefix + "maximum.value"); }
			catch ( ... ) {}
			try { desc.maxPen = SCApp->configGetPen(prefix + "maximum.pen", defaultMaxPen); }
			catch ( ... ) {}
			try { desc.maxBrush = SCApp->configGetBrush(prefix + "maximum.brush", defaultMaxBrush); }
			catch ( ... ) {}
		}
	}
	catch ( ... ) {}

	try {
		auto groups = SCApp->configGetStrings("streams.groups");
		for ( const auto &groupProfile : groups) {
			ChannelGroup group;
			try {
				auto members = SCApp->configGetStrings("streams.group." + groupProfile + ".members");
				for ( const auto &member : members ) {
					vector<string> toks;
					Core::split(toks, member, ".", false);
					if ( toks.size() > 4 ) {
						SEISCOMP_WARNING("Invalid group member '%s' in channel group '%s': "
						                 "expected not more than 4 tokens separated with dot: ignoring",
						                 member.c_str(), groupProfile.c_str());
						continue;
					}

					switch ( toks.size() ) {
						case 1:
							group.members.push_back(makeWaveformID(toks[0], "*", "*", "*"));
							break;
						case 2:
							group.members.push_back(makeWaveformID(toks[0], toks[1], "*", "*"));
							break;
						case 3:
							group.members.push_back(makeWaveformID(toks[0], toks[1], toks[2], "*"));
							break;
						case 4:
							group.members.push_back(makeWaveformID(toks[0], toks[1], toks[2], toks[3]));
							break;
						default:
							break;
					}
				}
			}
			catch ( ... ) {
				SEISCOMP_WARNING("Empty station group '%s': no members defined: ignoring",
				                 groupProfile.c_str());
			}

			try {
				group.title = SCApp->configGetString("streams.group." + groupProfile + ".title");
			}
			catch ( ... ) {}

			try {
				group.regionName = SCApp->configGetString("streams.group." + groupProfile + ".region");
			}
			catch ( ... ) {}

			group.pen.setColor(QColor());
			group.pen = SCApp->configGetPen("streams.group." + groupProfile + ".pen", group.pen);

			try {
				group.gradient = SCApp->configGetColorGradient("streams.group." + groupProfile + ".pen.gradient", group.gradient);
			}
			catch ( ... ) {}

			_channelGroups.push_back(group);
			_channelGroupLookup[groupProfile] = _channelGroups.size() - 1;
		}
	}
	catch ( ... ) {}

	addActions(_ui.menuEdit->actions());
	addActions(_ui.menuMode->actions());
	addActions(_ui.menuView->actions());

	selectModeNone();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
MainWindow::~MainWindow() {
	if ( _recordStreamThread ) {
		_recordStreamThread->stop(true);
		delete _recordStreamThread;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::start() {
	if ( !Settings::global.showPicks ) {
		_ui.actionViewPicks->setEnabled(false);
		_ui.actionViewArrivals->setEnabled(false);
	}

	if ( Settings::global.disableTimeWindowRequest ) {
		_ui.actionReload->setEnabled(false);
		_ui.actionSwitchToRealtime->setEnabled(false);
		_ui.actionLoadDataBack->setEnabled(false);
		_ui.actionLoadDataNext->setEnabled(false);
	}

	_dataFiles = SCApp->commandline().unrecognizedOptions();
	for ( auto it = _dataFiles.begin(); it != _dataFiles.end(); ) {
		if ( it->size() > 1 && (*it)[0] == '-' ) {
			it = _dataFiles.erase(it);
		}
		else {
			++it;
		}
	}

	if ( SCApp->commandline().hasOption("record-file") || !_dataFiles.empty() ) {
		try {
			_dataFiles.push_back(SCApp->commandline().option<std::string>("record-file"));
		}
		catch ( ... ) {}

		loadFiles();

		_ui.actionLoadDataBack->setEnabled(false);
		_ui.actionLoadDataNext->setEnabled(false);
		_ui.actionReload->setEnabled(false);
		_ui.actionSwitchToRealtime->setEnabled(false);
	}
	else {
		openAcquisition();
	}

	_sortMode = SortMode::Config;
	_sortLat = 0.0;
	_sortLon = 0.0;

	try {
		_sortLat = Gui::Application::Instance()->configGetDouble("streams.sort.latitude");
	}
	catch ( ... ) {}
	try {
		_sortLon = Gui::Application::Instance()->configGetDouble("streams.sort.longitude");
	}
	catch ( ... ) {}

	try {
		string sortMode = SCApp->configGetString("streams.sort.mode");
		if ( sortMode == "config" ) {
			_sortMode = SortMode::Config;
		}
		else if ( sortMode == "distance" ) {
			_sortMode = SortMode::Distance;
		}
		else if ( sortMode == "station" ) {
			_sortMode = SortMode::Station;
		}
		else if ( sortMode == "network" ) {
			_sortMode = SortMode::NetworkStation;
		}
		else if ( sortMode == "group" ) {
			_sortMode = SortMode::Group;
		}

		restoreDefaultSorting();
	}
	catch ( ... ) {}

	if ( Settings::global.autoApplyFilter ) {
		toggleFilter();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::restoreDefaultSorting() {
	switch ( _sortMode ) {
		case SortMode::Distance:
			sortByOrigin(_sortLat, _sortLon);
			break;
		case SortMode::Station:
			sortByStationCode();
			break;
		case SortMode::NetworkStation:
			sortByNetworkStationCode();
			break;
		case SortMode::Group:
			sortByGroup();
			break;
		default:
			sortByConfig();
			break;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
DataModel::ConfigStation* MainWindow::configStation(const string& networkCode,
                                                    const string& stationCode) const {
	if ( !SCApp->configModule() )
		return NULL;

	for ( size_t i = 0; i < SCApp->configModule()->configStationCount(); ++i ) {
		ConfigStation* cs = SCApp->configModule()->configStation(i);
		if ( cs->networkCode() == networkCode &&
		     cs->stationCode() == stationCode )
			return cs;
	}

	return NULL;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool MainWindow::isStationEnabled(const string& networkCode,
                                  const string& stationCode) const {
	ConfigStation* cs = configStation(networkCode, stationCode);
	return cs?cs->enabled():true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::setStationEnabled(const string& networkCode,
                                   const string& stationCode,
                                   bool enable) {
	updateTraces(networkCode, stationCode, enable);

	ConfigStation* cs = configStation(networkCode, stationCode);
	if ( !cs ) {
		ConfigModule *module = SCApp->configModule();
		if ( !module ) {
			_statusBarFile->setText("Settings the station state is disabled, no configmodule available");
			return;
		}

		ConfigStationPtr newCs = ConfigStation::Create("Config/" + module->name() + "/" + networkCode + "/" + stationCode);
		newCs->setNetworkCode(networkCode);
		newCs->setStationCode(stationCode);
		newCs->setEnabled(enable);

		CreationInfo ci;
		ci.setAuthor(SCApp->author());
		ci.setAgencyID(SCApp->agencyID());
		ci.setCreationTime(Core::Time::UTC());

		newCs->setCreationInfo(ci);

		Notifier::Enable();
		module->add(newCs.get());
		Notifier::Disable();

		cs = newCs.get();
	}

	if ( cs->enabled() != enable ) {
		cs->setEnabled(enable);
		SEISCOMP_INFO("Set station %s.%s state to: %d",
		              cs->networkCode().c_str(),
		              cs->stationCode().c_str(),
		              enable);

		CreationInfo *ci;
		try {
			ci = &cs->creationInfo();
			ci->setModificationTime(Core::Time::UTC());
		}
		catch ( ... ) {
			cs->setCreationInfo(CreationInfo());
			ci = &cs->creationInfo();
			ci->setCreationTime(Core::Time::UTC());
		}

		ci->setAuthor(SCApp->author());
		ci->setAgencyID(SCApp->agencyID());

		Notifier::Enable();
		cs->update();
		Notifier::Disable();
	}

	Core::MessagePtr msg = Notifier::GetMessage(true);

	if ( msg ) {
		SCApp->sendMessage(Settings::global.groupConfig.c_str(), msg.get());
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::updateTraces(const std::string &networkCode,
                              const std::string &stationCode,
                              bool enable) {
	QList<Seiscomp::Gui::RecordViewItem*> items;

	foreach ( TraceView *view, _traceViews ) {
		items += view->stationStreams(networkCode, stationCode);
	}

	if ( items.empty() ) {
		return;
	}

	_statusBarFile->setText(QString("Station %1.%2 has been %3 at %4 (localtime)")
	                        .arg(networkCode.c_str(), stationCode.c_str(),
	                             enable?"enabled":"disabled",
	                             Seiscomp::Core::Time::LocalTime().toString("%F %T").c_str()));

	if ( SCApp->nonInteractive() ) {
		foreach ( Seiscomp::Gui::RecordViewItem *item, items ) {
			if ( item->isInvisibilityForced() == enable ) {
				item->forceInvisibilty(!enable);
			}
		}
	}
	else {
		auto targetView = _traceViews[enable ? 0 : 1];

		foreach ( Seiscomp::Gui::RecordViewItem *item, items ) {
			if ( item->recordView() != targetView ) {
				if ( item->recordView()->takeItem(item) ) {
					targetView->addItem(item);
				}
				else {
					throw Core::GeneralException("Unable to take item");
				}
			}
		}

		if ( !_switchBack->isActive() ) {
			restoreDefaultSorting();
		}
		else {
			sortByOrigin(_originLat, _originLon);
		}
	}

	updateTraceCount();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::moveSelection(Seiscomp::Gui::RecordView* target,
                               Seiscomp::Gui::RecordView* source) {
	if ( target == source ) return;

	bool newState;
	if ( target == _traceViews[0] )
		newState = true;
	else if ( target == _traceViews[1] )
		newState = false;
	else
		throw Core::GeneralException("Unknown view");

	set< pair<string,string> > stations;
	QList<Seiscomp::Gui::RecordViewItem*> items = source->selectedItems();
	foreach(Seiscomp::Gui::RecordViewItem* item, items)
		stations.insert(pair<string,string>(item->streamID().networkCode(),
		                                    item->streamID().stationCode()));

	QString stationList("Stations to be changed:\n");
	for ( set< pair<string,string> >::iterator it = stations.begin();
	      it != stations.end(); ++it )
		stationList += QString("- %1.%2\n").arg(it->first.c_str()).arg(it->second.c_str());

	_questionApplyChanges.setInfo(stationList);

	if ( _questionApplyChanges.exec() == QDialog::Rejected )
		return;

	for ( set< pair<string,string> >::iterator it = stations.begin();
	      it != stations.end(); ++it )
		setStationEnabled(it->first, it->second, newState);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
TraceView* MainWindow::createTraceView() {
	TraceView* traceView = new TraceView(_bufferSize);
	traceView->setRowSpacing(_rowSpacing);
	traceView->setFramesEnabled(_withFrames);
	traceView->setFrameMargin(_frameMargin);
	if ( _rowHeight > 0 ) {
		traceView->setMinimumRowHeight(_rowHeight);
		traceView->setMaximumRowHeight(_rowHeight);
	}
	traceView->setRelativeRowHeight(_numberOfRows);
	traceView->setAlternatingRowColors(true);
	traceView->setAutoScale(true);
	traceView->setDefaultDisplay();
	traceView->setSelectionEnabled(false);
	traceView->setSelectionMode(Seiscomp::Gui::RecordView::ExtendedSelection);
	traceView->setDefaultItemColumns(4);
	traceView->layout()->setContentsMargins(6, 6, 6, 6);
	traceView->showScrollBar(!SCApp->nonInteractive());

	// determine the required label width
	int labelWidth=0;
	QFont f(traceView->font());
	QFontMetrics fm(f);
	labelWidth += fm.boundingRect("WW WW WWW").width();
	f.setBold(true);
	fm = QFontMetrics(f);
	labelWidth += fm.boundingRect("WWWW ").width();

	traceView->setLabelWidth(labelWidth);

	connect(traceView, SIGNAL(progressStarted()), _statusBarProg, SLOT(reset()));
	connect(traceView, SIGNAL(progressChanged(int)), _statusBarProg, SLOT(setValue(int)));
	connect(traceView, SIGNAL(progressFinished()), _statusBarProg, SLOT(reset()));

	connect(traceView->timeWidget(), SIGNAL(dragged(double)),
	        traceView, SLOT(move(double)));

	connect(traceView, SIGNAL(filterChanged(const QString&)),
	        this, SLOT(filterChanged(const QString&)));

	connect(_ui.actionToggleAllRecords, SIGNAL(toggled(bool)), traceView, SLOT(showAllRecords(bool)));
	connect(_ui.actionToggleRecordBorders, SIGNAL(toggled(bool)), traceView, SLOT(showRecordBorders(bool)));
	connect(_ui.actionToggleSpectrogram, SIGNAL(toggled(bool)), traceView, SLOT(toggleSpectrogram(bool)));

	connect(_ui.actionHorZoomIn, SIGNAL(triggered()), traceView, SLOT(horizontalZoomIn()));
	connect(_ui.actionHorZoomOut, SIGNAL(triggered()), traceView, SLOT(horizontalZoomOut()));
	connect(_ui.actionVerZoomIn, SIGNAL(triggered()), traceView, SLOT(verticalZoomIn()));
	connect(_ui.actionVerZoomOut, SIGNAL(triggered()), traceView, SLOT(verticalZoomOut()));

	//connect(_ui.actionAlignPickTime, SIGNAL(triggered()), _traceView, SLOT(setAlignPickTime()));
	connect(_ui.actionDefaultDisplay, SIGNAL(triggered()), traceView, SLOT(setDefaultDisplay()));
	//connect(_ui.actionFilter, SIGNAL(triggered()), _traceView, SLOT(openFilterDialog()));
	//connect(_ui.actionDisplay, SIGNAL(triggered()), _traceView, SLOT(openDisplayDialog()));

	connect(_ui.actionNormalizeVisibleAmplitudes, SIGNAL(toggled(bool)), this, SLOT(scaleVisibleAmplitudes(bool)));

	connect(traceView, SIGNAL(selectedTime(Seiscomp::Gui::RecordWidget*, Seiscomp::Core::Time)),
	        this, SLOT(selectedTime(Seiscomp::Gui::RecordWidget*, Seiscomp::Core::Time)));

	connect(traceView, SIGNAL(addedItem(const Seiscomp::Record*, Seiscomp::Gui::RecordViewItem*)),
	        this, SLOT(setupItem(const Seiscomp::Record*, Seiscomp::Gui::RecordViewItem*)));

	connect(traceView, SIGNAL(selectedRubberBand(QRect, QList<Seiscomp::Gui::RecordViewItem*>,
	                                             double, double,
	                                             Seiscomp::Gui::RecordView::SelectionOperation)),
	        this, SLOT(selectedTraceViewRubberBand(QRect, QList<Seiscomp::Gui::RecordViewItem*>,
	                                               double, double,
	                                               Seiscomp::Gui::RecordView::SelectionOperation)));

	if ( !_traceViews.empty() )
		traceView->copyState(_traceViews.front());

	_traceViews.append(traceView);

	return traceView;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::scaleVisibleAmplitudes(bool enable) {
	TRACEVIEWS(setAutoMaxScale(enable));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::listHiddenStreams() {
	Seiscomp::Gui::InfoText info(this);
	info.setWindowModality(Qt::WindowModal);
	info.setWindowTitle("Hidden streams");

	QString data;

	data = "<table>";
	data += "<tr><td><strong>ID</strong></td><td><strong>Delay</strong></td></tr>";

	int numberOfLines = 0;

	Core::Time now = Core::Time::UTC();
	foreach ( TraceView* view, _traceViews ) {
		for ( int i = 0; i < view->rowCount(); ++i ) {
			Seiscomp::Gui::RecordViewItem* item = view->itemAt(i);

			if ( item->isVisible() ) continue;

			Gui::RecordWidget *w = item->widget();
			OPT(Core::Time) lastSample;
			if ( w->records() ) {
				lastSample = w->records()->timeWindow().endTime();
			}

			QString state;

			if ( item->isInvisibilityForced() ) {
				state = "disabled";
			}

			if ( !lastSample ) {
				data += QString("<tr><td>%1</td><td></td><td>%2</td></tr>")
				                .arg(waveformIDToString(item->streamID())).arg(state);
			}
			else {
				data += QString("<tr><td>%1</td><td align=right>%2</td><td>%3</td></tr>")
				                .arg(waveformIDToString(item->streamID()))
				                .arg(strTimeSpan((now - *lastSample).seconds()))
				                .arg(state);
			}

			++numberOfLines;
		}
	}

	data += "</table>";

	if ( !numberOfLines ) return;

	info.setText(data);
	info.exec();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::updateMarkerVisibility() {
	for ( auto view : _traceViews ) {
		int rowCount = view->rowCount();
		for ( int r = 0; r < rowCount; ++r ) {
			auto item = view->itemAt(r);
			int markerCount = item->widget()->markerCount();
			for ( int i = 0; i < markerCount; ++i ) {
				auto marker = static_cast<TraceMarker*>(item->widget()->marker(i));
				marker->setVisible(
					(marker->isAssociated() && _ui.actionViewArrivals->isChecked()) ||
					(!marker->isAssociated() && _ui.actionViewPicks->isChecked())
				);
			}
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::removeTab(int index) {
	TraceView *traceView = static_cast<TraceView*>(_tabWidget->widget(index));
	_tabWidget->removeTab(index);

	if ( _tabWidget->count() == 0 )
		return;

	TraceView *mainView = static_cast<TraceView*>(_tabWidget->widget(0));

	_traceViews.remove(_traceViews.indexOf(traceView));
	traceView->moveItemsTo(mainView);
	delete traceView;

	if ( _tabWidget->count() == 1 ) {
		mainView->setParent(NULL);

		delete _tabWidget;
		_tabWidget = NULL;

		//setCentralWidget(mainView);
		centralWidget()->layout()->addWidget(mainView);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::setupItem(const Record*, Gui::RecordViewItem *item) {
	item->label()->setInteractive(false);
	item->label()->setOrientation(Qt::Horizontal);
	QFont f(item->label()->font(0));
	f.setBold(true);
	item->label()->setFont(f, 0);

	const WaveformStreamID &streamID = item->streamID();
	string streamIDStr = waveformIDToStdString(streamID);

	for ( size_t i = 0; i < _decorationDescs.size(); ++i ) {
		DecorationDesc &desc = _decorationDescs[i];
		if ( !Core::wildcmp(desc.matchID, streamIDStr) ) continue;

		TraceDecorator *deco = new TraceDecorator(item->widget(), &desc);
		item->widget()->setDecorator(deco);
		item->widget()->setDrawOffset(false);
		if ( desc.gain && *desc.gain != 0.0 )
			item->widget()->setRecordScale(0, 1.0 / *desc.gain);

		if ( desc.minValue && desc.maxValue && desc.minMaxMargin ) {
			double center = (*desc.minValue + *desc.maxValue) * 0.5;
			double vmin = (*desc.minValue-center)*(1+*desc.minMaxMargin)+center;
			double vmax = (*desc.maxValue-center)*(1+*desc.minMaxMargin)+center;
			if ( desc.fixedScale )
				item->widget()->setAmplRange(vmin / *item->widget()->recordScale(0),
				                             vmax / *item->widget()->recordScale(0));
			else
				item->widget()->setMinimumAmplRange(vmin / *item->widget()->recordScale(0),
				                                    vmax / *item->widget()->recordScale(0));
		}

		break;
	}

	item->setValue(DATA_GROUP, 0);
	item->setValue(DATA_GROUP_MEMBER, 0);

	for ( size_t s = 0; s < _channelGroups.size(); ++s ) {
		auto &group = _channelGroups[s];
		int memberIndex = 0;

		for ( size_t i = 0; i < group.members.size(); ++i ) {
			if ( compare(group.members[i], streamID) ) {
				memberIndex = -int(group.members.size()-i);
				break;
			}
		}

		if ( !memberIndex ) {
			bool insideRegion = false;

			if ( item->data().canConvert<TraceState>() ) {
				Seiscomp::Client::StationLocation loc = item->data().value<TraceState>().location;

				// Check if the region filter matches
				if ( !group.regionName.empty() ) {
					if ( Regions::getRegionName(loc.latitude, loc.longitude) == group.regionName ) {
						insideRegion = true;
					}
				}
			}

			if ( !insideRegion ) {
				continue;
			}
		}

		item->setValue(DATA_GROUP, -int(_channelGroups.size()-s));
		item->setValue(DATA_GROUP_INDEX, group.numberOfUsedChannels);
		item->setValue(DATA_GROUP_MEMBER, memberIndex);
		if ( group.pen.color().isValid() )
			item->widget()->setRecordPen(0, group.pen);
		if ( !group.title.empty() )
			item->setToolTip(group.title.c_str());

		if ( !group.gradient.isEmpty() )
			_needColorUpdate = true;

		// Increase number of used channels
		++group.numberOfUsedChannels;

		break;
	}

	item->widget()->showScaledValues(_ui.actionApplyGain->isChecked());

	if ( !_scaleMap.contains(streamID) ) {
		try {
			double scale = 1.0 / Client::Inventory::Instance()->getGain(
					streamID.networkCode(), streamID.stationCode(),
					streamID.locationCode(), streamID.channelCode(),
					Settings::global.endTime ? *Settings::global.endTime : Core::Time::UTC()
				);

			_scaleMap[streamID] = scale;
			item->widget()->setRecordScale(0, scale);
		}
		catch ( ... ) {}
	}

	item->label()->setText(streamID.stationCode().c_str(), 0);
	item->label()->setText(streamID.networkCode().c_str(), 1);
	item->label()->setText(streamID.locationCode().empty()?
	                       "  ":streamID.locationCode().c_str(), 2);
	item->label()->setText(streamID.channelCode().c_str(), 3);

	QFontMetrics fm(item->label()->font(0));
	item->label()->setWidth(fm.boundingRect("WWWW ").width(), 0);

	fm = QFontMetrics(item->label()->font(1));
	item->label()->setWidth(fm.boundingRect("WW ").width(), 1);

	fm = QFontMetrics(item->label()->font(1));
	item->label()->setWidth(fm.boundingRect("WW ").width(), 2);

	fm = QFontMetrics(item->label()->font(1));
	item->label()->setWidth(fm.boundingRect("WWW").width(), 3);

	item->setContextMenuPolicy(Qt::CustomContextMenu);

	if ( SCApp->nonInteractive() ) {
		return;
	}

	applySpectrogramSettings(static_cast<TraceWidget*>(item->widget()));

	item->setDraggingEnabled(true);
	connect(item, SIGNAL(customContextMenuRequested(const QPoint &)),
	        this, SLOT(itemCustomContextMenuRequested(const QPoint &)));
	connect(item, SIGNAL(clickedOnTime(Seiscomp::Gui::RecordViewItem*, Seiscomp::Core::Time)),
	        this, SLOT(createOrigin(Seiscomp::Gui::RecordViewItem*, Seiscomp::Core::Time)));
	connect(item->label(), SIGNAL(doubleClicked()), this, SLOT(changeTraceState()));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::itemCustomContextMenuRequested(const QPoint &p) {
	Gui::RecordViewItem *item = static_cast<Gui::RecordViewItem*>(sender());
	QPoint tracePos = item->widget()->mapFromGlobal(item->mapToGlobal(p));

	QMenu menu(this);

	QAction *artificialOrigin = NULL;

	if ( tracePos.x() >= 0 && tracePos.x() < item->widget()->width() )
		artificialOrigin = menu.addAction("Create artificial origin");

	if ( menu.isEmpty() ) return;

	QAction *action = menu.exec(item->mapToGlobal(p));

	if ( artificialOrigin && action == artificialOrigin ) {
		QPoint tracePos = item->widget()->mapFromGlobal(item->mapToGlobal(p));
		createOrigin(item, item->widget()->unmapTime(tracePos.x()));
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::changeTraceState() {
	Seiscomp::Gui::RecordLabel* label = (Seiscomp::Gui::RecordLabel*)sender();
	Seiscomp::Gui::RecordViewItem* item = label->recordViewItem();
	if ( item == NULL ) return;

	Seiscomp::Gui::RecordView* view = item->recordView();
	if ( view == NULL ) return;

	bool newState;
	if ( view == _traceViews[0] )
		newState = false;
	else if ( view == _traceViews[1] )
		newState = true;
	else
		throw Core::GeneralException("Unknown view");

	_questionApplyChanges.setInfo("Stations to be changed:\n" +
	                              QString(" - %1.%2")
	                               .arg(item->streamID().networkCode().c_str(),
	                                    item->streamID().stationCode().c_str()));

	if ( _questionApplyChanges.exec() == QDialog::Rejected )
		return;

	setStationEnabled(item->streamID().networkCode(),
	                  item->streamID().stationCode(),
	                  newState);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::createOrigin(Gui::RecordViewItem* item, Core::Time time) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	if ( item->data().type() == QVariant::Invalid ) {
#else
	if ( item->data().typeId() == QVariant::Invalid ) {
#endif
		if ( !_statusBarFile ) return;

		const double *v = item->widget()->value(time);
		if ( !v ) {
			_statusBarFile->setText("");
		}
		else {
			_statusBarFile->setText(QString("value = %1").arg(*v, 0, 'f', 2));
		}

		return;
	}

	Client::StationLocation loc = item->data().value<TraceState>().location;

	Gui::OriginDialog dlg(loc.longitude, loc.latitude, this);
	dlg.setTime(time);
	if ( dlg.exec() != QDialog::Accepted ) return;

	DataModel::Origin* origin = DataModel::Origin::Create();
	DataModel::CreationInfo ci;
	ci.setAgencyID(SCApp->agencyID());
	ci.setAuthor(SCApp->author());
	ci.setCreationTime(Core::Time::UTC());
	origin->setCreationInfo(ci);
	origin->setLongitude(dlg.longitude());
	origin->setLatitude(dlg.latitude());
	origin->setDepth(DataModel::RealQuantity(dlg.depth()));
	origin->setTime(Core::Time(dlg.getTime_t()));

	//Seiscomp::DataModel::ArtificialOriginMessage message(origin);
	//SCApp->sendMessage("GUI", &message);
	SCApp->sendCommand(Gui::CM_OBSERVE_LOCATION, "", origin);

	if ( Settings::global.automaticSortEnabled ) {
		_originLat = loc.latitude;
		_originLon = loc.longitude;
		sortByOrigin(_originLat, _originLon);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::loadFiles() {
	//setCentralWidget(createTraceView());
	centralWidget()->layout()->addWidget(createTraceView());

	while ( _tabWidget ) {
		removeTab(0);
	}

	TRACEVIEWS(setBufferSize(Core::TimeSpan(0,0)));

	_traceViews.front()->setAutoInsertItem(true);
	_traceViews.front()->clear();

	_traceViews.front()->setUpdatesEnabled(false);

	_lastRecordTime = Core::None;

	for ( size_t i = 0; i < _dataFiles.size(); ++i ) {
		RecordStream::File file;
		file.setRecordType("mseed");
		file.setStartTime(_dataTimeStart);
		file.setEndTime(_dataTimeEnd);

		if ( !file.setSource(_dataFiles[i]) ) {
			cerr << "could not open file '" << _dataFiles[i] << "'" << endl;
			continue;
		}

		try {
			std::string type = SCApp->commandline().option<std::string>("record-type");
			if ( !file.setRecordType(type.c_str()) ) {
				cerr << "unable to set recordtype '" << type << "'" << endl;
				continue;
			}
		}
		catch ( ... ) {}

		IO::RecordInput input(&file, Array::FLOAT, Record::DATA_ONLY);

		cout << "loading " << _dataFiles[i] << "..." << flush;
		Util::StopWatch t;

		for ( Record *rec : input ) {
			if ( _traceViews.front()->feed(rec) ) {
				if ( !_lastRecordTime || *_lastRecordTime < rec->endTime() ) {
					_lastRecordTime = rec->endTime();
				}
			}
		}

		cout << "(" << t.elapsed() << " sec)" << endl;
	}

	Core::TimeWindow tw = _traceViews.front()->coveredTimeRange();
	_originTime = tw.endTime();
	_bufferSize = tw.endTime() - tw.startTime();

	TRACEVIEWS(setAlignment(_originTime));
	TRACEVIEWS(setBufferSize(tw.length()));
	TRACEVIEWS(setTimeSpan(tw.length()));
	TRACEVIEWS(setTimeRange(-static_cast<double>(tw.length()), 0));

	alignRight();
	colorByConfig();

	_traceViews.front()->setUpdatesEnabled(true);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::openAcquisition() {
	_originTime = Settings::global.endTime ? *Settings::global.endTime : Core::Time::UTC();
	_dataTimeStart = _originTime - _bufferSize;
	_dataTimeEnd = Settings::global.endTime;

	TRACEVIEWS(setBufferSize(_bufferSize));
	TRACEVIEWS(setTimeSpan(_bufferSize));

	centralWidget()->layout()->addWidget(createTraceView());

	while ( _tabWidget != NULL )
		removeTab(0);

	if ( !SCApp->nonInteractive() ) {
		// Create the Enabled/Disabled tabs
		addTabulator();

		_tabWidget->setTabText(_tabWidget->indexOf(_traceViews[0]), "Enabled");
		_tabWidget->setTabIcon(_tabWidget->indexOf(_traceViews[0]), QIcon(":icons/icons/enabled.png"));
		_tabWidget->setTabText(_tabWidget->indexOf(_traceViews[1]), "Disabled");
		_tabWidget->setTabIcon(_tabWidget->indexOf(_traceViews[1]), QIcon(":icons/icons/disabled.png"));

		_tabWidget->setCurrentIndex(0);
	}

	if ( !Settings::global.inventoryDisabled ) {
		typedef QPair<WaveformStreamID, int> WIDWithIndex;
		QList<WIDWithIndex> requestMap;
		QList<WaveformStreamID> resolvedStationRequestMap;

		bool usePreconfigured = false;

		std::vector<std::string> streamsBlackList;
		try {
			streamsBlackList = SCApp->configGetStrings("streams.blacklist");
		}
		catch ( ... ) {}

		try {
			if ( Settings::global.vstreams.empty() ) {
				usePreconfigured = true;
			}
			else if ( Settings::global.vstreams[0] == "default" ) {
				usePreconfigured = true;
			}

			int index = 0;
			for ( auto &stream : Settings::global.vstreams ) {
				if ( stream == "default" ) {
					continue;
				}

				auto it = _channelGroupLookup.find(stream);
				if ( it != _channelGroupLookup.end() ) {
					auto &group = _channelGroups[it->second];
					for ( auto &member : group.members ) {
						requestMap.append(WIDWithIndex(member, index++));
					}
					continue;
				}

				vector<string> tokens;
				Core::split(tokens, stream, ".", false);

				if ( tokens.empty() ) {
					continue;
				}

				if ( tokens.size() > 4 ) {
					cerr << "error in entry '" << stream << "': too many tokens, missing ',' ? -> ignoring" << endl;
					continue;
				}

				requestMap.append(
					WIDWithIndex(
						WaveformStreamID(tokens[0],
						                 tokens.size() > 1 ? tokens[1] : "*",
						                 tokens.size() > 2 ? tokens[2] : "*",
						                 tokens.size() > 3 ? tokens[3] : "*", ""),
						index++
					)
				);
			}
		}
		catch ( ... ) {
			usePreconfigured = true;
		}

		if ( usePreconfigured ) {
			cout << "using configured streams of config module" << endl;
			//streamMap["*"]["*"].insert("*", ChannelEntry("*",0));

			auto module = SCApp->configModule();

			if ( module ) {
				int index = 0;

				for ( size_t j = 0; j < module->configStationCount(); ++j ) {
					DataModel::ConfigStation *station = module->configStation(j);
					DataModel::Setup *setup = DataModel::findSetup(station, SCApp->name());
					if ( setup ) {
						DataModel::ParameterSet* ps = DataModel::ParameterSet::Find(setup->parameterSetID());
						if ( !ps ) {
							SEISCOMP_ERROR("Cannot find parameter set %s", setup->parameterSetID().c_str());
							continue;
						}

						Util::KeyValues params;
						params.init(ps);

						std::string net, sta, loc, cha;
						net = station->networkCode();
						sta = station->stationCode();

						params.getString(loc, "detecLocid");
						params.getString(cha, "detecStream");

						if ( cha.empty() ) {
							continue;
						}

						SensorLocation *sloc = Client::Inventory::Instance()->getSensorLocation(net, sta, loc, _originTime);

						if ( Settings::global.threeComponents ) {
							if ( !sloc ) {
								SEISCOMP_WARNING("%s.%s.%s: meta data not found: cannot resolve 3C",
								                 net, sta, loc);
							}
							else {
								ThreeComponents tc;
								getThreeComponents(tc, sloc, cha.substr(0, 2).data(), _originTime);
								for ( size_t i = 0; i < 3; ++i ) {
									if ( tc.comps[i] ) {
										cha = tc.comps[i]->code();
										if ( isStreamBlacklisted(streamsBlackList, net, sta, loc, cha) ) {
											continue;
										}
										requestMap.append(WIDWithIndex(WaveformStreamID(net, sta, loc, cha, ""), index++));
									}
								}
								continue;
							}
						}

						if ( cha.size() < 3 ) {
							char compCode = 'Z';

							//cerr << " * " << net << "." << sta << "." << loc << "." << cha << 'Z' << endl;
							if ( sloc ) {
								auto stream = getVerticalComponent(sloc, cha.c_str(), _originTime);
								if ( stream && !stream->code().empty() ) {
									cha = stream->code();
								}
								else {
									cha += compCode;
								}
							}
							else {
								cha += compCode;
							}
						}

						if ( isStreamBlacklisted(streamsBlackList, net, sta, loc, cha) ) {
							continue;
						}

						requestMap.append(WIDWithIndex(WaveformStreamID(net, sta, loc, cha, ""), index++));
					}
				}
			}
		}

		QRectF regionRect;

		double latMin = -90.0;
		double latMax = 90.0;
		double lonMin = -180.0;
		double lonMax = 180.0;
		try {
			lonMin = Gui::Application::Instance()->configGetDouble("streams.region.lonmin");
		}
		catch ( ... ) {
		}
		try {
			lonMax = Gui::Application::Instance()->configGetDouble("streams.region.lonmax");
		}
		catch ( ... ) {
		}
		try {
			latMin = Gui::Application::Instance()->configGetDouble("streams.region.latmin");
		}
		catch ( ... ) {
		}
		try {
			latMax = Gui::Application::Instance()->configGetDouble("streams.region.latmax");
		}
		catch ( ... ) {
		}

		if ( lonMin > 180 ) {
			lonMin -= 360.0;
		}
		if ( lonMin < -180 ) {
			lonMin += 360.0;
		}
		if ( lonMax > 180 ) {
			lonMax -= 360.0;
		}
		if ( lonMax < -180 ) {
			lonMax += 360.0;
		}
		double latDelta = 0.0;
		latDelta = lonMax-lonMin;
		if ( latDelta < 0 ) {
			latDelta += 360.;
		}
		try {
			if ( latMin != -90.0 || latMax != 90.0 || lonMin != -180.0 || lonMax != 180.0 ) {
				regionRect.setRect(lonMin, latMin, latDelta, latMax-latMin);
			}
		}
		catch ( ... ) {
		}

		TRACEVIEWS(clear());
		TRACEVIEWS(setAutoInsertItem(false));

		if ( _recordStreamThread )
			_recordStreamThread->stop(true);

		Inventory* inv = Seiscomp::Client::Inventory::Instance()->inventory();
		if ( inv == NULL ) {
			QMessageBox::information(this, "Error", "Could not read inventory (NULL)");
			return;
		}

		for ( size_t i = 0; i < inv->networkCount(); ++i ) {
			Network* net = inv->network(i);
			try {
				if ( net->end() < _originTime ) continue;
			}
			catch ( ... ) {}

			QList<WIDWithIndex> staRequests;
			foreach ( const WIDWithIndex &wfsi, requestMap ) {
				if ( Core::wildcmp(wfsi.first.networkCode(), net->code()) ) {
					staRequests.append(wfsi);
				}
			}
			if ( staRequests.isEmpty() ) continue;

			for ( size_t j = 0; j < net->stationCount(); ++j ) {
				Station* sta = net->station(j);
				try {
					if ( sta->end() < _originTime ) continue;
				}
				catch ( ... ) {}

				if ( !regionRect.isEmpty() ) {
					try {
						if ( !regionRect.contains(sta->longitude(), sta->latitude()) )
							continue;
					}
					catch ( ... ) {
						continue;
					}
				}

				QList<WIDWithIndex> locRequests;
				foreach ( const WIDWithIndex &wfsi, staRequests ) {
					if ( isWildcard(wfsi.first.stationCode()) ) {
						if ( Core::wildcmp(wfsi.first.stationCode(), sta->code()) ) {
							locRequests.append(wfsi);

							WaveformStreamID nwfsi(wfsi.first);
							nwfsi.setNetworkCode(net->code());
							nwfsi.setStationCode(sta->code());
							resolvedStationRequestMap.append(nwfsi);
						}
					}
					else {
						if ( wfsi.first.stationCode() == sta->code() ) {
							locRequests.append(wfsi);

							WaveformStreamID nwfsi(wfsi.first);
							nwfsi.setNetworkCode(net->code());
							nwfsi.setStationCode(sta->code());
							resolvedStationRequestMap.append(nwfsi);
						}
					}
				}
				if ( locRequests.isEmpty() ) continue;

				for ( size_t l = 0; l < sta->sensorLocationCount(); ++l ) {
					SensorLocation *loc = sta->sensorLocation(l);

					try {
						if ( loc->end() < _originTime ) continue;
					}
					catch ( ... ) {}

					QList<WIDWithIndex> chaRequests;
					foreach ( const WIDWithIndex &wfsi, locRequests ) {
						if ( Core::wildcmp(wfsi.first.locationCode(), loc->code()) ) {
							chaRequests.append(wfsi);
						}
					}
					if ( chaRequests.isEmpty() ) continue;

					for ( size_t s = 0; s < loc->streamCount(); ++s ) {
						Stream* stream = loc->stream(s);

						try {
							if ( stream->end() < _originTime ) continue;
						}
						catch ( ... ) {}

						bool foundChaCode = false;
						int index = 0;

						foreach ( const WIDWithIndex &wfsi, chaRequests ) {
							if ( Core::wildcmp(wfsi.first.channelCode(), stream->code()) ) {
								foundChaCode = true;
								index = wfsi.second;
								break;
							}
						}

						if ( foundChaCode ) {
							double scale = 1.0;
							try { scale = 1.0 / stream->gain(); }
							catch ( ... ) {}

							_waveformStreams.insert(WaveformStreamEntry(
							                          WaveformStreamID(net->code(), sta->code(),
							                                           loc->code(), stream->code(),
							                                           ""), index, scale));
							/*
							cerr << " + " << net->code() << "." << sta->code()
							     << "." << loc->code() << "." << stream->code() << endl;
							*/
						}
					}
				}
			}
		}

		TRACEVIEWS(hide());

		//QProgressDialog progress(this);
		//progress.setWindowTitle(tr("Please wait..."));
		//progress.setRange(0, _waveformStreams.size());

		SCApp->showMessage(QString("Added 0/%1 streams")
		                   .arg(_waveformStreams.size()).toLatin1());
		cout << "Adding " << _waveformStreams.size() << " streams" << endl;

		//ofstream of("streams");

		int count = 0;
		for ( WaveformStreamSet::iterator it = _waveformStreams.begin();
		      it != _waveformStreams.end(); ++it, ++count ) {
			SEISCOMP_DEBUG("Adding row: %s.%s.%s.%s", it->streamID.networkCode().c_str(),
			               it->streamID.stationCode().c_str(), it->streamID.locationCode().c_str(),
			               it->streamID.channelCode().c_str());

			bool stationEnabled = isStationEnabled(it->streamID.networkCode(), it->streamID.stationCode());

			// Later on the items should be sorted into the right tabs
			TraceView *view = _traceViews[SCApp->nonInteractive()?0:(stationEnabled?0:1)];

			Seiscomp::Gui::RecordViewItem* item = view->addItem(it->streamID, waveformIDToString(it->streamID));

			if ( item == NULL ) continue;

			if ( SCApp->nonInteractive() )
				item->forceInvisibilty(!stationEnabled);

			_scaleMap[item->streamID()] = it->scale;
			item->widget()->setRecordScale(0, it->scale);

			QPalette pal = item->widget()->palette();
			pal.setColor(QPalette::WindowText, QColor(128,128,128));
			pal.setColor(QPalette::HighlightedText, QColor(128,128,128));
			item->widget()->setPalette(pal);

			try {
				TraceState state;
				state.location = Seiscomp::Client::Inventory::Instance()->stationLocation(
					it->streamID.networkCode(),
					it->streamID.stationCode(),
					_originTime
				);

				QVariant d;
				d.setValue(state);

				item->setData(d);
			}
			catch ( ... ) {}

			setupItem(NULL, item);

			item->setValue(DATA_INDEX, (*it).index);
			item->setValue(DATA_DELTA, 0.0);

			SCApp->showMessage(QString("Added %1/%2 streams")
			                   .arg(count+1)
			                   .arg(_waveformStreams.size()).toLatin1());
			//progress.setValue(progress.value()+1);
			//progress.update();
		}

		foreach ( const WaveformStreamID &wfsi, resolvedStationRequestMap ) {
			if ( isWildcard(wfsi.networkCode()) || isWildcard(wfsi.stationCode()) ) {
				continue;
			}
			_channelRequests.append(wfsi);
		}

		sortByConfig();
		colorByConfig();
		TRACEVIEWS(show());
	}
	else {
		while ( _tabWidget != NULL )
			removeTab(0);

		if ( _recordStreamThread )
			_recordStreamThread->stop(true);

		try {
			for ( auto &stream : Settings::global.vstreams ) {
				auto it = _channelGroupLookup.find(stream);
				if ( it != _channelGroupLookup.end() ) {
					auto &group = _channelGroups[it->second];
					for ( auto &member : group.members ) {
						_channelRequests.append(member);
					}
					continue;
				}

				if ( stream == "default" ) {
					continue;
				}

				vector<string> tokens;
				Core::split(tokens, stream, ".", false);

				if ( tokens.empty() ) {
					continue;
				}

				if ( tokens.size() > 4 ) {
					cout << "error in entry '" << stream << "': too many tokens, missing ',' ? -> ignoring" << endl;
				}
				else {
					_channelRequests.append(
						WaveformStreamID(
							tokens[0],
							tokens.size() > 1 ? tokens[1] : "*",
							tokens.size() > 2 ? tokens[2] : "*",
							tokens.size() > 3 ? tokens[3] : "*",
							string()
						)
					);
				}
			}
		}
		catch ( ... ) {}

		TRACEVIEWS(clear());
		TRACEVIEWS(setAutoInsertItem(true));
		TRACEVIEWS(show());
	}

	TRACEVIEWS(setJustification(1.0));
	TRACEVIEWS(horizontalZoom(1.0));
	TRACEVIEWS(setAlignment(_originTime));

	_timer->start();

	alignRight();
	checkTraceDelay();
	updateTraceCount();

	reloadData();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::openFile(const std::string &filename) {
	qApp->setOverrideCursor(Qt::WaitCursor);

	IO::XMLArchive ar;
	ar.open(filename.c_str());

	bool regWasEnabled = PublicObject::IsRegistrationEnabled();
	PublicObject::SetRegistrationEnabled(false);

	DataModel::EventParametersPtr ep;
	ar >> ep;

	PublicObject::SetRegistrationEnabled(regWasEnabled);

	if ( !ep ) {
		qApp->restoreOverrideCursor();
		QMessageBox::information(this, tr("Error"), tr("No event parameters found in XML file."));
		return;
	}

	cerr << "Loaded " << ep->pickCount() << " picks" << endl;

	map<string, int> arrivalPicks;

	for ( size_t i = 0; i < ep->originCount(); ++i ) {
		auto org = ep->origin(i);
		try {
			if ( org->evaluationStatus() == REJECTED ) {
				continue;
			}
		}
		catch ( ... ) {}

		for ( size_t j = 0; j < org->arrivalCount(); ++j ) {
			auto arr = org->arrival(j);
			auto itp = arrivalPicks.insert(map<string, int>::value_type(arr->pickID(), 0));
			++itp.first->second;
		}
	}


	int accepted = 0, rejected = 0;
	for ( size_t i = 0; i < ep->pickCount(); ++i ) {
		int refCount;

		auto it = arrivalPicks.find(ep->pick(i)->publicID());
		if ( it != arrivalPicks.end() ) {
			refCount = it->second;
		}
		else {
			refCount = 0;
		}

		auto registeredPick = Pick::Find(ep->pick(i)->publicID());
		if ( registeredPick ) {
			++rejected;
			SC_FMT_WARNING("{}: pick already loaded", ep->pick(i)->publicID());
			continue;
		}

		ep->pick(i)->registerMe();

		if ( addPick(ep->pick(i), refCount) ) {
			++accepted;
		}
		else {
			++rejected;
		}
	}

	qApp->restoreOverrideCursor();

	QMessageBox::information(this, "Load XML", tr("Added %1/%2 picks")
	                         .arg(accepted).arg(accepted+rejected));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::openXML() {
	QString filename = QFileDialog::getOpenFileName(this,
		tr("Open XML file"), "", tr("XML files (*.xml);;All (*.*)"));

	if ( filename.isEmpty() ) return;

	openFile(filename.toStdString());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::selectStreams() {
	Gui::InventoryListView* list = new Gui::InventoryListView(this, Qt::Window);
	list->setAttribute(Qt::WA_DeleteOnClose);

	for ( WaveformStreamSet::iterator it = _waveformStreams.begin();
	      it != _waveformStreams.end(); ++it ) {
		list->selectStream(waveformIDToString(it->streamID), true);
	}

	list->show();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::addTabulator() {
	if ( !_tabWidget ) {
		_tabWidget = new TraceTabWidget(this);

		SCScheme.applyTabPosition(_tabWidget);

		if ( !_traceViews.isEmpty() )
			_traceViews.front()->setParent(NULL);
		else
			createTraceView();

		_tabWidget->addTab(_traceViews.front(), "Default");

		connect(_tabWidget, SIGNAL(tabRemovalRequested(int)),
		        this, SLOT(removeTab(int)));

		connect(_tabWidget, SIGNAL(moveSelectionRequested(Seiscomp::Gui::RecordView*,
		                                                  Seiscomp::Gui::RecordView*)),
		        this, SLOT(moveSelection(Seiscomp::Gui::RecordView*,
		                                 Seiscomp::Gui::RecordView*)));

		//setCentralWidget(_tabWidget);
		centralWidget()->layout()->addWidget(_tabWidget);
	}

	Seiscomp::Gui::RecordView* source = static_cast<Seiscomp::Gui::RecordView*>(_tabWidget->currentWidget());
	Seiscomp::Gui::RecordView* target = createTraceView();
	source->moveSelectionTo(target);
	_tabWidget->addTab(target, "New Tab");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::filterChanged(const QString &s) {
	Settings::global.filters.push_back(s.toStdString());
	_statusBarFilter->blockSignals(true);
	_statusBarFilter->addItem(s, s);
	_statusBarFilter->setCurrentIndex(_statusBarFilter->count() - 1);
	_statusBarFilter->blockSignals(false);
	filterSelectionChanged();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::enableSearch() {
	_statusBarSearch->selectAll();
	_statusBarSearch->setVisible(true);
	_statusBarSearch->setFocus();

	// To avoid shortcut ambiguities
	_ui.actionModeNone->setEnabled(false);
	_ui.actionAbortSearch->setEnabled(true);

	foreach ( TraceView* view, _traceViews ) {
		view->setFocusProxy(_statusBarSearch);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::searchByText(const QString &text) {
	if ( text.isEmpty() ) return;

	QRegularExpression rx = QRegularExpression(
		QRegularExpression::wildcardToRegularExpression(text + "*"),
		QRegularExpression::CaseInsensitiveOption
	);

	TraceView *current = nullptr, *found = nullptr;

	if ( _tabWidget ) {
		current = _traceViews[_tabWidget->currentIndex()];
	}
	else {
		current = _traceViews[0];
	}

	foreach ( TraceView *view, _traceViews ) {
		view->deselectAllItems();
		int row = view->findByText(0, rx, view == current?_lastFoundRow+1:0);
		if ( row != -1 ) {
			view->setItemSelected(view->itemAt(row), true);
			if ( found == NULL || current == view ) {
				found = view;
				_lastFoundRow = row;
			}
		}
	}

	if ( !found ) {
		QPalette pal = _statusBarSearch->palette();
		pal.setColor(QPalette::Base, _searchError);
		_statusBarSearch->setPalette(pal);
		_lastFoundRow = -1;
	}
	else {
		if ( found != current && _tabWidget )
			_tabWidget->setCurrentIndex(_tabWidget->indexOf(found));

		QPalette pal = _statusBarSearch->palette();
		pal.setColor(QPalette::Base, _searchBase);
		_statusBarSearch->setPalette(pal);

		found->ensureVisible(_lastFoundRow);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::search(const QString &text) {
	_lastFoundRow = -1;

	searchByText(text);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::nextSearch() {
	searchByText(_statusBarSearch->text());
	if ( _lastFoundRow == -1 )
		searchByText(_statusBarSearch->text());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::abortSearch() {
	// To avoid shortcut ambiguities
	_ui.actionModeNone->setEnabled(true);
	_ui.actionAbortSearch->setEnabled(false);

	if ( _statusBarSearch->isVisible() ) {
		_statusBarSearch->setVisible(false);
		_statusBarSearch->releaseKeyboard();

		foreach ( TraceView *view, _traceViews )
			view->setFocusProxy(NULL);
	}
	else {
		clearSelection();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::clearSelection() {
	foreach ( TraceView *view, _traceViews ) {
		view->deselectAllItems();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::applySpectrogramSettings() {
	foreach ( TraceView *view, _traceViews ) {
		for ( int i = 0; i < view->rowCount(); ++i ) {
			auto item = view->itemAt(i);
			applySpectrogramSettings(static_cast<TraceWidget*>(item->widget()));
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::applySpectrogramSettings(TraceWidget *traceWidget) {
	auto spectrogram = traceWidget->spectrogram();
	spectrogram->setSmoothTransform(_spectrogramSettings->ui.cbSmoothing->isChecked());
	switch ( _spectrogramSettings->ui.cbNormalization->currentIndex() ) {
		default:
		case 0:
			spectrogram->setNormalizationMode(SpectrogramRenderer::NormalizationMode::Fixed);
			break;
		case 1:
			spectrogram->setNormalizationMode(SpectrogramRenderer::NormalizationMode::Frequency);
			break;
		case 2:
			spectrogram->setNormalizationMode(SpectrogramRenderer::NormalizationMode::Time);
			break;
	}

	spectrogram->setLogScale(_spectrogramSettings->ui.cbLogScale->isChecked());
	spectrogram->setGradientRange(
		_spectrogramSettings->ui.spinMinAmp->value(),
		_spectrogramSettings->ui.spinMaxAmp->value()
	);
	spectrogram->setFrequencyRange(
		_spectrogramSettings->ui.spinMinFrequency->value() == 0
		? Core::None
		: OPT(double)(_spectrogramSettings->ui.spinMinFrequency->value()),
		_spectrogramSettings->ui.spinMaxFrequency->value() == 0
		? Core::None
		: OPT(double)(_spectrogramSettings->ui.spinMaxFrequency->value())
	);
	auto options = spectrogram->options();
	options.windowLength = _spectrogramSettings->ui.spinTimeWindow->value();
	options.windowOverlap = _spectrogramSettings->ui.spinOverlap->value() * 0.01;
	spectrogram->setOptions(options);
	traceWidget->resetSpectrogram();
	traceWidget->setShowSpectrogramAxis(_spectrogramSettings->ui.cbShowAxis->isChecked());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::checkTraceDelay() {
	if ( Settings::global.maxDelay == 0 ) {
		return;
	}

	Core::Time now = Core::Time::UTC();
	Core::TimeSpan maxDelay = Core::TimeSpan(Settings::global.maxDelay, 0);
	foreach ( TraceView* view, _traceViews ) {
		for ( int i = 0; i < view->rowCount(); ++i ) {
			Seiscomp::Gui::RecordViewItem* item = view->itemAt(i);
			Seiscomp::Gui::RecordWidget *w = item->widget();
			OPT(Seiscomp::Core::Time) lastSample;

			if ( w->records() ) {
				lastSample = w->records()->timeWindow().endTime();
			}

			if ( lastSample ) {
				item->setVisible(now - *lastSample <= maxDelay);
			}
			else {
				item->setVisible(now - _dataTimeStart <= maxDelay);
			}
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::updateTraceCount() {
	if ( !_tabWidget ) {
		return;
	}

	if ( _traceViews.count() > 0 ) {
		int idx = _tabWidget->indexOf(_traceViews[0]);
		if ( idx >= 0 ) {
			_tabWidget->setTabText(idx, QString("Enabled (%1)").arg(_traceViews[0]->rowCount()));
		}
	}

	if ( _traceViews.count() > 1 ) {
		int idx = _tabWidget->indexOf(_traceViews[1]);
		if ( idx >= 0 ) {
			_tabWidget->setTabText(idx, QString("Disabled (%1)").arg(_traceViews[1]->rowCount()));
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::selectModeNone() {
	clearSelection();
	TRACEVIEWS(restoreSelectionMode());
	_statusBarSelectMode->blockSignals(true);
	_statusBarSelectMode->setCurrentIndex(MODE_STANDARD);
	_statusBarSelectMode->blockSignals(false);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::selectModeZoom(bool allowToggle) {
	if ( SCApp->nonInteractive() ) {
		return;
	}

	if ( allowToggle && (_statusBarSelectMode->currentIndex() == MODE_ZOOM) ) {
		selectModeNone();
	}
	else {
		TRACEVIEWS(setZoomEnabled());
		_statusBarSelectMode->blockSignals(true);
		_statusBarSelectMode->setCurrentIndex(MODE_ZOOM);
		_statusBarSelectMode->blockSignals(false);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::selectModePicks(bool allowToggle) {
	if ( SCApp->nonInteractive() ) {
		return;
	}

	if ( allowToggle && (_statusBarSelectMode->currentIndex() == MODE_PICKS) ) {
		selectModeNone();
	}
	else {
		TRACEVIEWS(setRubberBandSelectionEnabled());
		_statusBarSelectMode->blockSignals(true);
		_statusBarSelectMode->setCurrentIndex(MODE_PICKS);
		_statusBarSelectMode->blockSignals(false);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::selectMode(int mode) {
	switch ( mode ) {
		case 0:
			selectModeNone();
			break;
		case 1:
			selectModePicks(false);
			break;
		case 2:
			selectModeZoom(false);
			break;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::filterSelectionChanged() {
	if ( _statusBarFilter->currentIndex() > 0 ) {
		TRACEVIEWS(setFilterByName(_statusBarFilter->currentData().toString().toStdString().c_str()));
		TRACEVIEWS(enableFilter(true));
		TRACEVIEWS(toggleFilter(true));
		_lastFilterIndex = _statusBarFilter->currentIndex();
		_ui.actionPreviousFilter->setEnabled(Settings::global.filters.size() > 1);
		_ui.actionNextFilter->setEnabled(Settings::global.filters.size() > 1);
	}
	else {
		TRACEVIEWS(setFilter(nullptr));
		TRACEVIEWS(enableFilter(false));
		TRACEVIEWS(toggleFilter(false));
		_ui.actionPreviousFilter->setEnabled(false);
		_ui.actionNextFilter->setEnabled(false);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::reload() {
	Core::TimeWindow dataTimeWindow;
	CURRENT_TRACEVIEW_RET(dataTimeWindow, visibleTimeRange());

	if ( dataTimeWindow.length() >= Settings::global.warnDataTimeRange ) {
		if ( QMessageBox::question(this, "Load data",
		                           tr("Data range exceeds %1.\n"
		                              "Do you want to continue?")
		                           .arg(prettyTimeRange(Settings::global.warnDataTimeRange.seconds())),
		                           QMessageBox::Yes, QMessageBox::No) == QMessageBox::No ) {
			return;
		}
	}

	reloadTimeWindow(dataTimeWindow);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::switchToRealtime() {
	Settings::global.endTime = Core::None;
	// Reset buffer size to initial config
	_bufferSize = Core::TimeSpan(Settings::global.bufferSize, 0);
	_originTime = Core::Time::UTC();
	_dataTimeStart = _originTime - _bufferSize;
	_dataTimeEnd = Settings::global.endTime;

	TRACEVIEWS(setBufferSize(_bufferSize));
	TRACEVIEWS(setTimeSpan(_bufferSize));
	TRACEVIEWS(setAlignment(_originTime));
	TRACEVIEWS(setTimeRange(-_bufferSize.length(), 0));
	_wantReload = true;

	if ( _recordStreamThread ) {
		_statusBarFile->setText(tr("Waiting for recordstream to finish"));
		_recordStreamThread->stop(false);
		return;
	}

	reloadData();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::reloadTimeWindow(const Core::TimeWindow &tw) {
	_dataTimeStart = tw.startTime();
	_dataTimeEnd = tw.endTime();
	Settings::global.endTime = _dataTimeEnd;
	_bufferSize = *_dataTimeEnd - _dataTimeStart;
	_originTime = *_dataTimeEnd;

	TRACEVIEWS(setBufferSize(_bufferSize));
	TRACEVIEWS(setTimeSpan(_bufferSize));
	TRACEVIEWS(setAlignment(_originTime));
	TRACEVIEWS(setTimeRange(-static_cast<double>(_bufferSize), 0));
	_wantReload = true;

	if ( _recordStreamThread ) {
		_statusBarFile->setText(tr("Waiting for recordstream to finish"));
		_recordStreamThread->stop(false);
		return;
	}

	reloadData();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::reloadData() {
	_wantReload = false;
	_ui.actionSwitchToRealtime->setEnabled(static_cast<bool>(Settings::global.endTime));

	_statusBarFile->setText(tr("Loading picks"));

	TRACEVIEWS(clearRecords());
	clearPickMarkers();

	if ( Settings::global.showPicks && SCApp->query() ) {
		SCApp->showMessage("Loading picks");
		DatabaseIterator it = loadPicks(_dataTimeStart, _dataTimeEnd);

		auto db = SCApp->query()->driver();
		int picks = 0, arrivals = 0;
		for ( ; *it; ++it ) {
			PickPtr pick = Pick::Cast(*it);
			if ( pick ) {
				auto idx = db->findColumn("refCount");
				int refCount = -1;
				if ( idx >= 0 ) {
					if ( !Core::fromString(refCount, db->getRowFieldString(idx)) ) {
						refCount = -1;
					}
				}

				if ( addPick(pick.get(), refCount) ) {
					++picks;
					if ( refCount > 0 ) {
						++arrivals;
					}
				}
			}
		}

		SCApp->showMessage(QString("Loaded %1 picks").arg(it.count()).toLatin1());
		cerr << "Got " << it.count() << " picks from database" << endl;
		cerr << "Accepted " << picks << " picks" << endl;
		cerr << "Declared " << arrivals << " arrivals" << endl;
	}

	_statusBarFile->setText(QString());

	_recordStreamThread = new RecordStreamThread(SCApp->recordStreamURL());
	if ( !_recordStreamThread->connect() ) {
		delete _recordStreamThread;
		_recordStreamThread = nullptr;
		QMessageBox::critical(
			this, tr("Error"),
			tr("Could not connect to stream '%1'")
			.arg(SCApp->recordStreamURL().c_str())
		);
	}
	else {
		if ( !Settings::global.disableTimeWindowRequest ) {
			if ( Settings::global.initStartTime && !_dataTimeEnd ) {
				_recordStreamThread->setStartTime(Core::Time::UTC());
			}
			else {
				_recordStreamThread->setStartTime(_dataTimeStart);
			}
			_recordStreamThread->setEndTime(_dataTimeEnd);
		}

		for ( auto &item : _channelRequests ) {
			_recordStreamThread->addStream(
				item.networkCode(), item.stationCode(),
				item.locationCode(), item.channelCode()
			);
		}

		connect(_recordStreamThread, SIGNAL(receivedRecord(Seiscomp::Record*)),
		        this, SLOT(receivedRecord(Seiscomp::Record*)));
		_recordStreamThread->start();

		_statusBarFile->setText(tr("Loading records"));
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::recordStreamClosed(RecordStreamThread *thread) {
	if ( _recordStreamThread == thread ) {
		_statusBarFile->setText(tr("Recordstream finished"));
		_recordStreamThread->wait(true);
		delete _recordStreamThread;
		_recordStreamThread = nullptr;
	}

	if ( _wantReload ) {
		reloadData();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::advance() {
	Core::TimeWindow dataTimeWindow;
	CURRENT_TRACEVIEW_RET(dataTimeWindow, visibleTimeRange());
	dataTimeWindow.setEndTime(dataTimeWindow.endTime() + Core::TimeSpan(0, 500000));

	auto remainder = dataTimeWindow.endTime().epochSeconds() % Settings::global.bufferSize;
	if ( remainder ) {
		dataTimeWindow.setEndTime(Core::Time(dataTimeWindow.endTime().epochSeconds() + Settings::global.bufferSize - remainder, 0));
	}
	else {
		dataTimeWindow.setEndTime(Core::Time(dataTimeWindow.endTime().epochSeconds() + Settings::global.bufferSize, 0));
	}
	dataTimeWindow.setStartTime(dataTimeWindow.endTime() - Core::TimeSpan(Settings::global.bufferSize, 0));

	reloadTimeWindow(dataTimeWindow);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::reverse() {
	Core::TimeWindow dataTimeWindow;
	CURRENT_TRACEVIEW_RET(dataTimeWindow, visibleTimeRange());
	dataTimeWindow.setEndTime(dataTimeWindow.endTime() + Core::TimeSpan(0, 500000));

	auto remainder = dataTimeWindow.endTime().epochSeconds() % Settings::global.bufferSize;
	if ( remainder ) {
		dataTimeWindow.setEndTime(Core::Time(dataTimeWindow.endTime().epochSeconds() - remainder, 0));
	}
	else {
		dataTimeWindow.setEndTime(Core::Time(dataTimeWindow.endTime().epochSeconds() - Settings::global.bufferSize, 0));
	}
	dataTimeWindow.setStartTime(dataTimeWindow.endTime() - Core::TimeSpan(Settings::global.bufferSize, 0));

	reloadTimeWindow(dataTimeWindow);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::selectedTraceViewRubberBand(QRect rect,
                                             QList<RecordViewItem*> items,
                                             double tmin, double tmax,
                                             RecordView::SelectionOperation operation) {
	auto view = static_cast<TraceView*>(sender());

	QVector<TraceMarker*> markers;

	if ( rect.width() <= 1 && rect.height() <= 1 ) {
		// Simple click
		if ( items.empty() ) {
			return;
		}

		QPoint p = items.front()->widget()->mapFromGlobal(rect.topLeft());
		auto marker = items.front()->widget()->markerAt(p.x(), p.y(), false);
		if ( marker ) {
			markers.append(static_cast<TraceMarker*>(marker));
		}
	}
	else {
		Core::Time left(tmin), right(tmax);

		for ( auto item : items ) {
			auto markerCount = item->widget()->markerCount();
			for ( int i = 0; i < markerCount; ++i ) {
				auto marker = static_cast<TraceMarker*>(item->widget()->marker(i));
				if ( !marker->isVisible() ) {
					continue;
				}
				if ( marker->time() < left || marker->time() >= right ) {
					continue;
				}

				markers.append(marker);
			}
		}
	}

	if ( _associator && _associator->push(markers, operation) ) {
		view->update();
	}
	if ( _dockAssociator ) {
		_dockAssociator->setVisible(true);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::nextFilter() {
	if ( _statusBarFilter->currentIndex() == 0 ) {
		return;
	}

	auto idx = _statusBarFilter->currentIndex() + 1;
	if ( idx >= _statusBarFilter->count() ) {
		idx = 1;
	}
	_statusBarFilter->setCurrentIndex(idx);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::previousFilter() {
	if ( _statusBarFilter->currentIndex() == 0 ) {
		return;
	}

	auto idx = _statusBarFilter->currentIndex() - 1;
	if ( idx < 1 ) {
		idx = _statusBarFilter->count() - 1;
	}
	_statusBarFilter->setCurrentIndex(idx);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::toggleFilter() {
	if ( _statusBarFilter->currentIndex() > 0 ) {
		_statusBarFilter->setCurrentIndex(0);
	}
	else if ( _statusBarFilter->count() > 1 ) {
		_statusBarFilter->setCurrentIndex(_lastFilterIndex < 0 ? 1 : _lastFilterIndex);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::showScaledValues(bool enable) {
	foreach ( TraceView* view, _traceViews ) {
		for ( int i = 0; i < view->rowCount(); ++i ) {
			Seiscomp::Gui::RecordViewItem* item = view->itemAt(i);
			item->widget()->showScaledValues(enable);
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::receivedRecord(Seiscomp::Record* r) {
	RecordPtr rec = r;
	foreach( TraceView* view, _traceViews ) {
		if ( view->feed(rec) ) {
			if ( !_lastRecordTime || (*_lastRecordTime < rec->endTime()) ) {
				_lastRecordTime = rec->endTime();
			}
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::selectedTime(Seiscomp::Gui::RecordWidget *,
                              Seiscomp::Core::Time) {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::scrollLineUp() {
	CURRENT_TRACEVIEW(selectPreviousRow());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::scrollLineDown() {
	CURRENT_TRACEVIEW(selectNextRow());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::scrollPageUp() {
	CURRENT_TRACEVIEW(scrollPageUp());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::scrollPageDown() {
	CURRENT_TRACEVIEW(scrollPageDown());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::scrollToTop() {
	CURRENT_TRACEVIEW(selectFirstRow());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::scrollToBottom() {
	CURRENT_TRACEVIEW(selectLastRow());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::scrollLeft() {
	CURRENT_TRACEVIEW(scrollLeft());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::scrollRight() {
	CURRENT_TRACEVIEW(scrollRight());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::alignOriginTime() {
	SEISCOMP_DEBUG("OriginTime = %s", _originTime.iso());
	TRACEVIEWS(setAlignment(_originTime));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::sortByStationCode() {
	_switchBack->stop();
	TRACEVIEWS(sortByText(0, 1, 3, 2));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::sortByNetworkStationCode() {
	_switchBack->stop();
	TRACEVIEWS(sortByText(1, 0, 2, 3));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::sortByConfig() {
	_switchBack->stop();
	TRACEVIEWS(sortByValue(DATA_INDEX));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::colorByConfig() {
	if ( !_needColorUpdate ) return;

	foreach ( TraceView* view, _traceViews ) {
		int count = view->rowCount();
		for ( int i = 0; i < count; ++i ) {
			RecordViewItem *item = view->itemAt(i);
			int ig = int(item->value(DATA_GROUP));
			if ( ig >= 0 ) continue;
			ig += int(_channelGroups.size());
			const ChannelGroup &group = _channelGroups[ig];
			if ( group.gradient.isEmpty() ) continue;

			QPen p = group.pen;

			if ( group.numberOfUsedChannels < 2 ) {
				p.setColor(group.gradient.begin().value().first);
			}
			else {
				double rangeMin = group.gradient.begin().key();
				double rangeMax = (--group.gradient.end()).key();
				p.setColor(group.gradient.colorAt(rangeMin + (rangeMax - rangeMin) * item->value(DATA_GROUP_INDEX) / double(group.numberOfUsedChannels - 1)));
			}

			item->widget()->setRecordPen(0, p);
		}
	}

	_needColorUpdate = false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::sortByDistance() {
	_switchBack->stop();
	TRACEVIEWS(sortByValue(DATA_DELTA, DATA_INDEX));
	//TRACEVIEWS(sortByValue(DATA_GROUP, DATA_DELTA, DATA_GROUP_MEMBER, DATA_INDEX));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::sortByGroup() {
	_switchBack->stop();
	TRACEVIEWS(sortByValue(DATA_GROUP, DATA_GROUP_MEMBER, DATA_INDEX));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::alignLeft() {
	TRACEVIEWS(setJustification(0));
	TRACEVIEWS(align());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::alignRight() {
	TRACEVIEWS(setJustification(1));
	TRACEVIEWS(align());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::jumpToLastRecord() {
	if ( _lastRecordTime ) {
		SEISCOMP_DEBUG("LastRecordTime = %s", _lastRecordTime->iso());
		TRACEVIEWS(align());
		TRACEVIEWS(move((*_lastRecordTime - _traceViews.front()->alignment()).length()));
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::clearPickMarkers() {
	_markerMap.clear();
	clearPickCart();
	TRACEVIEWS(clearMarker());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::clearPickCart() {
	// Clear associator cart
	if ( _associator ) {
		_associator->push(QVector<TraceMarker*>(), RecordView::Select);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::step() {
	static int stepper = 0;

	colorByConfig();

	if ( _ui.actionToggleAutoMove->isChecked() && !Settings::global.endTime ) {
		TRACEVIEWS(setAlignment(Core::Time::UTC()));
	}

	// Check every 10 seconds the traces delay
	if ( stepper % 10 == 0 ) {
		checkTraceDelay();
	}

	++stepper;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::switchToNormalState() {
	_statusBarFile->setText("Switching back to normal state");
	sortByConfig();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::sortByOrigin(double lat, double lon) {
	foreach (TraceView* view, _traceViews) {
		for ( int i = 0; i < view->rowCount(); ++i ) {
			Seiscomp::Gui::RecordViewItem* item = view->itemAt(i);
			Seiscomp::Client::StationLocation loc = item->data().value<TraceState>().location;

			double delta = 0, az1, az2;

			Math::Geo::delazi(lat, lon, loc.latitude, loc.longitude,
			                  &delta, &az1, &az2);

			item->setValue(DATA_DELTA, delta);
		}
	}

	sortByDistance();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::toggledFullScreen(bool fs) {
	if ( menuBar() )
		menuBar()->setVisible(!fs);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::messageArrived(Seiscomp::Core::Message* msg, Seiscomp::Client::Packet*) {
	Origin* origin = NULL;

	Seiscomp::DataModel::ArtificialOriginMessage* ao = Seiscomp::DataModel::ArtificialOriginMessage::Cast(msg);
	if ( ao && ao->origin() ) {
		_statusBarFile->setText(QString("An artificial origin arrived at %1 (localtime)")
		                         .arg(Seiscomp::Core::Time::LocalTime().toString("%F %T").c_str()));
		origin = ao->origin();
	}

	if ( !origin ) {
		Seiscomp::Core::DataMessage* dm = Seiscomp::Core::DataMessage::Cast(msg);
		if ( dm ) {
			for ( Seiscomp::Core::DataMessage::iterator it = dm->begin(); it != dm->end(); ++it ) {
				origin = Seiscomp::DataModel::Origin::Cast(it->get());
				if ( origin ) {
					_statusBarFile->setText(QString("An preliminary origin has arrived at %1 (localtime)")
					                        .arg(Seiscomp::Core::Time::LocalTime().toString("%F %T").c_str()));
					break;
				}
			}
		}
	}

	if ( !origin ) {
		Gui::CommandMessage *cmd = Gui::CommandMessage::Cast(msg);
		if ( cmd ) {
			if ( cmd->command() == Gui::CM_OBSERVE_LOCATION )
				origin = Seiscomp::DataModel::Origin::Cast(cmd->object());
		}

		if ( origin ) {
			_statusBarFile->setText(QString("An preliminary origin has arrived at %1 (localtime)")
			                        .arg(Seiscomp::Core::Time::LocalTime().toString("%F %T").c_str()));
		}
	}

	if ( origin && Settings::global.automaticSortEnabled ) {
		_originLat = origin->latitude();
		_originLon = origin->longitude();
		sortByOrigin(_originLat, _originLon);

		try {
			_switchBack->setInterval(Gui::Application::Instance()->configGetInt("autoResetDelay")*1000);
		}
		catch ( ... ) {
			_switchBack->setInterval(1000*60*15);
		}
		_switchBack->start();

		return;
	}


}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::objectAdded(const QString &parentID,
                             Seiscomp::DataModel::Object* object) {
	if ( Settings::global.showPicks ) {
		Pick* pick = Pick::Cast(object);
		if ( pick ) {
			SEISCOMP_INFO("about to add a pick to stream %s",
			              waveformIDToString(pick->waveformID()).toStdString().c_str());
			addPick(pick, 0);
			return;
		}
	}

	auto origin = Origin::Cast(object);
	if ( origin ) {
		_statusBarFile->setText(QString("An origin arrived at %1 (localtime)")
		                         .arg(Seiscomp::Core::Time::LocalTime().toString("%F %T").c_str()));

		// Update association state
		bool skipOrigin = false;
		try {
			if ( origin->evaluationStatus() == REJECTED ) {
				skipOrigin = true;
			}
		}
		catch ( ... ) {}

		if ( !skipOrigin ) {
			auto arrivalCount = origin->arrivalCount();
			for ( size_t i = 0; i < arrivalCount; ++i ) {
				auto it = _markerMap.find(origin->arrival(i)->pickID());
				if ( it != _markerMap.end() ) {
					if ( it.value()->setAssociated(true) ) {
						it.value()->update();
						if ( _associator ) {
							_associator->updatedMarker(it.value());
						}
					}
				}
			}
		}

		if ( Settings::global.automaticSortEnabled ) {
			_originLat = origin->latitude();
			_originLon = origin->longitude();
			sortByOrigin(_originLat, _originLon);

			try {
				_switchBack->setInterval(Gui::Application::Instance()->configGetInt("autoResetDelay") * 1000);
			}
			catch ( ... ) {
				_switchBack->setInterval(1000 * 60 * 15);
			}

			_switchBack->start();
		}

		return;
	}

	auto cs = ConfigStation::Cast(object);
	if ( cs ) {
		if ( SCApp->configModule() && parentID == SCApp->configModule()->publicID().c_str() )
			updateTraces(cs->networkCode(), cs->stationCode(), cs->enabled());
		return;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void MainWindow::objectUpdated(const QString &parentID,
                               Seiscomp::DataModel::Object* object) {
	auto event = Event::Cast(object);
	if ( event ) {
		return;
	}

	auto origin = Origin::Cast(object);
	if ( origin ) {
		// Update associated state
		if ( SCApp->query() ) {
			auto db = SCApp->query()->driver();
			auto it = loadPickRefCounts(origin->publicID());
			for ( ; it.valid(); ++it ) {
				auto idx = db->findColumn("pickID");
				if ( idx < 0 ) {
					continue;
				}
				auto pickID = db->getRowFieldString(idx);

				idx = db->findColumn("refCount");
				if ( idx < 0 ) {
					continue;
				}

				int refCount = -1;
				if ( Core::fromString(refCount, db->getRowFieldString(idx)) ) {
					auto itm = _markerMap.find(pickID);
					if ( itm != _markerMap.end() ) {
						if ( itm.value()->setAssociated(refCount > 0) ) {
							itm.value()->update();
							if ( _associator ) {
								_associator->updatedMarker(itm.value());
							}
						}
					}
				}
			}
		}
	}

	auto cs = ConfigStation::Cast(object);
	if ( cs ) {
		if ( SCApp->configModule() && parentID == SCApp->configModule()->publicID().c_str() )
			updateTraces(cs->networkCode(), cs->stationCode(), cs->enabled());
		return;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool MainWindow::addPick(Pick* pick, int refCount) {
	try {
		if ( SCApp->isAgencyIDBlocked(pick->creationInfo().agencyID()) ) {
			return false;
		}
	}
	catch ( ... ) {}

	// HACK: Z is appended because sent Picks does not have the component code
	// set correctly
	Seiscomp::Gui::RecordViewItem *item = nullptr;

	foreach ( TraceView* view, _traceViews ) {
		WaveformStreamID streamID(pick->waveformID());

		item = view->item(streamID);
		if ( !item ) {
			streamID.setChannelCode(streamID.channelCode() + "Z");
			item = view->item(streamID);
		}

		if ( !item && Settings::global.mapPicksToBestMatchingTrace ) {
			// Map to location code
			int rowCount = view->rowCount();

			for ( int r = 0; r < rowCount; ++r ) {
				auto rvitem = view->itemAt(r);
				if ( rvitem->streamID().networkCode() != pick->waveformID().networkCode() ||
				     rvitem->streamID().stationCode() != pick->waveformID().stationCode() ||
				     rvitem->streamID().locationCode() != pick->waveformID().locationCode() ) {
					continue;
				}

				item = rvitem;

				if ( !rvitem->streamID().channelCode().compare(0, 2, pick->waveformID().channelCode()) ) {
					// Channel code matches, found best match. Otherwise keep
					// on searching and use the location code item as fallback.
					break;
				}
			}
		}

		if ( item ) {
			break;
		}
	}

	// No trace found
	if ( !item ) {
		return false;
	}

	// Remove old markers
	for ( int i = 0; i < item->widget()->markerCount(); ) {
		auto marker = static_cast<TraceMarker*>(item->widget()->marker(i));
		if ( _traceViews.front()->alignment() - marker->time() > _bufferSize ) {
			if ( marker->pick ) {
				auto it = _markerMap.find(marker->pick->publicID());
				if ( it != _markerMap.end() && it.value() == marker ) {
					_markerMap.erase(it);
				}
			}
			delete marker;
		}
		else {
			++i;
		}
	}

	auto age = _traceViews.front()->alignment() - pick->time().value();

	if ( age > _bufferSize ) {
		return false;
	}

	if ( Settings::global.endTime && age < Core::TimeSpan(0, 0) ) {
		return false;
	}

	auto marker = new TraceMarker(nullptr, pick);
	marker->setAssociated(refCount > 0);

	marker->setVisible(
		(marker->isAssociated() && _ui.actionViewArrivals->isChecked()) ||
		(!marker->isAssociated() && _ui.actionViewPicks->isChecked())
	);

	item->widget()->addMarker(marker);
	item->widget()->update();

	_markerMap[pick->publicID()] = marker;

	return true;
}


}
}
}
