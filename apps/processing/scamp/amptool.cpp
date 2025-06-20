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


#include "amptool.h"
#include "util.h"

#include <seiscomp/logging/filerotator.h>
#include <seiscomp/logging/channel.h>

#include <seiscomp/client/inventory.h>

#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/utils.h>
#include <seiscomp/datamodel/parameter.h>
#include <seiscomp/datamodel/parameterset.h>
#include <seiscomp/datamodel/utils.h>

#include <seiscomp/io/archive/xmlarchive.h>

#include <seiscomp/processing/amplitudeprocessor.h>

#include <functional>
#include <iomanip>


using namespace std;
using namespace Seiscomp;
using namespace Seiscomp::Core;
using namespace Seiscomp::Client;
using namespace Seiscomp::Processing;
using namespace Seiscomp::DataModel;
using namespace Private;

#define _T(name) database()->convertColumnName(name)
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool AmpTool::CompareWaveformStreamID::operator()(
	const Seiscomp::DataModel::WaveformStreamID &one,
	const Seiscomp::DataModel::WaveformStreamID &other
) const
{
	if ( one.networkCode() < other.networkCode() ) return true;
	if ( one.networkCode() > other.networkCode() ) return false;

	if ( one.stationCode() < other.stationCode() ) return true;
	if ( one.stationCode() > other.stationCode() ) return false;

	if ( one.locationCode() < other.locationCode() ) return true;
	if ( one.locationCode() > other.locationCode() ) return false;

	return one.channelCode() < other.channelCode();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
AmpTool::AmpTool(int argc, char **argv) : StreamApplication(argc, argv) {
	setAutoApplyNotifierEnabled(true);
	setInterpretNotifierEnabled(true);

	setLoadInventoryEnabled(true);
	setLoadConfigModuleEnabled(true);

	setPrimaryMessagingGroup("AMPLITUDE");

	addMessagingSubscription("PICK");
	addMessagingSubscription("AMPLITUDE");
	addMessagingSubscription("LOCATION");

	setAutoAcquisitionStart(false);

	_amplitudeTypes.insert("MLv");
	_amplitudeTypes.insert("mb");
	_amplitudeTypes.insert("mB");
	_cache.setPopCallback(bind(&AmpTool::removedFromCache, this, placeholders::_1));

	_errorChannel = NULL;
	_errorOutput = NULL;

	_processingInfoChannel = NULL;
	_processingInfoOutput = NULL;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
AmpTool::~AmpTool() {
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AmpTool::createCommandLineDescription() {
	Application::createCommandLineDescription();

	commandline().addOption("Messaging", "test", "Test mode, no messages are sent.");
	commandline().addOption("Generic", "expiry,x",
	                        "Time span in hours after which objects expire.", &_fExpiry, true);
	commandline().addOption("Generic", "origin-id,O",
	                        "OriginID to calculate amplitudes for.", &_originID, true);
	commandline().addOption("Generic", "dump-records",
	                        "Dumps the filtered traces to ASCII when using -O.");

	commandline().addGroup("Input");
	commandline().addOption("Input", "ep",
	                        "Event parameters XML file for offline processing of "
	                        "all contained origins. Use '-' to read from stdin.",
	                        &_epFile);
	commandline().addOption("Input", "picks,p",
	                        "Force measuring amplitudes for picks only. Origins "
	                        "are ignored and time windows are independent of "
	                        "distance. Works only in combination with --ep.");
	commandline().addOption("Input", "reprocess",
	                        "Reprocess and update existing (non manual)"
	                        "amplitudes in combination with --ep.");

	commandline().addGroup("Reprocess");
	commandline().addOption("Reprocess", "force",
	                        "Force reprocessing of amplitudes even if they are manual.");
	commandline().addOption("Reprocess", "start-time",
	                        "Start time for amplitude request window.",
	                        &_strTimeWindowStartTime);
	commandline().addOption("Reprocess", "end-time",
	                        "End time for amplitude request window.",
	                        &_strTimeWindowEndTime);
	commandline().addOption("Reprocess", "commit",
	                        "Send amplitude updates to the messaging otherwise"
	                        "an XML document will be output.");

	commandline().addGroup("Output");
	commandline().addOption("Output", "formatted,f",
	                        "Use formatted XML output. Otherwise XML is unformatted.");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool AmpTool::validateParameters() {
	if ( !Application::validateParameters() ) {
		return false;
	}

	_testMode = commandline().hasOption("test");

	if ( !_originID.empty() && _testMode )
		setMessagingEnabled(false);

	if ( !_epFile.empty() ) {
		setMessagingEnabled(false);
		setLoggingToStdErr(true);

		if ( !isInventoryDatabaseEnabled() && !isConfigDatabaseEnabled() )
			setDatabaseEnabled(false, false);
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool AmpTool::initConfiguration() {
	if ( !Client::StreamApplication::initConfiguration() ) {
		return false;
	}

	try {
		std::vector<std::string> amplitudes = configGetStrings("amplitudes");
		_amplitudeTypes.clear();
		_amplitudeTypes.insert(amplitudes.begin(), amplitudes.end());
	}
	catch (...) {}

	try { _minWeight = configGetDouble("amptool.minimumPickWeight"); }
	catch ( ... ) {}

	try { _replaceWaveformIDWithBindings = configGetBool("amptool.streamFromBindings"); }
	catch ( ... ) {}

	try { _initialAcquisitionTimeout = configGetDouble("amptool.initialAcquisitionTimeout"); }
	catch ( ... ) {}

	try { _runningAcquisitionTimeout = configGetDouble("amptool.runningAcquisitionTimeout"); }
	catch ( ... ) {}

	_dumpRecords = commandline().hasOption("dump-records");
	_reprocessAmplitudes = commandline().hasOption("reprocess");
	_picks = commandline().hasOption("picks");
	_forceReprocessing = commandline().hasOption("force");

	_formatted = commandline().hasOption("formatted");

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool AmpTool::init() {
	if ( !StreamApplication::init() )
		return false;

	if ( _picks && _epFile.empty() ) {
		SC_FMT_ERROR("--picks is only allowed with --ep");
		return false;
	}

	_inputPicks = addInputObjectLog("pick");
	_inputAmps = addInputObjectLog("amplitude");
	_inputOrgs = addInputObjectLog("origin");
	_outputAmps = addOutputObjectLog("amplitude", primaryMessagingGroup());

	_errorChannel = SEISCOMP_DEF_LOGCHANNEL("processing/error", Logging::LL_ERROR);
	_errorOutput = new Logging::FileRotatorOutput(Environment::Instance()->logFile("scamp-processing-error").c_str(),
	                                              60*60*24, 30);

	// Log into processing/info to avoid logging the same information into the global info channel
	_processingInfoChannel = SEISCOMP_DEF_LOGCHANNEL("processing/info", Logging::LL_INFO);
	_processingInfoOutput = new Logging::FileRotatorOutput(Environment::Instance()->logFile("scamp-processing-info").c_str(),
	                                                       60*60*24, 30);

	_errorOutput->subscribe(_errorChannel);
	_processingInfoOutput->subscribe(_processingInfoChannel);

	_cache.setTimeSpan(TimeSpan(_fExpiry*3600.));
	_cache.setDatabaseArchive(query());

	AmplitudeTypeList *services = AmplitudeProcessorFactory::Services();

	if ( services ) {
		_registeredAmplitudeTypes = *services;
		delete services;
	}

	std::string logAmplTypes;
	for ( AmplitudeList::iterator it = _amplitudeTypes.begin();
	      it != _amplitudeTypes.end(); ) {
		logAmplTypes += " * ";

		if ( std::find(_registeredAmplitudeTypes.begin(),
		               _registeredAmplitudeTypes.end(), *it) == _registeredAmplitudeTypes.end() ) {
			logAmplTypes += *it;
			logAmplTypes += ": Disabled (unknown type)";
			_amplitudeTypes.erase(it++);
		}
		else {
			logAmplTypes += *it;
			logAmplTypes += ": OK";
			++it;
		}

		logAmplTypes += '\n';
	}

	SEISCOMP_INFO("\nAmplitudes to calculate:\n%s", logAmplTypes.c_str());

	_timer.setTimeout(1);
	_timer.setCallback(bind(&AmpTool::handleTimeout, this));

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool AmpTool::run() {
	if ( !_originID.empty() ) {
		OriginPtr org = Origin::Cast(query()->getObject(Origin::TypeInfo(), _originID));
		if ( !org ) {
			cerr << "Origin not found!" << endl;
			return false;
		}

		_fetchMissingAmplitudes = false;
		query()->loadArrivals(org.get());
		process(org.get(), nullptr);
		return true;
	}

	if ( !_strTimeWindowStartTime.empty() || !_strTimeWindowEndTime.empty() ) {
		if ( database() == NULL ) {
			cerr << "No database currently active for time window reprocessing" << endl;
			return false;
		}

		OPT(Core::Time) startTime, endTime;

		if ( !_strTimeWindowStartTime.empty() ) {
			Core::Time tmp;
			if ( !tmp.fromString(_strTimeWindowStartTime.c_str(), "%F %T") ) {
				cerr << "Invalid start time: " << _strTimeWindowStartTime << endl;
				return false;
			}
			startTime = tmp;
		}

		if ( !_strTimeWindowEndTime.empty() ) {
			Core::Time tmp;
			if ( !tmp.fromString(_strTimeWindowEndTime.c_str(), "%F %T") ) {
				cerr << "Invalid end time: " << _strTimeWindowEndTime << endl;
				return false;
			}
			endTime = tmp;
		}

		std::string dbQuery;
		dbQuery += "select PPick." + _T("publicID") + ", Pick.* from Pick,PublicObject as PPick,Amplitude "
		           "where Pick._oid=PPick._oid and Amplitude." + _T("pickID") + "=PPick." + _T("publicID");

		if ( startTime ) {
			 dbQuery += " and Pick." + _T("time_value") + ">='" + startTime->toString("%F %T") + "'";
		}

		if ( endTime ) {
			 dbQuery += " and Pick." + _T("time_value") + "<'" + endTime->toString("%F %T") + "'";
		}

		dbQuery += " group by PPick." + _T("publicID") + ", Pick._oid";

		if ( !commandline().hasOption("commit") ) {
			_testMode = true;
		}

		EventParametersPtr ep;
		if ( _testMode ) {
			ep = new EventParameters;
		}

		using PickList = list<PickPtr>;
		PickList picks;

		cerr << "Collecting picks ... " << flush;
		DatabaseIterator db_it = query()->getObjectIterator(dbQuery, Pick::TypeInfo());
		ObjectPtr obj;
		while ( (obj = db_it.get()) ) {
			Pick *pick = static_cast<Pick*>(obj.get());
			try {
				pick->waveformID().networkCode();
				pick->waveformID().stationCode();
				pick->waveformID().locationCode();
				pick->waveformID().channelCode();
				pick->time().value();
			}
			catch ( ... ) {
				continue;
			}

			++db_it;

			picks.push_back(pick);
			if ( ep ) ep->add(pick);
		}

		db_it.close();

		cerr << picks.size() << endl;

		_report << std::endl;
		_report << "Reprocessing report" << std::endl;
		_report << "-------------------" << std::endl;

		_report << " + Picks" << std::endl;

		int errors = 0;
		int ampsRecomputed = 0;
		int messagesSent = 0;

		int idx = 1;
		for ( PickList::iterator it = picks.begin(); it != picks.end(); ++it, ++idx ) {
			PickPtr pick = *it;
			SingleAmplitudeMap dbAmps;

			if ( isExitRequested() ) break;

			// Clear all processors
			_processors.clear();

			// Clear all station time windows
			_stationRequests.clear();

			_report << "   + " << pick->publicID() << std::endl;
			cerr << "[" << idx << "]" << " " << (*it)->publicID() << endl;
			db_it = query()->getAmplitudesForPick((*it)->publicID());
			while ( (obj = db_it.get()) ) {
				Amplitude *amp = static_cast<Amplitude*>(obj.get());
				cerr << "  [" << setw(10) << left << amp->type() << "]  ";

				AmplitudeProcessorPtr proc = AmplitudeProcessorFactory::Create(amp->type().c_str());
				if ( !proc ) {
					if ( _amplitudeTypes.find(amp->type()) == _amplitudeTypes.end() )
						cerr << "No processor";
					else {
						cerr << "No processor but enabled";
						++errors;
					}
				}
				else {
					cerr << "Fetch data";
					dbAmps[amp->type()] = amp;
					proc->setTrigger(pick->time().value());
					proc->setReferencingPickID(pick->publicID());
					proc->setPublishFunction(bind(&AmpTool::storeLocalAmplitude, this, placeholders::_1, placeholders::_2));
					_report << "     + Data" << std::endl;
					addProcessor(proc.get(), NULL, pick.get(), None, None, None);
				}

				cerr << endl;
				++db_it;
			}

			db_it.close();

			cerr << "  --------------------------------" << endl;

			if ( _stationRequests.empty() ) continue;

			for ( RequestMap::iterator it = _stationRequests.begin(); it != _stationRequests.end(); ++it ) {
				StationRequest &req = it->second;
				for ( WaveformIDSet::iterator wit = req.streams.begin(); wit != req.streams.end(); ++wit ) {
					const WaveformStreamID &wsid = *wit;
					recordStream()->addStream(wsid.networkCode(), wsid.stationCode(),
					                          wsid.locationCode(), wsid.channelCode(),
					                          req.timeWindow.startTime(),
					                          req.timeWindow.endTime());
				}

				_report << " + TimeWindow (" << it->first << "): " << req.timeWindow.startTime().toString("%F %T")
				        << ", " << req.timeWindow.endTime().toString("%F %T") << std::endl;
			}

			_reprocessMap.clear();
			readRecords(false);

			list<AmplitudePtr> updates;

			for ( AmplitudeMap::iterator it = dbAmps.begin();
			      it != dbAmps.end(); ++it ) {
				AmplitudePtr oldAmp = it->second;
				AmplitudePtr newAmp = _reprocessMap[oldAmp->type()];

				cerr << "  [" << setw(10) << left << oldAmp->type() << "]  " << oldAmp->amplitude().value() << "  ";
				if ( newAmp ) {
					if ( newAmp->amplitude().value() != oldAmp->amplitude().value() ) {
						*oldAmp = *newAmp;
						if ( ep )
							ep->add(oldAmp.get());
						else
							updates.push_back(oldAmp);
						cerr << "->  " << newAmp->amplitude().value();
					}
					else
						cerr << "  no changes";

					++ampsRecomputed;
				}
				else {
					cerr << "-";
					++errors;
				}
				cerr << endl;
			}

			if ( !updates.empty() ) {
				if ( !_testMode ) {
					NotifierMessagePtr nmsg = new NotifierMessage;
					for ( list<AmplitudePtr>::iterator it = updates.begin();
					      it != updates.end(); ++it ) {
						nmsg->attach(new Notifier("EventParameters", OP_UPDATE, it->get()));
					}

					connection()->send(nmsg.get());
					++messagesSent;
				}
				else {
					cerr << "  --------------------------------" << endl;
					cerr << "  Test mode, nothing sent" << endl;
				}
			}
		}

		if ( ep ) {
			IO::XMLArchive ar;
			ar.create("-");
			ar.setFormattedOutput(true);
			ar << ep;
			ar.close();
		}

		cerr << "----------------------------------" << endl;
		cerr << "Recomputed " << ampsRecomputed << " amplitudes" << endl;
		cerr << "Sent " << messagesSent << " messages" << endl;
		if ( errors )
			cerr << errors << " errors occurred, check the processing log" << endl;

		return true;
	}


	if ( !_epFile.empty() ) {
		_fetchMissingAmplitudes = false;

		// Disable database
		setDatabase(NULL);
		_cache.setDatabaseArchive(NULL);

		IO::XMLArchive ar;
		if ( !ar.open(_epFile.c_str()) ) {
			SEISCOMP_ERROR("Failed to open %s", _epFile.c_str());
			return false;
		}

		ar >> _ep;
		ar.close();

		if ( !_ep ) {
			SEISCOMP_ERROR("No event parameters found in %s", _epFile.c_str());
			return false;
		}

		if ( commandline().hasOption("reprocess") ) {
			for ( size_t i = 0; i < _ep->amplitudeCount(); ++i ) {
				AmplitudePtr amp = _ep->amplitude(i);
				feed(amp.get());
			}
		}

		if ( _picks) {
			for ( size_t i = 0; i < _ep->pickCount(); ++i ) {
				PickPtr pick = _ep->pick(i);
				SEISCOMP_INFO("Processing pick %s", pick->publicID().c_str());
				process(nullptr, pick.get());
				if ( isExitRequested() ) {
					break;
				}
			}
		}
		else {
			for ( size_t i = 0; i < _ep->originCount(); ++i ) {
				OriginPtr org = _ep->origin(i);
				SEISCOMP_INFO("Processing origin %s", org->publicID().c_str());
				process(org.get(), nullptr);
				if ( isExitRequested() ) break;
			}
		}

		ar.create("-");
		ar.setFormattedOutput(_formatted);
		ar << _ep;
		ar.close();

		_ep = NULL;

		return true;
	}

	return StreamApplication::run();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AmpTool::done() {
	Seiscomp::Client::StreamApplication::done();

	if ( _errorChannel ) delete _errorChannel;
	if ( _errorOutput ) delete _errorOutput;

	if ( _processingInfoChannel ) delete _processingInfoChannel;
	if ( _processingInfoOutput ) delete _processingInfoOutput;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AmpTool::addObject(const std::string& parentID, DataModel::Object* object) {
	updateObject(parentID, object);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AmpTool::updateObject(const std::string &parentID, Object* object) {
	Pick *pick = Pick::Cast(object);
	if ( pick ) {
		logObject(_inputPicks, Time::UTC());
		feed(pick);
		return;
	}

	Amplitude *amp = Amplitude::Cast(object);
	if ( amp && !amp->pickID().empty() ) {
		logObject(_inputAmps, Time::UTC());
		PickPtr pick = _cache.get<Pick>(amp->pickID());
		// No pick, no amplitude
		if ( !pick )
			return;

		AmplitudeMap::iterator it = _pickAmplitudes.find(pick->publicID());
		// If there is no amplitude associated to the pick load all
		// amplitudes from the database
		if ( it == _pickAmplitudes.end() )
			loadAmplitudes(pick->publicID(), amp);
		feed(amp);
		return;
	}

	Origin *origin = Origin::Cast(object);
	if ( origin ) {
		logObject(_inputOrgs, Time::UTC());
		process(origin, nullptr);
		return;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AmpTool::removeObject(const std::string &parentID, Object* object) {
	Pick *pick = Pick::Cast(object);
	if ( pick ) {
		logObject(_inputPicks, Time::UTC());
		Pick *cachedPick = Pick::Find(pick->publicID());
		if ( cachedPick )
			_cache.remove(cachedPick);
		return;
	}

	Amplitude *amp = Amplitude::Cast(object);
	if ( amp && !amp->pickID().empty() ) {
		logObject(_inputAmps, Time::UTC());
		Amplitude *cachedAmplitude = Amplitude::Find(amp->publicID());
		if ( cachedAmplitude ) {
			AmplitudeRange range = _pickAmplitudes.equal_range(cachedAmplitude->pickID());
			for ( AmplitudeMap::iterator it = range.first; it != range.second; ) {
				if ( it->second == cachedAmplitude )
					_pickAmplitudes.erase(it++);
				else
					++it;
			}
		}
		return;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AmpTool::process(Origin *origin, Pick *pickInput) {
	if ( origin && pickInput ) {
		SEISCOMP_ERROR("Provide either origin or pick, not both");
		return;
	}

	if ( origin && Private::status(origin) == REJECTED ) {
		SEISCOMP_INFO("Ignoring origin %s with status = REJECTED",
		              origin->publicID().c_str());
		return;
	}

	PickPtr pick = nullptr;
	if ( pickInput ){
		pick = pickInput;
		try {
			if ( pick->evaluationStatus() == REJECTED ) {
				SEISCOMP_INFO("Ignoring pick %s with status = REJECTED",
				              pick->publicID().c_str());
				return;
			}
		}
		catch ( Core::ValueException& ) {}
	}

	if ( _amplitudeTypes.empty() ) {
		// No amplitudes to calculate
		return;
	}

	// Clear all processors
	_processors.clear();

	// Clear all station time windows
	_stationRequests.clear();

	// Typedef a pickmap entry containing the pick and
	// the distance of the station from the origin
	struct PickStreamEntry {
		PickCPtr pick;
		double distance;
		bool used{false};
	};

	// Typedef a pickmap that maps a streamcode to a pick
	using PickStreamMap = map<string, PickStreamEntry>;
	std::map<std::string, bool> considerUnusedArrivals;

	for ( const auto &type : _amplitudeTypes ) {
		bool consider = false;
		try {
			consider = configGetBool(
				fmt::format("amplitudes.{}.considerUnusedArrivals", type)
			);
		}
		catch ( ... ) {}
		considerUnusedArrivals[type] = consider;
	}

	// This map is needed to find the earliest P pick of
	// a certain stream
	PickStreamMap pickStreamMap;

	_ampIDReuse.clear();

	if ( origin ) {
		_report << std::endl;
		_report << "Processing report for Origin: " << origin->publicID() << std::endl;
		_report << "-----------------------------------------------------------------" << std::endl;

		_report << " + Arrivals" << endl;

		if ( (origin->arrivalCount() == 0) && query() ) {
			query()->loadArrivals(origin);
		}

		for ( size_t i = 0; i < origin->arrivalCount(); ++i ) {
			Arrival *arr = origin->arrival(i);
			const string &pickID = arr->pickID();

			double weight = Private::arrivalWeight(arr);

			if ( Private::shortPhaseName(arr->phase().code()) != 'P' ) {
				SEISCOMP_INFO("Ignoring pick '%s' weight=%.1f phase=%s",
				              pickID.c_str(), weight, arr->phase().code().c_str());
				continue;
			}

			pick = _cache.get<Pick>(pickID);
			if ( !pick ) {
				SEISCOMP_LOG(_errorChannel, "Pick '%s' not found", pickID.c_str());
				_report << "   - " << pickID << " [pick not found]" << std::endl;
				continue;
			}

			DataModel::WaveformStreamID wfid = pick->waveformID();
			// Strip the component code because every AmplitudeProcessor
			// will use its own component to pick the amplitude on
			wfid.setChannelCode(wfid.channelCode().substr(0,2));

			string streamID = Private::toStreamID(wfid);

			double distance;
			try {
				distance = Private::arrivalDistance(arr);
			}
			catch ( Core::ValueException& ) {
				SEISCOMP_LOG(_errorChannel, "Arrival.distance not set for %s pick %s: skip it",
				                 streamID.c_str(), pickID.c_str());
				_report << "   - " << pickID << " [Arrival.distance not set]" << std::endl;
				continue;
			}

			auto &e = pickStreamMap[streamID];

			// When there is already a pick registered for this stream which has
			// been picked earlier, ignore the current pick
			if ( e.pick && e.pick->time().value() < pick->time().value() ) {
				continue;
			}

			e.pick = pick;
			e.distance = distance;
			if ( !e.used ) {
				e.used = weight >= _minWeight;
			}
		}
	}
	else if ( pick ) {
		_report << std::endl;
		_report << "Processing report for Pick: " << pick->publicID() << std::endl;
		_report << "-----------------------------------------------------------------" << std::endl;

		auto wfid = pick->waveformID();
		// Strip the component code because every AmplitudeProcessor
		// will use its own component to pick the amplitude on
		wfid.setChannelCode(wfid.channelCode().substr(0,2));
		string streamID = Private::toStreamID(wfid);
		auto &e = pickStreamMap[streamID];
		e.pick = pick;
		// there is no distance for picks without origins
		e.distance = 0.0;
		e.used = true;
	}

	for ( auto &[streamID, e] : pickStreamMap ) {
		PickCPtr pick = e.pick;
		const string &pickID = pick->publicID();
		const Time &pickTime = pick->time().value();
		double distance = e.distance;
		OPT(double) depth;

		AmplitudeRange amps = getAmplitudes(pickID);

		_report << "   + " << pickID << ", " << Private::toStreamID(pick->waveformID()) << std::endl;
		if ( origin ) {
			try {
				depth = origin->depth().value();
				_report << "     + depth = " << *depth << std::endl;
			}
			catch ( ... ) {
				_report << "     - depth [not set]" << std::endl;
			}
			_report << "     + distance = " << distance << std::endl;
		}

		for ( const auto &type : _amplitudeTypes ) {
			// Station is disabled, check if amplitude type is overriden
			if ( !e.used ) {
				if ( auto ait = considerUnusedArrivals.find(type);
				     ait == considerUnusedArrivals.end() || !ait->second ) {
					_report << "     - " << type << " [arrival is disabled]" << std::endl;
					continue;
				}
			}

			AmplitudePtr existingAmp = hasAmplitude(amps, type);

			if ( existingAmp ) {
				if ( !_reprocessAmplitudes ) {
					SEISCOMP_INFO("Skipping %s calculation for pick %s, amplitude exists already",
					              type.c_str(), pickID.c_str());
					_report << "     - " << type << " [amplitude exists already]" << std::endl;
					continue;
				}

				try {
					if ( existingAmp->evaluationMode() == DataModel::MANUAL ) {
						if ( !_forceReprocessing ) {
							SEISCOMP_INFO("Skipping %s calculation for pick %s, amplitude exists already and is manual (use --force)",
							              type.c_str(), pickID.c_str());
							_report << "     - " << type << " [manual amplitude exists already]" << std::endl;
							continue;
						}
					}
				}
				catch ( ... ) {}
			}

			AmplitudeProcessorPtr proc = AmplitudeProcessorFactory::Create(type.c_str());
			if ( !proc ) {
				SEISCOMP_LOG(_errorChannel, "Failed to create AmplitudeProcessor %s", type.c_str());
				_report << "     - " << type << " [amplitudeprocessor NULL]" << std::endl;
				continue;
			}

			if ( existingAmp ) {
				_ampIDReuse[proc.get()] = existingAmp;
			}

			proc->setTrigger(pickTime);
			proc->setReferencingPickID(pickID);

			int res = -1;
			if ( origin ) {
				res = addProcessor(proc.get(), origin, pick.get(), distance, depth, (double) origin->time().value());
			}
			else {
				SC_FMT_DEBUG("Measuring {} amplitude for pick {} independent of origin",
				             type.c_str(), pick->publicID().c_str());
				res = addProcessor(proc.get(), nullptr, pick.get(), None, None, None);
			}

			if ( res < 0 ) {
				// RecordStream not available
				if ( res == -2 ) {
					return;
				}
				continue;
			}

			proc->setPublishFunction(bind(&AmpTool::emitAmplitude, this, placeholders::_1, placeholders::_2));
		}
	}

	if ( _processors.empty() ) {
		_report << " + Nothing to do, no processors to add" << std::endl;
		printReport();
		return;
	}

	for ( RequestMap::iterator it = _stationRequests.begin(); it != _stationRequests.end(); ++it ) {
		StationRequest &req = it->second;
		for ( WaveformIDSet::iterator wit = req.streams.begin(); wit != req.streams.end(); ++wit ) {
			const WaveformStreamID &wsid = *wit;
			recordStream()->addStream(wsid.networkCode(), wsid.stationCode(),
			                          wsid.locationCode(), wsid.channelCode(),
			                          req.timeWindow.startTime(),
			                          req.timeWindow.endTime());
		}

		SC_FMT_DEBUG("{} time window: {} - {}",
		             it->first, req.timeWindow.startTime().toString("%F %T"),
		             req.timeWindow.endTime().toString("%F %T"));
		_report << " + TimeWindow (" << it->first << "): " << req.timeWindow.startTime().toString("%F %T")
		        << ", " << req.timeWindow.endTime().toString("%F %T") << std::endl;
	}

	SEISCOMP_INFO("Set stream timeout to %f seconds", _initialAcquisitionTimeout);
	_acquisitionTimeout = _initialAcquisitionTimeout;
	_firstRecord = true;

	_result << " + Processing" << std::endl;

	_acquisitionTimer.restart();
	_noDataTimer.restart();
	_hasRecordsReceived = false;

	SEISCOMP_INFO("Starting timeout monitor");
	_timer.start();
	readRecords(false);
	if ( _timer.isActive() ) {
		_timer.stop();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int AmpTool::addProcessor(Processing::AmplitudeProcessor *proc,
                          const Seiscomp::DataModel::Origin *origin,
                          const DataModel::Pick *pick,
                          OPT(double) distance, OPT(double) depth, OPT(double) originTime) {
	WaveformProcessor::Component components[3];
	int componentCount = 0;

	// Lookup station parameters of config module
	Util::KeyValues *params = NULL;
	std::string stationID = pick->waveformID().networkCode() + "." +
	                        pick->waveformID().stationCode();
	ParameterMap::iterator it = _parameters.find(stationID);
	if ( it != _parameters.end() ) {
		params = it->second.get();
	}
	else if ( configModule() != NULL ) {
		for ( size_t i = 0; i < configModule()->configStationCount(); ++i ) {
			ConfigStation *station = configModule()->configStation(i);

			if ( station->networkCode() != pick->waveformID().networkCode() ) continue;
			if ( station->stationCode() != pick->waveformID().stationCode() ) continue;

			Setup *setup = findSetup(station, name());
			if ( setup ) {
				ParameterSet *ps = ParameterSet::Find(setup->parameterSetID());

				if ( !ps ) {
					SEISCOMP_ERROR("Cannot find parameter set %s", setup->parameterSetID().c_str());
					continue;
				}

				Util::KeyValuesPtr keys = new Util::KeyValues;
				keys->init(ps);
				_parameters[stationID] = keys;
				params = keys.get();
			}
		}
	}

	switch ( proc->usedComponent() ) {
		case AmplitudeProcessor::Vertical:
			components[0] = WaveformProcessor::VerticalComponent;
			componentCount = 1;
			break;
		case AmplitudeProcessor::FirstHorizontal:
			components[0] = WaveformProcessor::FirstHorizontalComponent;
			componentCount = 1;
			break;
		case AmplitudeProcessor::SecondHorizontal:
			components[0] = WaveformProcessor::SecondHorizontalComponent;
			componentCount = 1;
			break;
		case AmplitudeProcessor::Horizontal:
			components[0] = WaveformProcessor::FirstHorizontalComponent;
			components[1] = WaveformProcessor::SecondHorizontalComponent;
			componentCount = 2;
			break;
		case AmplitudeProcessor::Any:
			components[0] = WaveformProcessor::VerticalComponent;
			components[1] = WaveformProcessor::FirstHorizontalComponent;
			components[2] = WaveformProcessor::SecondHorizontalComponent;
			componentCount = 3;
			break;
		default:
			_report << "     - " << proc->type() << " [unsupported component " << proc->usedComponent() << "]"
			        << std::endl;
			return -1;
	}


	std::string streamIDs[3];
	WaveformStreamID cwids[3];
	WaveformStreamID wid = pick->waveformID();

	// Adjust waveformID with bindings
	if ( _replaceWaveformIDWithBindings && params ) {
		string tmp;
		if ( params->getString(tmp, "detecLocid") ) {
			wid.setLocationCode(tmp);
		}
		if ( params->getString(tmp, "detecStream") ) {
			wid.setChannelCode(tmp);
		}
	}

	Client::Inventory *inv = Client::Inventory::Instance();
	DataModel::ThreeComponents tc;
	if ( inv ) {
		try {
			DataModel::SensorLocation *loc = inv->getSensorLocation(
				wid.networkCode(), wid.stationCode(),
				wid.locationCode(), pick->time().value()
			);

			if ( !loc ) {
				throw Core::ValueException("sensor location not found");
			}

			if ( wid.channelCode().size() >= 3 ) {
				DataModel::getThreeComponents(tc, loc, wid.channelCode().substr(0, wid.channelCode().size()-1).c_str(), pick->time().value());
			}
			else {
				DataModel::getThreeComponents(tc, loc, wid.channelCode().c_str(), pick->time().value());
			}
		}
		catch ( ... ) {}
	}

	DataModel::SensorLocation *receiver = NULL;

	for ( int i = 0; i < componentCount; ++i ) {
		cwids[i] = wid;

		if ( tc.comps[components[i]] == NULL ) {
			SEISCOMP_LOG(_errorChannel, "%s: no inventory information found for %s (%d) -> ignoring Arrival %s",
			             proc->type().c_str(),
			             Private::toStreamID(cwids[i]).c_str(), i, pick->publicID().c_str());
			_report << "   - " << proc->type().c_str() << " [streams not found]" << std::endl;
			return -1;
		}

		cwids[i].setChannelCode(tc.comps[components[i]]->code());
		streamIDs[i] = Private::toStreamID(cwids[i]);

		if ( cwids[i].channelCode().empty() ) {
			SEISCOMP_LOG(_errorChannel, "%s: invalid channel code in %s -> ignoring Arrival %s",
			             proc->type().c_str(), streamIDs[i].c_str(), pick->publicID().c_str());
			_report << "   - " << proc->type().c_str() << " [invalid channel code]" << std::endl;
			return -1;
		}

		StreamMap::iterator it = _streams.find(streamIDs[i]);
		if ( it != _streams.end() )
			proc->streamConfig(components[i]) = *it->second;
		else {
			Processing::StreamPtr stream = new Processing::Stream;
			stream->init(cwids[i].networkCode(),
			             cwids[i].stationCode(),
			             cwids[i].locationCode(),
			             cwids[i].channelCode(),
			             pick->time().value());
			_streams[streamIDs[i]] = stream;

			proc->streamConfig(components[i]) = *stream;
		}

		if ( !i )
			receiver = tc.comps[components[i]]->sensorLocation();

		if ( proc->streamConfig(components[i]).gain == 0.0 ) {
			SEISCOMP_LOG(_errorChannel, "%s: no gain found for %s -> ignoring Arrival %s",
			             proc->type().c_str(), streamIDs[i].c_str(), pick->publicID().c_str());
			_report << "     - " << proc->type().c_str() << " [gain not found]" << std::endl;
			return -1;
		}
	}


	// If initialization fails, abort
	if ( !proc->setup(
		Settings(
			configModuleName(),
			wid.networkCode(),
			wid.stationCode(),
			wid.locationCode(),
			wid.channelCode().substr(0,2),
			&configuration(), params)) ) {
		_report << "     - " << proc->type().c_str() << " [setup failed]" << std::endl;
		return -1;
	}

	if ( origin ) {
		proc->setEnvironment(origin, receiver, pick);
	}
	else {
		const std::string &n = wid.networkCode();
		const std::string &s = wid.stationCode();
		const std::string &l = wid.locationCode();
		proc->setEnvironment(
			nullptr, // No hypocenter information
			Client::Inventory::Instance()->getSensorLocation(
				n, s, l, proc->trigger()
			),
			pick
		);
	}

	proc->computeTimeWindow();

	if ( proc->isFinished() ) {
		_report << "     - " << proc->type() << " [" << proc->status().toString() << " (" << proc->statusValue() << ")]" << std::endl;
		// NOTE: Do not create a dummy amplitude that prevents the calculation
		//       for the next origin because initial criterias like Depth and
		//       distance can change significantly for the next origin
		//createDummyAmplitude(proc.get());
		return -1;
	}


	if ( _processors.empty() )
		openStream();

	if ( recordStream() == NULL ) {
		SEISCOMP_ERROR("Opening the acquisition stream failed");
		printReport();
		return -2;
	}

	for ( int i = 0; i < componentCount; ++i ) {
		pair<ProcessorMap::iterator, bool> handle =
			_processors.insert(ProcessorMap::value_type(streamIDs[i], ProcessorSlot()));

		// Update processors station time window
		StationRequest &req = _stationRequests[stationID];
		if ( req.timeWindow.length() > TimeSpan(0, 0) ) {
			req.timeWindow = req.timeWindow | proc->safetyTimeWindow();
		}
		else {
			req.timeWindow = proc->safetyTimeWindow();
		}

		// The second value of the pair describes whether a new entry has been inserted or not
		if ( handle.second ) {
			req.streams.insert(cwids[i]);
			//addStream(cwids[i].networkCode(), cwids[i].stationCode(), cwids[i].locationCode(), cwids[i].channelCode());
			_report << "       + stream request = " << streamIDs[i] << std::endl;
		}

		handle.first->second.push_back(proc);
	}

	_report << "     + " << proc->type() << std::endl;

	return 0;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AmpTool::printReport() {
	SEISCOMP_LOG(_processingInfoChannel, "%s%s", _report.str().c_str(),
	             _result.str().c_str());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AmpTool::feed(Seiscomp::DataModel::Pick *pick) {
	if ( isAgencyIDAllowed(objectAgencyID(pick)) )
		_cache.feed(pick);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AmpTool::feed(Seiscomp::DataModel::Amplitude *amp) {
	if ( isAgencyIDAllowed(objectAgencyID(amp)) )
		_pickAmplitudes.insert(AmplitudeMap::value_type(amp->pickID(), amp));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AmpTool::removedFromCache(Seiscomp::DataModel::PublicObject *po) {
	size_t amplCount = _pickAmplitudes.erase(po->publicID());
	SEISCOMP_DEBUG("Removed object %s from cache, removed %lu amplitudes",
	               po->publicID().c_str(), (unsigned long)amplCount);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
size_t AmpTool::loadAmplitudes(const std::string &pickID,
                               Seiscomp::DataModel::Amplitude *ignoreAmp) {
	if ( !_fetchMissingAmplitudes ) return 0;
	// Load all amplitudes from the database
	size_t cnt = 0;
	DatabaseIterator it = query()->getAmplitudesForPick(pickID);
	for ( ; *it; ++it ) {
		AmplitudePtr staAmp = Amplitude::Cast(*it);
		if ( staAmp && staAmp != ignoreAmp ) {
			++cnt;
			feed(staAmp.get());
		}
	}

	return cnt;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
AmpTool::AmplitudeRange AmpTool::getAmplitudes(const std::string &pickID) {
	AmplitudeRange range = _pickAmplitudes.equal_range(pickID);

	// No amplitude stored yet?
	if ( range.first == range.second ) {
		loadAmplitudes(pickID);

		// Reload the amplitudes from local cache
		range = _pickAmplitudes.equal_range(pickID);
	}

	return range;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Amplitude *AmpTool::hasAmplitude(const AmplitudeRange &range,
                                 const std::string &type) const {
	for ( AmplitudeMap::iterator it = range.first; it != range.second; ++it )
		if ( it->second->type() == type ) return it->second.get();

	return NULL;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AmpTool::createDummyAmplitude(const AmplitudeProcessor *proc) {
	AmplitudePtr amp = Amplitude::Create();
	amp->setType(proc->type());
	amp->setPickID(proc->referencingPickID());
	// NOTE: Don't call feed here to bypass the agency check
	_pickAmplitudes.insert(AmplitudeMap::value_type(amp->pickID(), amp));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Seiscomp::DataModel::AmplitudePtr
AmpTool::createAmplitude(const Seiscomp::Processing::AmplitudeProcessor *proc,
                         const Seiscomp::Processing::AmplitudeProcessor::Result &res) {
	AmplitudePtr amp = Amplitude::Create();
	CreationInfo ci;
	if ( amp == NULL ) {
		SEISCOMP_LOG(_errorChannel, "Failed to create Amplitude %s for %s", proc->type().c_str(), res.record->streamID().c_str());
		return NULL;
	}

	amp->setAmplitude(
		RealQuantity(
			res.amplitude.value, Core::None,
			res.amplitude.lowerUncertainty, res.amplitude.upperUncertainty,
			Core::None
		)
	);

	if ( res.period > 0 ) amp->setPeriod(RealQuantity(res.period));
	if ( res.snr >= 0 ) amp->setSnr(res.snr);
	amp->setType(proc->type());
	amp->setUnit(proc->unit());
	amp->setTimeWindow(
		DataModel::TimeWindow(res.time.reference, res.time.begin, res.time.end)
	);

	if ( res.component <= WaveformProcessor::SecondHorizontal ) {
		amp->setWaveformID(
			WaveformStreamID(
				res.record->networkCode(), res.record->stationCode(),
				res.record->locationCode(), proc->streamConfig((WaveformProcessor::Component)res.component).code(), ""
			)
		);
	}
	else {
		amp->setWaveformID(
			WaveformStreamID(
				res.record->networkCode(), res.record->stationCode(),
				res.record->locationCode(), res.record->channelCode().substr(0,2), ""
			)
		);
	}

	amp->setPickID(proc->referencingPickID());

	Time now = Time::UTC();
	ci.setAgencyID(agencyID());
	ci.setAuthor(author());
	ci.setCreationTime(now);
	amp->setCreationInfo(ci);

	proc->finalizeAmplitude(amp.get());

	logObject(_outputAmps, now);

	return amp;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AmpTool::emitAmplitude(const AmplitudeProcessor *proc,
                            const AmplitudeProcessor::Result &res) {
	if ( _dumpRecords && !_originID.empty() )
		proc->writeData();

	AmplitudePtr amp = createAmplitude(proc, res);
	if ( !amp ) return;

	ProcAmpReuseMap::iterator it = _ampIDReuse.find(proc);

	if ( connection() && !_testMode ) {
		NotifierMessagePtr nmsg = new NotifierMessage;

		if ( it != _ampIDReuse.end() ) {
			bool changed = it->second->amplitude().value() != amp->amplitude().value();
			Core::Time ct_new, ct_old;

			try {
				ct_new = amp->creationInfo().creationTime();
				ct_old = it->second->creationInfo().modificationTime();
			}
			catch ( ... ) {}

			*it->second = *amp;

			try {
				it->second->creationInfo().setCreationTime(ct_old);
				it->second->creationInfo().setModificationTime(ct_new);
			}
			catch ( ... ) {}

			if ( changed )
				nmsg->attach(new Notifier("EventParameters", OP_UPDATE, it->second.get()));
		}
		else
			nmsg->attach(new Notifier("EventParameters", OP_ADD, amp.get()));

		if ( nmsg && !nmsg->empty() ) {
			connection()->send(nmsg.get());
		}
	}
	else if ( !_originID.empty() || _testMode ) {
		cerr << *amp << endl;
	}
	else if ( _ep ) {
		if ( it == _ampIDReuse.end() ) {
			_ep->add(amp.get());
			cerr << "+ " << amp->publicID() << "  " << amp->type() << endl;
		}
		else {
			cerr << "U " << it->second->publicID() << "  " << it->second->type()
			     << "  " << it->second->amplitude().value() << " -> " << amp->amplitude().value()
			     << endl;

			Core::Time ct_new, ct_old;

			try {
				ct_new = amp->creationInfo().creationTime();
				ct_old = it->second->creationInfo().modificationTime();
			}
			catch ( ... ) {}

			*it->second = *amp;

			try {
				it->second->creationInfo().setCreationTime(ct_old);
				it->second->creationInfo().setModificationTime(ct_new);
			}
			catch ( ... ) {}
		}
	}

	// Store the amplitude for pickID
	if ( it == _ampIDReuse.end() )
		feed(amp.get());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AmpTool::storeLocalAmplitude(const Seiscomp::Processing::AmplitudeProcessor *proc,
                                  const Seiscomp::Processing::AmplitudeProcessor::Result &res) {
	AmplitudePtr amp = createAmplitude(proc, res);
	if ( !amp ) return;

	_reprocessMap[amp->type()] = amp;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool AmpTool::storeRecord(Record *rec) {
	if ( _firstRecord ) {
		SEISCOMP_INFO("Data request: got first record, set timeout to %f seconds",
		              _runningAcquisitionTimeout);
		_noDataTimer.restart();
		_acquisitionTimeout = _runningAcquisitionTimeout;
		_firstRecord = false;
	}

	// This flag is resetted by handleTimeout each second
	_hasRecordsReceived = true;
	handleRecord(rec);
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AmpTool::handleRecord(Record *rec) {
	Seiscomp::RecordPtr tmp(rec);

	std::string streamID = rec->streamID();

	ProcessorMap::iterator slot_it = _processors.find(streamID);
	if ( slot_it == _processors.end() ) return;

	for ( ProcessorSlot::iterator it = slot_it->second.begin(); it != slot_it->second.end(); ) {
		(*it)->feed(rec);
		if ( (*it)->status() == WaveformProcessor::InProgress ) {
			// processor still needs some time (progress = (*it)->statusValue())
			++it;
		}
		else if ( (*it)->status() == WaveformProcessor::Finished ) {
			_result << "   + " << (*it)->type() << ", " << slot_it->first.c_str() << std::endl;
			if ( (*it)->noiseOffset() )
				_result << "     + noiseOffset = " << *(*it)->noiseOffset() << std::endl;
			else
				_result << "     - noiseOffset" << std::endl;

			if ( (*it)->noiseAmplitude() )
				_result << "     + noiseAmplitude = " << *(*it)->noiseAmplitude() << std::endl;
			else
				_result << "     - noiseAmplitude" << std::endl;
			// processor finished successfully
			it = slot_it->second.erase(it);
		}
		else if ( (*it)->isFinished() ) {
			_result << "   - " << (*it)->type() << ", " << slot_it->first.c_str() << " ("
			        << (*it)->status().toString()
			        << ")" << std::endl;
			createDummyAmplitude(it->get());
			it = slot_it->second.erase(it);
		}
		else
			++it;
	}

	if ( slot_it->second.empty() )
		_processors.erase(slot_it);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AmpTool::handleTimeout() {
	//SEISCOMP_INFO("[data acquisition monitor] checking timeouts");
	// Check for data acquisition timeout
	if ( !_hasRecordsReceived ) {
		if ( _noDataTimer.elapsed().seconds() >= _acquisitionTimeout ) {
			boost::mutex::scoped_lock l(_acquisitionMutex);
			if ( recordStream() ) {
				SEISCOMP_INFO("[data acquisition monitor] timeout reached: closing stream");
				recordStream()->close();
			}
		}
	}
	else
		_noDataTimer.restart();

	_hasRecordsReceived = false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AmpTool::acquisitionFinished() {
	_timer.stop();
	SEISCOMP_INFO("Stopped timeout monitor");

	{
		boost::mutex::scoped_lock l(_acquisitionMutex);
		closeStream();
	}

	SEISCOMP_INFO("Closed stream");

	_report << " + Data request: finished" << std::endl;

	for ( ProcessorMap::iterator slot_it = _processors.begin();
	      slot_it != _processors.end(); ++slot_it ) {
		for ( ProcessorSlot::iterator it = slot_it->second.begin();
		      it != slot_it->second.end(); ++it ) {
			_result << "   - " << (*it)->type() << ", " << slot_it->first.c_str()
			        << " (" << (*it)->status().toString()
			        << ", " << (*it)->statusValue() << "%)" << std::endl;

			_result << "     - TimeWindow: " << (*it)->safetyTimeWindow().startTime().toString("%F %T") << ", "
			        << (*it)->safetyTimeWindow().endTime().toString("%F %T") << std::endl;

			(*it)->close();
		}
	}

	_processors.clear();

	double seconds = (double)_acquisitionTimer.elapsed();
	SEISCOMP_INFO("Acquisition took %.2f seconds", seconds);

	printReport();

	_report.str(std::string());
	_result.str(std::string());
}
