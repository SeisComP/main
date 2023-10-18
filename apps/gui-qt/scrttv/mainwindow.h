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


#ifndef SEISCOMP_GUI_SCRTTV_MAINWINDOW
#define SEISCOMP_GUI_SCRTTV_MAINWINDOW

#ifndef Q_MOC_RUN
#include <seiscomp/core/record.h>
#include <seiscomp/client/inventory.h>
#include <seiscomp/datamodel/waveformstreamid.h>
#include <seiscomp/datamodel/configstation.h>
#endif
#include <seiscomp/gui/core/gradient.h>
#include <seiscomp/gui/core/mainwindow.h>
#include <seiscomp/gui/core/questionbox.h>
#include <seiscomp/gui/core/recordview.h>
#include <seiscomp/gui/core/recordstreamthread.h>
#include <seiscomp/gui/core/spectrogramrenderer.h>
#include <seiscomp/gui/plot/axis.h>

#include "progressbar.h"
#include "ui_mainwindow.h"

#include <QtGui>
#include <QComboBox>
#include <QTabBar>

#include <map>
#include <set>


class QLineEdit;

namespace Seiscomp {

namespace DataModel {

class Pick;

}

// 30 minutes of records per stream
#define RECORD_TIMESPAN   30*60


namespace Applications {
namespace TraceView {


class Associator;
class SpectrogramSettings;
class TraceMarker;


using RecordStreamThread = Gui::RecordStreamThread;


struct TraceState {
	Seiscomp::Client::StationLocation location;
};


class TraceWidget : public Seiscomp::Gui::RecordWidget {
	public:
		TraceWidget(const DataModel::WaveformStreamID &sid);
		~TraceWidget() override;

	public:
		void setShowSpectrogram(bool enable);
		void setShowSpectrogramAxis(bool enable);
		Gui::SpectrogramRenderer *spectrogram();
		void resetSpectrogram();

	public:
		void fed(int slot, const Record *rec) override;
		void paintEvent(QPaintEvent *event) override;

	private:
		void drawSpectrogram(QPainter &painter);
		void drawSpectrogramAxis(QPainter &painter);

	private:
		bool                      _showSpectrogram{false};
		Gui::SpectrogramRenderer *_spectrogram{nullptr};
		Gui::Axis                *_spectrogramAxis{nullptr};
};


class TraceView : public Seiscomp::Gui::RecordView {
	Q_OBJECT

	public:
		TraceView(const Seiscomp::Core::TimeSpan &span,
		          QWidget *parent = 0, Qt::WindowFlags f = Qt::WindowFlags());

		~TraceView();

	public:
		void setTimeSpan(const Seiscomp::Core::TimeSpan &span) {
			_timeSpan = span;
		}

	public slots:
		void setDefaultDisplay() {
			setUpdatesEnabled(false);
			Seiscomp::Gui::RecordView::setDefaultDisplay();
			setJustification(1.0);
			setTimeRange(-_timeSpan,0);
			setUpdatesEnabled(true);
		}

		void toggleFilter(bool);
		void toggleSpectrogram(bool);

	protected:
		Gui::RecordWidget *createRecordWidget(
			const DataModel::WaveformStreamID &streamID
		) const override;

	private:
		double _timeSpan;
};


class TraceViewTabBar : public QTabBar {
	Q_OBJECT

	public:
		TraceViewTabBar(QWidget *parent = 0);

	public:
		int findIndex(const QPoint& p);

	protected:
		void mousePressEvent(QMouseEvent *e);

	private slots:
		void textChanged();
};


class TraceTabWidget : public QTabWidget {
	Q_OBJECT

	//! Construction
	public:
		TraceTabWidget(QWidget* parent = NULL);

	signals:
		void tabRemovalRequested(int index);
		void moveSelectionRequested(Seiscomp::Gui::RecordView *target,
		                            Seiscomp::Gui::RecordView *source);

	//! Private slots
	private slots:
		void showContextMenu(const QPoint&);
		void closeTab();

	protected:
		void checkActiveTab(const QPoint& p);
		bool checkDraging(QDropEvent *event);

	//! Event handler
	protected:
		void dragEnterEvent(QDragEnterEvent *event);
		void dragMoveEvent(QDragMoveEvent *event);
		void dropEvent(QDropEvent *event);
		void showEvent(QShowEvent *);

	private:
		TraceViewTabBar* _tabBar;
		QList<QAction*> _tabActions;
		int _tabToClose;
		bool _nonInteractive;
};



class MainWindow : public Seiscomp::Gui::MainWindow {
	Q_OBJECT

	public:
		MainWindow();
		~MainWindow();


	public:
		void start();
		void openFile(const std::string &filename);


	private slots:
		void openAcquisition();
		void openXML();

		void selectStreams();
		void addTabulator();

		void nextFilter();
		void previousFilter();
		void toggleFilter();
		void showScaledValues(bool enable);
		void changeTraceState();

		void receivedRecord(Seiscomp::Record* r);
		void selectedTime(Seiscomp::Gui::RecordWidget*, Seiscomp::Core::Time);

		void scrollLineUp();
		void scrollLineDown();
		void scrollPageUp();
		void scrollPageDown();
		void scrollToTop();
		void scrollToBottom();
		void scrollLeft();
		void scrollRight();

		void alignOriginTime();
		void sortByStationCode();
		void sortByNetworkStationCode();
		void sortByDistance();
		void sortByConfig();
		void sortByGroup();
		void colorByConfig();

		void alignLeft();
		void alignRight();

		void jumpToLastRecord();
		void clearPickMarkers();
		void clearPickCart();

		void step();
		void switchToNormalState();

		void scaleVisibleAmplitudes(bool);
		void listHiddenStreams();

		void updateMarkerVisibility();

		void removeTab(int);

		void setupItem(const Seiscomp::Record*, Seiscomp::Gui::RecordViewItem*);
		void itemCustomContextMenuRequested(const QPoint &);
		void createOrigin(Seiscomp::Gui::RecordViewItem*, Seiscomp::Core::Time);

		void messageArrived(Seiscomp::Core::Message*, Seiscomp::Client::Packet*);
		void objectAdded(const QString& parentID, Seiscomp::DataModel::Object*);
		void objectUpdated(const QString& parentID, Seiscomp::DataModel::Object*);

		bool addPick(Seiscomp::DataModel::Pick* pick, int refCount);

		void moveSelection(Seiscomp::Gui::RecordView* target,
		                   Seiscomp::Gui::RecordView* source);

		void filterChanged(const QString&);

		void enableSearch();
		void search(const QString &text);
		void nextSearch();
		void abortSearch();

		void clearSelection();

		void applySpectrogramSettings();

		void checkTraceDelay();
		void updateTraceCount();

		void selectModeNone();
		void selectModeZoom(bool allowToggle = true);
		void selectModePicks(bool allowToggle = true);
		void selectMode(int mode);

		void filterSelectionChanged();

		void selectedTraceViewRubberBand(QRect rect,
		                                 QList<Seiscomp::Gui::RecordViewItem*>,
		                                 double tmin, double tmax,
		                                 Seiscomp::Gui::RecordView::SelectionOperation operation);

		void reload();
		void switchToRealtime();
		void recordStreamClosed(RecordStreamThread*);

		void advance();
		void reverse();


	protected:
		void toggledFullScreen(bool);
		void applyFilter();


	private:
		void loadFiles();

		DataModel::ConfigStation* configStation(const std::string& networkCode,
		                                        const std::string& stationCode) const;

		bool isStationEnabled(const std::string& networkCode,
		                      const std::string& stationCode) const;

		void setStationEnabled(const std::string& networkCode,
		                       const std::string& stationCode,
		                       bool enable);

		void updateTraces(const std::string& networkCode,
		                  const std::string& stationCode,
		                  bool enable);

		TraceView* createTraceView();

		void sortByOrigin(double lat, double lon);

		void searchByText(const QString &text);

		void reloadTimeWindow(const Core::TimeWindow &tw);
		void reloadData();

		void applySpectrogramSettings(TraceWidget *widget);


	private:
		Ui::MainWindow                            _ui;
		QDockWidget                              *_dockAssociator{nullptr};
		QVector<TraceView*>                       _traceViews;
		Associator                               *_associator{nullptr};
		SpectrogramSettings                      *_spectrogramSettings{nullptr};
		QMap<std::string, TraceMarker*>           _markerMap;

		Gui::RecordStreamThread                  *_recordStreamThread;
		QList<DataModel::WaveformStreamID>        _channelRequests;

		QComboBox                                *_statusBarSelectMode;
		QLabel                                   *_statusBarFile;
		QComboBox                                *_statusBarFilter;
		QLineEdit                                *_statusBarSearch;
		Gui::ProgressBar                         *_statusBarProg;
		Core::TimeSpan                            _bufferSize;
		Core::Time                                _originTime;
		Core::Time                                _lastRecordTime;
		Core::TimeWindow                          _dataTimeWindow;

		QMap<DataModel::WaveformStreamID, double> _scaleMap;
		QColor                                    _searchBase, _searchError;

		TraceTabWidget                           *_tabWidget;

		QTimer                                   *_timer;
		QTimer                                   *_switchBack;

		bool                                      _needColorUpdate;
		int                                       _lastFoundRow;
		int                                       _rowSpacing;
		bool                                      _withFrames;
		int                                       _frameMargin;
		int                                       _rowHeight;
		int                                       _numberOfRows;
		bool                                      _wantReload{false};
		int                                       _lastFilterIndex{-1};
		std::vector<std::string>                  _dataFiles;

		Seiscomp::Gui::QuestionBox                _questionApplyChanges;

		struct WaveformStreamEntry {
			WaveformStreamEntry(const Seiscomp::DataModel::WaveformStreamID& id, int idx, double s = 1.0)
			: streamID(id), index(idx), scale(s) {}

			bool operator==(const WaveformStreamEntry& other) const {
				return streamID == other.streamID;
			}

			Seiscomp::DataModel::WaveformStreamID streamID;
			int                                   index;
			double                                scale;
		};

		struct ltWaveformStreamID {
			bool operator()(const WaveformStreamEntry& left, const WaveformStreamEntry& right) const {
				return left.streamID < right.streamID;
			}
		};

		struct DecorationDesc {
			std::string matchID;
			OPT(double) minValue;
			OPT(double) maxValue;
			bool        fixedScale{false};
			OPT(double) gain;
			OPT(double) minMaxMargin;
			QPen        minPen;
			QBrush      minBrush;
			QPen        maxPen;
			QBrush      maxBrush;
			QString     description;
			QString     unit;
		};

		struct ChannelGroup {
			std::vector<DataModel::WaveformStreamID> members;
			std::string   title;
			std::string   regionName;
			QPen          pen;
			int           numberOfUsedChannels{0};
			Gui::Gradient gradient;
		};

		using WaveformStreamSet = std::set<WaveformStreamEntry, ltWaveformStreamID>;
		WaveformStreamSet _waveformStreams;

		using DecorationDescs = std::vector<DecorationDesc>;
		DecorationDescs _decorationDescs;

		using ChannelGroups = std::vector<ChannelGroup>;
		ChannelGroups _channelGroups;

		using ChannelGroupLookup = std::map<std::string, size_t>;
		ChannelGroupLookup _channelGroupLookup;

	friend class TraceDecorator;
};

}
}
}


#endif
