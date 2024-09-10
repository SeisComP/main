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


#ifndef SEISCOMP_APPLICATIONS_PICKER
#define SEISCOMP_APPLICATIONS_PICKER


#include <seiscomp/processing/application.h>
#include <seiscomp/processing/detector.h>
#include <seiscomp/processing/picker.h>
#include <seiscomp/processing/secondarypicker.h>
#include <seiscomp/processing/fx.h>
#include <seiscomp/processing/amplitudeprocessor.h>

#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/stationmagnitude.h>

#include <list>

#include "config.h"
#include "stationconfig.h"


namespace Seiscomp {
namespace Applications {
namespace Picker {


class App : public Processing::Application {
	public:
		App(int argc, char **argv);
		~App();


	protected:
		void createCommandLineDescription() override;
		bool validateParameters() override;
		bool initConfiguration() override;

		bool init() override;
		bool run() override;
		void done() override;

		void addObject(const std::string& parentID, DataModel::Object* o) override;
		void removeObject(const std::string& parentID, DataModel::Object* o) override;
		void updateObject(const std::string& parentID, DataModel::Object* o) override;

		void handleNewStream(const Record *rec) override;


	private:
		// Initializes a single component of a processor.
		bool initComponent(Processing::WaveformProcessor *proc,
		                   Processing::WaveformProcessor::Component comp,
		                   const Core::Time &time,
		                   const std::string &streamID,
		                   const DataModel::WaveformStreamID &waveformID,
		                   bool metaDataRequired);

		// Initializes a processor which can use multiple components. This
		// method calls initComponent for each requested component.
		bool initProcessor(Processing::WaveformProcessor *proc,
		                   Processing::WaveformProcessor::StreamComponent comp,
		                   const Core::Time &time,
		                   const std::string &streamID,
		                   const DataModel::WaveformStreamID &waveformID,
		                   bool metaDataRequired);

		bool initDetector(const std::string &streamID,
		                   const DataModel::WaveformStreamID &waveformID,
		                   const Record *rec);

		bool addFeatureExtractor(Seiscomp::DataModel::Pick *pick,
		                         DataModel::Amplitude *amp,
		                         const Record *rec, bool isPrimary);
		void addSecondaryPicker(const Core::Time &onset, const Record *rec,
		                        const std::string& pickID);
		void addAmplitudeProcessor(Processing::AmplitudeProcessorPtr proc,
		                           const Record *rec,
		                           const Seiscomp::DataModel::Pick *pick);

		template <typename T>
		void pushProcessor(const std::string &networkCode,
		                   const std::string &stationCode,
		                   const std::string &locationCode,
		                   T *proc);

		void processorFinished(const Record *rec, Processing::WaveformProcessor *wp);

		void emitTrigger(const Processing::Detector *pickProc,
		                 const Record *rec, const Core::Time& time);

		void emitDetection(const Processing::Detector *pickProc,
		                   const Record *rec, const Core::Time& time);

		void emitPPick(const Processing::Picker *,
		               const Processing::Picker::Result &,
		               double duration);

		void emitSPick(const Processing::SecondaryPicker *,
		               const Processing::SecondaryPicker::Result &);

		void emitFXPick(Seiscomp::DataModel::PickPtr pick,
		                Seiscomp::DataModel::AmplitudePtr amp,
		                bool isPrimary,
		                const Processing::FX*,
		                const Processing::FX::Result &);

		void emitAmplitude(const Processing::AmplitudeProcessor *ampProc,
		                   const Processing::AmplitudeProcessor::Result &res);

		void sendPick(Seiscomp::DataModel::Pick *pick,
		              Seiscomp::DataModel::Amplitude *amp,
		              const Record *rec, bool isPrimary);


	private:
		typedef std::map<std::string, Processing::StreamPtr> StreamMap;
		typedef std::map<std::string, DataModel::PickPtr> PickMap;

		typedef Processing::WaveformProcessor TWProc;

		struct ProcEntry {
			ProcEntry(const Core::Time &t, TWProc *p)
			: dataEndTime(t), proc(p) {}

			Core::Time  dataEndTime;
			TWProc     *proc;
		};

		typedef std::list<ProcEntry>  ProcList;
		typedef std::map<std::string, ProcList> ProcMap;
		typedef std::map<TWProc*, std::string> ProcReverseMap;
		typedef DataModel::EventParametersPtr EP;

		StreamMap      _streams;
		Config         _config;
		PickMap        _lastPicks;

		ProcMap        _runningStreamProcs;
		ProcReverseMap _procLookup;

		StringSet      _streamIDs;

		StationConfig  _stationConfig;
		EP             _ep;

		ObjectLog     *_logPicks;
		ObjectLog     *_logAmps;
};


}
}
}


#endif
