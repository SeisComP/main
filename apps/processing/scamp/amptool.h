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




#ifndef SEISCOMP_APPLICATIONS_AMPTOOL_H__
#define SEISCOMP_APPLICATIONS_AMPTOOL_H__

#include <seiscomp/client/streamapplication.h>
#include <seiscomp/processing/amplitudeprocessor.h>
#include <seiscomp/datamodel/publicobjectcache.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/amplitude.h>
#include <seiscomp/utils/timer.h>

#define SEISCOMP_COMPONENT AmpTool
#include <seiscomp/logging/log.h>

#include <boost/thread/mutex.hpp>
#include <map>
#include <sstream>


namespace Seiscomp {

class Record;

namespace DataModel {

class Pick;
class Amplitude;
class Origin;

}
}


class AmpTool : public Seiscomp::Client::StreamApplication {
	public:
		AmpTool(int argc, char **argv);
		~AmpTool();


	protected:
		void createCommandLineDescription();
		bool validateParameters();
		bool initConfiguration();

		bool init();
		bool run();
		void done();

		bool storeRecord(Seiscomp::Record *rec);
		void handleRecord(Seiscomp::Record *rec);
		void handleTimeout();

		void acquisitionFinished();

		void addObject(const std::string&, Seiscomp::DataModel::Object* object);
		void updateObject(const std::string&, Seiscomp::DataModel::Object* object);
		void removeObject(const std::string&, Seiscomp::DataModel::Object* object);


	private:
		typedef std::multimap<std::string, Seiscomp::DataModel::AmplitudePtr> AmplitudeMap;
		typedef std::pair<AmplitudeMap::iterator, AmplitudeMap::iterator>     AmplitudeRange;
		typedef std::map<std::string, Seiscomp::DataModel::AmplitudePtr>      SingleAmplitudeMap;

		void process(Seiscomp::DataModel::Origin *origin = nullptr, Seiscomp::DataModel::Pick *pick = nullptr);

		void feed(Seiscomp::DataModel::Pick *pick);
		void feed(Seiscomp::DataModel::Amplitude *amp);

		int addProcessor(Seiscomp::Processing::AmplitudeProcessor *,
		                 const Seiscomp::DataModel::Origin *origin,
		                 const Seiscomp::DataModel::Pick *pick,
		                 OPT(double) distance, OPT(double) depth, OPT(double) originTime);

		size_t loadAmplitudes(const std::string &pickID,
		                      Seiscomp::DataModel::Amplitude *ignoreAmp = NULL);

		AmplitudeRange getAmplitudes(const std::string &pickID);
		Seiscomp::DataModel::Amplitude *hasAmplitude(const AmplitudeRange &range, const std::string &type) const;

		void createDummyAmplitude(const Seiscomp::Processing::AmplitudeProcessor *proc);
		void emitAmplitude(const Seiscomp::Processing::AmplitudeProcessor *,
		                   const Seiscomp::Processing::AmplitudeProcessor::Result &);

		void storeLocalAmplitude(const Seiscomp::Processing::AmplitudeProcessor *,
		                         const Seiscomp::Processing::AmplitudeProcessor::Result &);

		void removedFromCache(Seiscomp::DataModel::PublicObject *);

		void printReport();

		Seiscomp::DataModel::AmplitudePtr createAmplitude(const Seiscomp::Processing::AmplitudeProcessor *,
		                                                  const Seiscomp::Processing::AmplitudeProcessor::Result &);


	private:
		struct CompareWaveformStreamID {
			bool operator()(const Seiscomp::DataModel::WaveformStreamID &lhs,
			                const Seiscomp::DataModel::WaveformStreamID &rhs) const;
		};

		typedef std::set<Seiscomp::DataModel::WaveformStreamID,CompareWaveformStreamID> WaveformIDSet;
		struct StationRequest {
			Seiscomp::Core::TimeWindow timeWindow;
			WaveformIDSet streams;
		};

		typedef std::map<const Seiscomp::Processing::AmplitudeProcessor*, Seiscomp::DataModel::AmplitudePtr> ProcAmpReuseMap;

		typedef Seiscomp::Processing::AmplitudeProcessorFactory::ServiceNames AmplitudeTypeList;
		typedef std::set<std::string> AmplitudeList;
		typedef std::vector<Seiscomp::Processing::AmplitudeProcessorPtr>      ProcessorSlot;
		typedef std::map<std::string, ProcessorSlot>                          ProcessorMap;
		typedef std::map<std::string, Seiscomp::Util::KeyValuesPtr>           ParameterMap;
		typedef std::map<std::string, StationRequest>                         RequestMap;

		typedef std::map<std::string, Seiscomp::Processing::StreamPtr>        StreamMap;
		typedef Seiscomp::DataModel::PublicObjectTimeSpanBuffer               Cache;
		typedef Seiscomp::DataModel::EventParametersPtr                       EventParametersPtr;

		StreamMap                  _streams;
		double                     _fExpiry{1.0};
		bool                       _fetchMissingAmplitudes{true};
		double                     _minWeight{0.5};
		std::string                _originID;
		std::string                _strTimeWindowStartTime;
		std::string                _strTimeWindowEndTime;

		AmplitudeTypeList          _registeredAmplitudeTypes;
		AmplitudeList              _amplitudeTypes;
		ProcessorMap               _processors;
		RequestMap                 _stationRequests;
		ParameterMap               _parameters;

		AmplitudeMap               _pickAmplitudes;
		ProcAmpReuseMap            _ampIDReuse;

		Cache                      _cache;

		bool                       _testMode;
		bool                       _firstRecord;
		bool                       _dumpRecords;
		bool                       _reprocessAmplitudes;
		bool                       _forceReprocessing{false};
		bool                       _picks{false};
		bool                       _replaceWaveformIDWithBindings{false};
		std::string                _epFile;
		EventParametersPtr         _ep;
		bool                       _formatted{false};

		double                     _initialAcquisitionTimeout{30.0};
		double                     _runningAcquisitionTimeout{2.0};
		double                     _acquisitionTimeout;
		bool                       _hasRecordsReceived;

		Seiscomp::Util::Timer      _timer;
		boost::mutex               _acquisitionMutex;

		Seiscomp::Util::StopWatch  _acquisitionTimer;
		Seiscomp::Util::StopWatch  _noDataTimer;

		Seiscomp::Logging::Channel *_errorChannel;
		Seiscomp::Logging::Output  *_errorOutput;

		Seiscomp::Logging::Channel *_processingInfoChannel;
		Seiscomp::Logging::Output  *_processingInfoOutput;

		std::stringstream           _report;
		std::stringstream           _result;

		SingleAmplitudeMap          _reprocessMap;

		ObjectLog                  *_inputPicks;
		ObjectLog                  *_inputAmps;
		ObjectLog                  *_inputOrgs;
		ObjectLog                  *_outputAmps;
};


#endif
