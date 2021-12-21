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

#include <seiscomp/gui/core/application.h>
#ifndef Q_MOC_RUN
#include <seiscomp/core/strings.h>
#include <seiscomp/datamodel/station.h>
#include <seiscomp/datamodel/waveformstreamid.h>
#include <seiscomp/io/recordstream.h>
#include <seiscomp/io/recordinput.h>
#endif
#include <seiscomp/gui/core/application.h>
#include <seiscomp/gui/core/utils.h>
#include <seiscomp/gui/core/recordstreamthread.h>


#include <QObject>

#include "mainwindow.h"
#include "heliwidget.h"


using namespace Seiscomp;


class HCApp : public Gui::Application {
	Q_OBJECT

	public:
		HCApp(int& argc, char** argv, int flags = DEFAULT, Type type = GuiClient);
		~HCApp();

	protected:
		QString findHeadline(const DataModel::WaveformStreamID &streamID,
		                     const Core::Time &refTime);
		double findGain(const DataModel::WaveformStreamID &streamID,
		               const Core::Time &refTime);

		bool initConfiguration();
		void createCommandLineDescription();
		bool validateParameters();
		bool handleInitializationError(Stage stage);
		bool init();
		bool run();

		void setupUi(MainWindow *w);

	protected:
		void timerEvent(QTimerEvent *);
		void saveSnapshots();

	private slots:
		void receivedRecord(Seiscomp::Record*);
		void acquisitionFinished();


	private:
		struct HeliStream {
			HeliStream() {}
			HeliStream(HeliCanvas *hc) : canvas(hc) {}
			HeliCanvas *canvas;
			QString     headline;
			Core::Time  lastSample;
		};

		typedef QMap<std::string, HeliStream> HeliStreamMap;

		std::vector<std::string> _streamCodes;
		std::vector<std::string> _streamIDs;
		HeliStreamMap            _helis;
		Core::Time               _endTime;
		std::string              _filterString;
		std::string              _imagePostProcessingScript;
		double                   _gain;
		std::string              _scaling;
		double                   _amplitudesRange;
		double                   _amplitudesMin;
		double                   _amplitudesMax;
		QVector<QColor>          _rowColors;
		bool                     _fixCurrentTimeToLastRecord;
		int                      _numberOfRows;
		int                      _timeSpanPerRow;
		int                      _xRes;
		int                      _yRes;
		int                      _dpi;
		int                      _snapshotTimeout;
		int                      _snapshotTimer;
		bool                     _antialiasing;
		int                      _lineWidth;
		bool                     _stationDescription;
		std::string              _timeFormat;
		std::string              _outputFilename;
		Gui::RecordStreamThread *_streamThread;
};
