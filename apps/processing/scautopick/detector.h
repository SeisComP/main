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


#ifndef APPS_PICKER_DETECTOR_H
#define APPS_PICKER_DETECTOR_H


#include <seiscomp/processing/detector.h>
#include <seiscomp/processing/amplitudeprocessor.h>


namespace Seiscomp {

namespace Applications {

namespace Picker {


DEFINE_SMARTPOINTER(Detector);

class Detector : public Processing::SimpleDetector {
	public:
		Detector(double initTime = 0.0);
		Detector(double on, double off, double initTime = 0.0);

		void setAmplitudePublishFunction(const Processing::AmplitudeProcessor::PublishFunc& func);
		void setPickID(const std::string&) const;

		void setDeadTime(const Core::TimeSpan &dt);
		void setAmplitudeTimeWindow(const Core::TimeSpan &dt);
		void setMinAmplitudeOffset(double);
		void setDurations(double minDur, double maxDur);
		void setSensitivityCorrection(bool enable);

		void reset();


	protected:
		void fill(size_t n, double *samples);

		bool handleGap(Filter *filter, const Core::TimeSpan&,
		               double lastSample, double nextSample,
		               size_t missingSamples);

		bool emitPick(const Record* rec, const Core::Time& t);

		void process(const Record *record, const DoubleArray &filteredData);

		bool validateOn(const Record *record, size_t &i, const DoubleArray &filteredData);
		bool validateOff(const Record *record, size_t i, const DoubleArray &filteredData);

		void calculateMaxAmplitude(const Record *record, size_t i0, size_t i1, const DoubleArray &filteredData);

		bool sendPick(const Record* rec, const Core::Time& t);
		void sendMaxAmplitude(const Record *record);


	private:
		void init();


	private:
		struct AmplitudeMaxProcessor {
			AmplitudeMaxProcessor() { reset(); }

			size_t      pickIndex;
			OPT(double) amplitude;
			size_t      index;
			size_t      neededSamples;
			size_t      processedSamples;

			void reset();

			bool isRunning() const;
			bool isFinished() const;
		};

		Processing::AmplitudeProcessor::PublishFunc _amplPublish;

		Core::Time  _currentPick;
		RecordCPtr  _currentPickRecord;

		Core::Time  _lastPick;
		OPT(double) _lastAmplitude;

		double      _triggeredAmplitude;
		OPT(double) _minAmplitude;

		Core::TimeSpan _deadTime;
		Core::TimeSpan _twMax;
		double         _amplitudeThreshold;

		double         _minDuration;
		double         _maxDuration;

		bool           _sensitivityCorrection;

		AmplitudeMaxProcessor _amplProc;

		mutable std::string _pickID;
};


}

}

}


#endif
