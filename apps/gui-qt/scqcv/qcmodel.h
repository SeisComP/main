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




#ifndef __QCMODEL_H__
#define __QCMODEL_H__

#include <QtGui>
#include <utility>

#ifndef Q_MOC_RUN
#include <seiscomp/datamodel/waveformquality.h>
#endif

namespace Seiscomp {
namespace Applications {
namespace Qc {

class QcViewConfig;


typedef QVector<DataModel::WaveformQualityPtr> WfQList; // <indexRow, WfQ>

struct StreamEntry {
	QString streamID;
	bool    enabled;
	WfQList report;
	WfQList alert;
};

typedef QMap<QString, StreamEntry> StreamMap; // <StreamID, StreamEntry>


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
class QcModel : public QAbstractTableModel {
	Q_OBJECT

	public:
		QcModel(const QcViewConfig* config, QObject* parent=0);
		const QcViewConfig* config() const;

		void setStreams(const std::list<std::pair<std::string, bool> >& streams);
		void setCleanUpTime(double time);
		void addColumn(const QString& qcParameterName);
		void addStream(const QString& streamID, bool enabled=true);
		void removeStream(const QString& streamID);
		void setWaveformQuality(DataModel::WaveformQuality* wfq);

		void setStationEnabled(const QString& net, const QString& sta, bool enabled);
		void setStreamEnabled(const QString& streamID, bool enabled);
		void setStreamEnabled(const QModelIndex &index, bool enabled);
		bool streamEnabled(const QModelIndex &index) const;
	
		int rowCount(const QModelIndex &parent = QModelIndex()) const;
		int columnCount(const QModelIndex &parent = QModelIndex()) const;

		QVariant data(const QModelIndex &index, int role) const;

		QVariant headerData(int section, Qt::Orientation orientation,
		                    int role = Qt::DisplayRole) const;

		const QString& getHeader(int section) const ;
		bool hasAlerts(const QString &streamID);

		QString getKey(const QModelIndex &index) const;

	signals:
		void stationStateChanged(QString, bool);
		
	private slots:
		void timeout();
		void cleanUp();

	private:
		const DataModel::WaveformQuality *getAlertData(const QModelIndex &index) const;
		const DataModel::WaveformQuality *getData(const QModelIndex &index) const;
		QColor getColor(const QModelIndex &index) const;
		void invalidateData(const QString &key);

	private:
		const QcViewConfig* _config;
		QTimer _timer;
		QStringList _columns;
		StreamMap _streamMap;
		bool _dataChanged;
		double _cleanUpTime;
		QString wfq2str(const DataModel::WaveformQuality* wfq) const;
};
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

}
}
}
#endif
