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

#ifndef __MAINWINDOW_H__
#define __MAINWINDOW_H__

#include <QtGui>
#ifndef Q_MOC_RUN
#include <seiscomp/datamodel/waveformstreamid.h>
#endif
#include <seiscomp/gui/core/mainwindow.h>


#include "ui_mainwindow.h"


class HeliWidget;

namespace Seiscomp {

class Record;

namespace Gui {

class RecordStreamThread;

}
}


class MainWindow : public Seiscomp::Gui::MainWindow {
	Q_OBJECT

	public:
		MainWindow();
		~MainWindow();


	public:
		void setGain(const float gain);
		void setHeadline(const QString headline);

		void setPostProcessingScript(const std::string &path);

		void setReferenceTime(const Seiscomp::Core::Time &time);
		void setStream(const std::string &networkCode,
		const std::string &stationCode,
		const std::string &locationCode,
		const std::string &channelCode);

		void setStream(Seiscomp::DataModel::WaveformStreamID streamID);

		void setFilter(const std::string &filter);
		bool setScaling(const std::string &scaling);
		void setAmplitudeRange(double min, double max);
		void setRowColors(const QVector<QColor> &);

		void setStationDescriptionEnabled(bool);
		void setAntialiasingEnabled(bool);
		void setLineWidth(int);
		void setLayout(int rows, int secondsPerRow);
		void setOutputResolution(int xres, int yres, int dpi);
		void setSnapshotTimeout(int timeout);

		void setTimeFormat(const std::string &timeFormat);

		void fixCurrentTimeToLastRecord(bool);

		void start(QString dumpFilename);
		void updateTimeLabel(const Seiscomp::Core::Time &time);

		void timerEvent(QTimerEvent *event);


	public slots:
		void print(QString filename);


	protected:
		void toggledFullScreen(bool val);


	private:
		void startAcquisition();


	private slots:
		void acquisitionFinished();
		void receivedRecord(Seiscomp::Record*);
		void advanceTime();


	private:
		Ui::MainWindow                        _ui;
		HeliWidget                           *_heliWidget;
		Seiscomp::Gui::RecordStreamThread    *_streamThread;
		Seiscomp::DataModel::WaveformStreamID _streamID;
		Seiscomp::Core::Time                  _realTimeStart;
		Seiscomp::Core::Time                  _globalEndTime;
		Seiscomp::Core::Time                  _referenceTime;
		Seiscomp::Core::TimeSpan              _fullTimeSpan;
		std::string                           _timeFormat;
		QTimer                                _timer;
		QString                               _dumpFilename;
		std::string                           _imagePostProcessingScript;
		bool                                  _fixCurrentTimeToLastRecord;
		bool                                  _showStationDescription;
		int                                   _rowTimeSpan;
		int                                   _xRes;
		int                                   _yRes;
		int                                   _dpi;
		int                                   _snapshotTimer;
};


#endif
