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


#ifndef __HANDLER_H___
#define __HANDLER_H___

#include <memory>

#include <QColor>

#include <seiscomp/core/record.h>
#include <seiscomp/config/config.h>
#include <seiscomp/datamodel/waveformquality.h>

#include "stationdata.h"


class StationDataHandler {
	public:
		virtual ~StationDataHandler() {}

	public:
		virtual void update(StationData* stationData) = 0;
};


class RecordHandler : public StationDataHandler {
	public:
		RecordHandler();

	public:
		void handle(StationData* stationData, Seiscomp::Record* record);
		virtual void update(StationData* stationData);

		void setRecordLifeSpan(double span);

	private:
		QColor interpolate(double x, int x0, int x1,
		                   const QColor& c0, const QColor& c1) const;
		QColor calculateColorFromVelocity(double velocity);

	private:
		int _velocityLimits[10];

		int                      _recordLifespan;
		Seiscomp::Core::TimeSpan _sampleLifespan;
};


class TriggerHandler : public StationDataHandler {
	public:
		static const int STATION_MINIMUM_FRAME_SIZE              = 1;
		static const int STATION_MAXIMUM_FRAME_SIZE              = 10;
		static const int STATION_DEFAULT_PICK_TRIGGER_FRAME_SIZE = 3;
		static const int STATION_DEFAULT_FRAME_SIZE              = 0;

	public:
		TriggerHandler();

	public:
		void handle(StationData* stationData, Seiscomp::DataModel::Amplitude* amplitude);
		void handle(StationData* stationData, Seiscomp::DataModel::Pick* pick);
		virtual void update(StationData* stationData);

		void setPickLifeSpan(double timeSpan);

	private:
		void cleanupPickCollection(StationData* stationData);
		int calculateFrameSizeFromAmplitude(double amplitude) const;
		Seiscomp::Core::TimeSpan calculateTriggerLifeSpanFromAmplitude(double amplitde);

		bool isAmplitudeExpired(StationData* stationData) const;
		bool isPickExpired(StationData* stationData) const;
		bool hasValidAmplitude(StationData* stationData) const;
		bool hasValidPick(StationData* stationData) const;

		void reset(StationData* stationData) const;

	private:
		Seiscomp::Core::TimeSpan _defaultPickTriggerLifeSpan;
		Seiscomp::Core::TimeSpan _minimumAmplitudeTriggerLifeSpan;
		Seiscomp::Core::TimeSpan _maximumAmplitudeTriggerLifeSpan;

		Seiscomp::Core::TimeSpan _pickLifeSpan;
};


class QCHandler : public StationDataHandler {
	public:
		enum DelayThreshold { QC_DELAY0 = 20, QC_DELAY1 = 60, QC_DELAY2 = 180,
			                  QC_DELAY3 = 600, QC_DELAY4 = 1800,
			                  QC_DELAY5 = 43200, QC_DELAY6 = 86400 };

	public:
		void init(const Seiscomp::Config::Config &config);

		void handle(StationData* stationData, Seiscomp::DataModel::WaveformQuality* waveformQuality);
		virtual void update(StationData* stationData);

	private:
		QCParameter::Parameter resolveQualityControlParameter(const std::string& parameter);
		void calculateQualityControlStatus(QCParameter::Parameter parameter, StationData* stationData);
		void setQualityControlValues(QCParameter::Parameter parameter, StationData* stationData,
		                             Seiscomp::DataModel::WaveformQuality* waveformQuality);
		void setGlobalQualityControlStatus(StationData* stationData);
		QColor calculateDelayQualityControlColor(double delayValue);

	private:
		struct Thresholds {
			bool     initialized;
			float    value[2];
		};

		Thresholds _thresholds[QCParameter::Parameter::Quantity];
};


#endif
