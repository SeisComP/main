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




#define SEISCOMP_COMPONENT Autoloc
#include <seiscomp/logging/log.h>
#include <seiscomp/client/inventory.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/journaling_package.h>
#include <seiscomp/datamodel/utils.h>
#include <seiscomp/utils/files.h>
#include <seiscomp/config/config.h>
#include <seiscomp/core/datamessage.h>
#include <seiscomp/io/archive/xmlarchive.h>
#include <algorithm>
#include <list>

#include "app.h"
#include "datamodel.h"
#include "sc3adapters.h"
#include "scutil.h"
#include "util.h"


using namespace Seiscomp::Client;
using namespace Seiscomp::Math;


namespace Seiscomp {

namespace Applications {

namespace Autoloc {



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
App::App(int argc, char **argv)
: Application(argc, argv), Autoloc3()
, objectCount(0)
, _inputPicks(nullptr)
, _inputAmps(nullptr)
, _inputOrgs(nullptr)
, _outputOrgs(nullptr)
{
	setMessagingEnabled(true);

	setPrimaryMessagingGroup("LOCATION");

	addMessagingSubscription("PICK");
	addMessagingSubscription("AMPLITUDE");
	addMessagingSubscription("LOCATION");

	_config = Autoloc3::config();

	_keepEventsTimeSpan = 86400; // one day
	_wakeUpTimout = 5; // wake up every 5 seconds to check pending operations

	_playbackSpeed = 1;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::printUsage() const {
	std::cout << "Usage:"  << std::endl << "  scautoloc [options]" << std::endl << std::endl
	     << "Associator of P-phase picks for locating seismic events." << std::endl;

	Seiscomp::Client::Application::printUsage();

	std::cout << "Examples:" << std::endl;
	std::cout << "Real-time processing with informative debug output." << std::endl
	     << "  scautoloc --debug" << std::endl;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::createCommandLineDescription() {
	Client::Application::createCommandLineDescription();

	commandline().addGroup("Mode");
	commandline().addOption("Mode", "test", "Do not send any object.");
	commandline().addOption("Mode", "offline",
	                        "Do not connect to a messaging server. Instead a "
	                        "station-locations.conf file can be provided. This "
	                        "implies --test and --playback.");
	commandline().addOption("Mode", "playback",
	                        "Flush origins immediately without delay.");
	commandline().addOption("Mode", "xml-playback", "TODO"); // TODO
	commandline().addGroup("Input");
	commandline().addOption("Input", "input,i",
	                        "Name of XML input file for --xml-playback.",
	                        &_inputFileXML, false);
	commandline().addOption("Input", "ep",
	                        "Name of input XML file (SCML) with all picks and"
	                        "origins for offline processing. The database"
	                        "connection is not received from messaging and must"
	                        "be provided. Results are sent in XML to stdout." ,
	                        &_inputEPFile, false);

	commandline().addGroup("Settings");
	commandline().addOption("Settings", "allow-rejected-picks",
	                        "Allow picks with evaluation status 'rejected' for"
	                        "nucleation and association.");
	commandline().addOption("Settings", "station-locations",
	                        "The station-locations.conf file to use when in"
	                        "offline mode. If no file is given the database is used.",
	                        &_stationLocationFile, false);
	commandline().addOption("Settings", "station-config",
	                        "The station configuration file.",
	                        &_config.staConfFile, false);
	commandline().addOption("Settings", "grid", "The grid configuration file.",
	                        &_gridConfigFile, false);
	commandline().addOption("Settings", "pick-log",
	                        "The pick log file. Providing a file name enables "
	                        "logging picks even when disabled by configuration.",
	                        &_config.pickLogFile, false);

	commandline().addOption("Settings", "default-depth",
	                        "Default depth for locating",
	                        &_config.defaultDepth);
	commandline().addOption("Settings", "default-depth-stickiness",
	                        "",
	                        &_config.defaultDepthStickiness);
	commandline().addOption("Settings", "max-sgap",
	                        "Maximum secondary azimuthal gap.",
	                        &_config.maxAziGapSecondary);
	commandline().addOption("Settings", "max-rms", "Maximum RMS residual."
	                        "to be considered", &_config.maxRMS);
	commandline().addOption("Settings", "max-residual", "Maximum travel-time residual"
	                        " per station to be considered.",
	                        &_config.maxResidualUse);
	commandline().addOption("Settings", "max-station-distance",
	                        "Maximum distance of stations to be used.",
	                        &_config.maxStaDist);
	commandline().addOption("Settings", "max-nucleation-distance-default",
	                        "Default maximum distance of stations to be used for"
	                        " nucleating new origins.",
	                        &_config.defaultMaxNucDist);
	commandline().addOption("Settings", "min-pick-affinity", "",
	                        &_config.minPickAffinity);

	commandline().addOption("Settings", "min-phase-count",
	                        "Minimum number of picks for an origin to be reported.",
	                        &_config.minPhaseCount);
	commandline().addOption("Settings", "min-score",
	                        "Minimum score for an origin to be reported.",
	                        &_config.minScore);
	commandline().addOption("Settings", "min-pick-snr",
	                        "Minimum SNR for a pick to be processed.",
	                        &_config.minPickSNR);

	commandline().addOption("Settings", "xxl-enable", "", &_config.xxlEnabled);
	commandline().addOption("Settings", "xxl-min-phase-count",
	                        "Minimum number of picks for an XXL origin to be reported.",
	                        &_config.xxlMinPhaseCount);
	commandline().addOption("Settings", "xxl-min-amplitude",
	                        "Flag pick as XXL if BOTH snr and amplitude exceed "
	                        "a threshold.", &_config.xxlMinAmplitude);
	commandline().addOption("Settings", "xxl-min-snr",
	                        "Flag pick as XXL if BOTH snr and amplitude exceed "
	                        "a threshold.", &_config.xxlMinSNR);
	commandline().addOption("Settings", "xxl-max-distance", "",
	                        &_config.xxlMaxStaDist);
	commandline().addOption("Settings", "xxl-max-depth", "",
	                        &_config.xxlMaxDepth);
	commandline().addOption("Settings", "xxl-dead-time", "",
	                        &_config.xxlDeadTime);

	commandline().addOption("Settings", "min-sta-count-ignore-pkp",
	                        "Minimum station count for which we ignore PKP phases.",
	                        &_config.minStaCountIgnorePKP);
	commandline().addOption("Settings", "min-score-bypass-nucleator",
	                        "Minimum score at which the nucleator is bypassed.",
	                        &_config.minScoreBypassNucleator);

	commandline().addOption("Settings", "keep-events-timespan",
	                        "The timespan to keep historical events.",
	                        &_keepEventsTimeSpan);

	commandline().addOption("Settings", "cleanup-interval",
	                        "The object cleanup interval in seconds.",
	                        &_config.cleanupInterval);
	commandline().addOption("Settings", "max-age",
	                        "During cleanup all objects older than maxAge (in "
	                        "seconds) are removed (maxAge == 0 disables cleanup).",
	                        &_config.maxAge);

	commandline().addOption("Settings", "wakeup-interval",
	                        "The interval in seconds to check pending operations.",
	                        &_wakeUpTimout);
	commandline().addOption("Settings", "speed",
	                        "Set this to greater 1 to increase XML playback speed.",
	                        &_playbackSpeed);
	commandline().addOption("Settings", "dynamic-pick-threshold-interval",
	                        "The interval in seconds in which to check for "
	                        "extraordinarily high pick activity, resulting in a "
	                        "dynamically increased pick threshold.",
	                        &_config.dynamicPickThresholdInterval);

	commandline().addOption("Settings", "use-manual-picks",
	                        "Allow using manual picks for nucleation and association.");
	commandline().addOption("Settings", "use-manual-origins",
	                        "Allow using manual origins from our own agency.");
	commandline().addOption("Settings", "use-imported-origins",
	                        "Allow useing imported origins from trusted agencies "
	                        "as configured in 'processing.whitelist.agencies'. "
	                        "Imported origins are not relocated and only used "
	                        "for phase association.");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::validateParameters() {
	if ( !Client::Application::validateParameters() ) {
		return false;
	}

	if ( commandline().hasOption("offline") ) {
		_config.offline = true;
		_config.playback = true;
		_config.test = true;
	}
	else {
		_config.offline = false;
	}

	if ( !_inputEPFile.empty() ) {
		_config.playback = true;
		_config.offline = true;
	}


	if ( _config.offline ) {
		setMessagingEnabled(false);
		if ( !_stationLocationFile.empty() )
			setDatabaseEnabled(false, false);
	}

	// Load inventory from database only if no station location file was specified.
	if ( !_stationLocationFile.empty() ) {
		setLoadStationsEnabled(false);
		setDatabaseEnabled(false, false);
	}
	else {
		setLoadStationsEnabled(true);

		if ( !isInventoryDatabaseEnabled() )
			setDatabaseEnabled(false, false);
		else
			setDatabaseEnabled(true, true);
	}

	// Maybe we do want to allow sending of origins in offline mode?
	if ( commandline().hasOption("test") )
		_config.test = true;

	if ( commandline().hasOption("playback") )
		_config.playback = true;

	if ( commandline().hasOption("use-manual-picks") )
		_config.useManualPicks = true;

	if ( commandline().hasOption("use-manual-origins") )
		_config.useManualOrigins = true;

	_config.allowRejectedPicks = commandline().hasOption("allow-rejected-picks");

	if ( commandline().hasOption("use-imported-origins") )
		_config.useImportedOrigins = true;

	if ( commandline().hasOption("try-default-depth") )
		_config.tryDefaultDepth = true;

	if ( commandline().hasOption("adopt-manual-depth") ) {
		_config.adoptManualDepth = true;
	}

	if ( !_config.pickLogFile.empty() ) {
		_config.pickLogEnable = true;
	}

	// derived parameter
	_config.maxResidualKeep = 3 * _config.maxResidualUse;

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::initConfiguration() {
	if ( !Client::Application::initConfiguration() ) return false;

	// support deprecated configuration, deprecated since 2020-11-16
	try {
		_config.maxAge = configGetDouble("autoloc.maxAge");
		SEISCOMP_ERROR("Configuration parameter autoloc.maxAge is deprecated."
		               " Use buffer.pickKeep instead!");
	}
	catch (...) {}
	// override deprecated configuration if value is set
	try { _config.maxAge = configGetDouble("buffer.pickKeep"); }
	catch (...) {}

	try {
		_config.authors = configGetStrings("autoloc.authors");
	}
	catch (...) {}


	// support deprecated configuration, deprecated since 2020-11-16
	try {
		_keepEventsTimeSpan = configGetInt("keepEventsTimeSpan");
		SEISCOMP_ERROR("Configuration parameter keepEventsTimeSpan is deprecated."
		               " Use buffer.originKeep instead!");
	}
	catch ( ... ) {}
	// override deprecated configuration if value is set
	try { _keepEventsTimeSpan = configGetInt("buffer.originKeep"); }
	catch ( ... ) {}


	// support deprecated configuration, deprecated since 2020-11-16
	try { _config.cleanupInterval = configGetDouble("autoloc.cleanupInterval");
		SEISCOMP_ERROR("Configuration parameter autoloc.cleanupInterval is deprecated."
		               " Use buffer.cleanupIntervalinstead!");}
	catch (...) {}
	try { _config.cleanupInterval = configGetDouble("buffer.cleanupInterval"); }
	catch (...) {}

	try { _config.defaultDepth = configGetDouble("locator.defaultDepth"); }
	catch (...) {}

	try { _config.defaultDepthStickiness = configGetDouble("autoloc.defaultDepthStickiness"); }
	catch (...) {}

	try { _config.tryDefaultDepth = configGetBool("autoloc.tryDefaultDepth"); }
	catch (...) {}

	try { _config.adoptManualDepth = configGetBool("autoloc.adoptManualDepth"); }
	catch (...) {}

	try { _config.minimumDepth = configGetDouble("locator.minimumDepth"); }
	catch (...) {}

	try { _config.maxAziGapSecondary = configGetDouble("autoloc.maxSGAP"); }
	catch (...) {}

	try { _config.maxRMS = configGetDouble("autoloc.maxRMS"); }
	catch (...) {}

	try { _config.maxDepth = configGetDouble("autoloc.maxDepth"); }
	catch (...) {}

	try { _config.maxResidualUse = configGetDouble("autoloc.maxResidual"); }
	catch (...) {}

	try { _config.maxStaDist = configGetDouble("autoloc.maxStationDistance"); }
	catch (...) {}

	try { _config.defaultMaxNucDist = configGetDouble("autoloc.defaultMaxNucleationDistance"); }
	catch (...) {}

	try { _config.xxlEnabled = configGetBool("autoloc.xxl.enable"); }
	catch (...) {}

	try { _config.xxlMinAmplitude = configGetDouble("autoloc.xxl.minAmplitude"); }
	catch (...) {
		try {
			// deprecated since 2013-06-26
			_config.xxlMinAmplitude = configGetDouble("autoloc.thresholdXXL");
			SEISCOMP_ERROR("Config parameter autoloc.thresholdXXL is deprecated.  Use autoloc.xxl.minAmplitude instead!");
		}
		catch (...) {}
	}

	try { _config.xxlMaxStaDist = configGetDouble("autoloc.xxl.maxStationDistance"); }
	catch (...) {
		try {
			// deprecated since 2013-06-26
			_config.xxlMaxStaDist = configGetDouble("autoloc.maxStationDistanceXXL");
			SEISCOMP_ERROR("Configuration parameter autoloc.maxStationDistanceXXL"
			               " is deprecated. Use autoloc.xxl.maxStationDistance instead!");
		}
		catch (...) {}
	}

	try { _config.xxlMinPhaseCount = configGetInt("autoloc.xxl.minPhaseCount"); }
	catch (...) {
		try {
			// deprecated since 2013-06-26
			_config.xxlMinPhaseCount = configGetInt("autoloc.minPhaseCountXXL");
			SEISCOMP_ERROR("Configuration parameter autoloc.minPhaseCountXXL is"
			               " deprecated. Use autoloc.xxl.minPhaseCount instead!");
		}
		catch (...) {}
	}

	try { _config.xxlMinSNR = configGetDouble("autoloc.xxl.minSNR"); }
	catch (...) {}

	try { _config.xxlMaxDepth = configGetDouble("autoloc.xxl.maxDepth"); }
	catch (...) {}

	try { _config.xxlDeadTime = configGetDouble("autoloc.xxl.deadTime"); }
	catch (...) {}

	try { _config.minPickSNR = configGetDouble("autoloc.minPickSNR"); }
	catch (...) {}

	try { _config.minPickAffinity = configGetDouble("autoloc.minPickAffinity"); }
	catch (...) {}

	try { _config.minPhaseCount = configGetInt("autoloc.minPhaseCount"); }
	catch (...) {}

	try { _config.minScore = configGetDouble("autoloc.minScore"); }
	catch (...) {}

	try { _config.minScoreBypassNucleator = configGetDouble("autoloc.minScoreBypassNucleator"); }
	catch (...) {}

	try { _config.minStaCountIgnorePKP = configGetInt("autoloc.minStaCountIgnorePKP"); }
	catch (...) {}

	try { _config.reportAllPhases = configGetBool("autoloc.reportAllPhases"); }
	catch (...) {}

	try { _config.useManualPicks = configGetBool("autoloc.useManualPicks"); }
	catch (...) {
		_config.useManualPicks = false;
	}

	try { _config.useManualOrigins = configGetBool("autoloc.useManualOrigins"); }
	catch (...) {}

	try { _config.useImportedOrigins = configGetBool("autoloc.useImportedOrigins"); }
	catch (...) {}

	try { _wakeUpTimout = configGetInt("autoloc.wakeupInterval"); }
	catch (...) {}

	try { _config.maxRadiusFactor = configGetDouble("autoloc.gridsearch._maxRadiusFactor"); }
	catch (...) {}

	try { _config.publicationIntervalTimeSlope = configGetDouble("autoloc.publicationIntervalTimeSlope"); }
	catch ( ... ) {}

	try { _config.publicationIntervalTimeIntercept = configGetDouble("autoloc.publicationIntervalTimeIntercept"); }
	catch ( ... ) {}

	try { _config.publicationIntervalPickCount = configGetInt("autoloc.publicationIntervalPickCount"); }
	catch ( ... ) {}

	try { _config.dynamicPickThresholdInterval = configGetDouble("autoloc.dynamicPickThresholdInterval"); }
	catch ( ... ) {}

	try { _gridConfigFile = Environment::Instance()->absolutePath(configGetString("autoloc.grid")); }
	catch (...) { _gridConfigFile = Environment::Instance()->shareDir() + "/scautoloc/grid.conf"; }

	try { _config.staConfFile = Environment::Instance()->absolutePath(configGetString("autoloc.stationConfig")); }
	catch (...) { _config.staConfFile = Environment::Instance()->shareDir() + "/scautoloc/station.conf"; }

	try { _config.pickLogFile = configGetString("autoloc.pickLog"); }
	catch (...) { _config.pickLogFile = ""; }

	try { _config.pickLogEnable = configGetBool("autoloc.pickLogEnable"); }
	catch (...) { _config.pickLogEnable = false; }
	if ( !_config.pickLogEnable ) {
		_config.pickLogFile = "";
	}

	try { _amplTypeSNR = configGetString("autoloc.amplTypeSNR"); }
	catch (...) {}

	try { _amplTypeAbs = configGetString("autoloc.amplTypeAbs"); }
	catch (...) {}

	try { _stationLocationFile = configGetString("autoloc.stationLocations"); }
	catch (...) {}

	// support deprecated configuration, deprecated since 2020-11-13
	try {
		_config.locatorProfile = configGetString("autoloc.locator.profile");
		SEISCOMP_ERROR("Configuration parameter autoloc.locator.profile is deprecated."
		                 " Use locator.profile instead!");
	}
	catch (...) {}

	// override deprecated configuration if value is set
	try { _config.locatorProfile = configGetString("locator.profile"); }
	catch (...) {}

	try { _config.playback = configGetBool("autoloc.playback"); }
	catch ( ... ) {}

	try { _config.test = configGetBool("autoloc.test"); }
	catch ( ... ) {}

	_config.pickLogFile = Environment::Instance()->absolutePath(_config.pickLogFile);
	_stationLocationFile = Environment::Instance()->absolutePath(_stationLocationFile);

	// network type
	std::string ntp = "global";
	try { ntp = configGetString("autoloc.networkType"); }
	catch ( ... ) {}
	if ( ntp == "global" ) {
		_config.networkType = ::Autoloc::GlobalNetwork;
	}
	else if ( ntp == "regional" ) {
		_config.networkType = ::Autoloc::RegionalNetwork;
	}
	else if ( ntp == "local" ) {
		_config.networkType = ::Autoloc::LocalNetwork;
	}
	else {
		SEISCOMP_ERROR("Illegal value %s for autoloc.networkType", ntp.c_str());
		return false;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::init() {
	if ( ! Client::Application::init() ) {
		return false;
	}

	_inputPicks = addInputObjectLog("pick");
	_inputAmps = addInputObjectLog("amplitude");
	_inputOrgs = addInputObjectLog("origin");
	_outputOrgs = addOutputObjectLog("origin", primaryMessagingGroup());

	// This is the SeisComP configuration, which we need to pass through
	// all the way to the locator.
	_config.scconfig = &configuration();

	SEISCOMP_INFO("Starting Autoloc");
	setConfig(_config);
	dumpConfig();
	if ( !setGridFile(_gridConfigFile) ) {
		return false;
	}

	if ( !initInventory() ) {
		return false;
	}

	if ( !_config.pickLogFile.empty() ) {
		setPickLogFilePrefix(_config.pickLogFile);
	}

	if ( _config.playback ) {
		if ( _inputEPFile.empty() ) {
			// XML playback, set timer to 1 sec
			enableTimer(1);
		}
	}
	else {
// TEMP		readHistoricEvents();
		if ( _wakeUpTimout > 0 ) {
			enableTimer(_wakeUpTimout);
		}
	}

	return Autoloc3::init();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::initInventory() {
	if ( _stationLocationFile.empty() ) {
		SEISCOMP_DEBUG("Initializing station inventory from DB");
		inventory = Inventory::Instance()->inventory();
		if ( ! inventory ) {
			SEISCOMP_ERROR("no inventory!");
			return false;
		}

		// Remove unneeded inventory items to save some memory
		for ( size_t n = 0; n < inventory->networkCount(); ++n ) {
			DataModel::Network *network = inventory->network(n);

			for ( size_t s = 0; s < network->stationCount(); ++s ) {
				DataModel::Station *station = network->station(s);

				for ( size_t l = 0; l < station->sensorLocationCount(); ++l ) {
					DataModel::SensorLocation *sensorLocation = station->sensorLocation(l);
					while (sensorLocation->streamCount())
						sensorLocation->removeStream(0);
					while (sensorLocation->auxStreamCount())
						sensorLocation->removeAuxStream(0);
					while (sensorLocation->commentCount())
						sensorLocation->removeComment(0);
				}
			}
		}
	}
	else {
		SEISCOMP_DEBUG_S("Initializing station inventory from file '" + _stationLocationFile + "'");
		inventory = ::Autoloc::Utils::inventoryFromStationLocationFile(_stationLocationFile);
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::initOneStation(const DataModel::WaveformStreamID &wfid, const Core::Time &time) {
	bool found {false};
	static std::set<std::string> configuredStreams;
	std::string key = wfid.networkCode() + "." + wfid.stationCode();

	if ( configuredStreams.find(key) != configuredStreams.end() )
		return false;

	for ( size_t n = 0; n < inventory->networkCount() && !found; ++n ) {
		DataModel::Network *network = inventory->network(n);

		if ( network->code() != wfid.networkCode() )
			continue;

		try {
			if ( time < network->start() )
				continue;
		}
		catch ( ... ) { }

		try {
			if ( time > network->end() )
				continue;
		}
		catch ( ... ) { }

		for ( size_t s = 0; s < network->stationCount(); ++s ) {
			DataModel::Station *station = network->station(s);

			if (station->code() != wfid.stationCode())
				continue;

			std::string epochStart="unset", epochEnd="unset";

			try {
				if (time < station->start())
					continue;
				epochStart = station->start().toString("%FT%TZ");
			}
			catch ( ... ) { }

			try {
				if (time > station->end())
					continue;
				epochEnd = station->end().toString("%FT%TZ");
			}
			catch ( ... ) { }

			SEISCOMP_DEBUG("Station %s %s epoch %s ... %s",
			               network->code().c_str(),
			               station->code().c_str(),
			               epochStart.c_str(),
			               epochEnd.c_str());

			double elevation = 0;
			try { elevation = station->elevation(); }
			catch ( ... ) {}
			::Autoloc::Station *sta =
				new ::Autoloc::Station(
					station->code(),
					network->code(),
					station->latitude(),
					station->longitude(),
					elevation);

			sta->used = true;
			sta->maxNucDist = _config.defaultMaxNucDist;

			setStation(sta);
			found = true;

			break;
		}
	}

	if ( !found ) {
		SEISCOMP_WARNING("%s not found in station inventory", key.c_str());
		return false;
	}

	configuredStreams.insert(key);
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::readHistoricEvents() {
	if ( _keepEventsTimeSpan <= 0 || ! query() ) return;

	SEISCOMP_DEBUG("readHistoricEvents: reading %d seconds of events", _keepEventsTimeSpan);

	typedef std::list<DataModel::OriginPtr> OriginList;
	typedef std::set<std::string> PickIds;

	// Fetch all historic events out of the database. The endtime is
	// probably in the future but because of timing differences between
	// different computers: safety first!
	Core::Time now = Core::Time::GMT();
	OriginList preferredOrigins;
	PickIds pickIds;

	// Store all preferred origins
	DataModel::DatabaseIterator it =
		query()->getPreferredOrigins(now - Core::TimeSpan(_keepEventsTimeSpan),
		                             now + Core::TimeSpan(_keepEventsTimeSpan), "");
	for ( ; it.get() != nullptr; ++it ) {
		DataModel::OriginPtr origin = DataModel::Origin::Cast(it.get());
		if ( origin )
			preferredOrigins.push_back(origin);
	}
	it.close();

	// Store all pickIDs of all origins and remove duplicates
	for (auto& origin : preferredOrigins) {
		if ( origin->arrivalCount() == 0 ) {
			query()->loadArrivals(origin.get());
			for ( size_t i = 0; i < origin->arrivalCount(); ++i )
				pickIds.insert(origin->arrival(i)->pickID());
		}

		SEISCOMP_DEBUG_S("read historical origin "+origin->publicID());
	}

	// Read picks from database
	for ( const std::string& pickID : pickIds ) {

		DataModel::ObjectPtr obj = query()->getObject(DataModel::Pick::TypeInfo(), pickID);
		if ( !obj ) {
			continue;
		}
		DataModel::PickPtr pick = DataModel::Pick::Cast(obj);
		if ( !pick ) {
			continue;
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::runFromXMLFile(const char *filename)
{
	SEISCOMP_INFO("App::runFromXMLFile");

	IO::XMLArchive ar;
	if ( ! ar.open(filename)) {
		SEISCOMP_ERROR("unable to open XML playback file '%s'", filename);
		return false;
	}

	DataModel::EventParametersPtr ep = new DataModel::EventParameters();
	ar >> ep;
	SEISCOMP_INFO("finished reading event parameters from XML");
	SEISCOMP_INFO("  number of picks:      %d", ep->pickCount());
	SEISCOMP_INFO("  number of amplitudes: %d", ep->amplitudeCount());
	SEISCOMP_INFO("  number of origins:    %d", ep->originCount());

	// Tuple to be used in DSU sorting. The second member is used to place picks
	// before amplitudes with identical creation times.
	typedef std::tuple<Core::Time, int, std::string, DataModel::PublicObjectPtr> TimeObject;
	typedef std::vector<TimeObject> TimeObjectVector;

	// retrieval of relevant objects from event parameters
	// and subsequent DSU sort
	TimeObjectVector objs;
	while (ep->pickCount() > 0) {
		DataModel::PickPtr pick = ep->pick(0);
		ep->removePick(0);
		DataModel::PublicObjectPtr o(pick);
		Core::Time t = pick->creationInfo().creationTime();
		objs.push_back(TimeObject(t, 0, pick->publicID(), o));
	}
	while (ep->amplitudeCount() > 0) {
		DataModel::AmplitudePtr amplitude = ep->amplitude(0);
		ep->removeAmplitude(0);
		DataModel::PublicObjectPtr o(amplitude);
		Core::Time t = amplitude->creationInfo().creationTime();
		// t += Core::TimeSpan(0.00001);
		objs.push_back(TimeObject(t, 1, amplitude->publicID(), o));
	}
	while (ep->originCount() > 0) {
		DataModel::OriginPtr origin = ep->origin(0);
		ep->removeOrigin(0);
		DataModel::PublicObjectPtr o(origin);
		Core::Time t = origin->creationInfo().creationTime();
		objs.push_back(TimeObject(t, 2, origin->publicID(), o));
	}
	std::sort(objs.begin(),objs.end());
	for ( TimeObject &obj : objs ) {
		_objects.push(std::get<3>(obj));
	}

	if ( _objects.empty() )
		return false;

	if ( _playbackSpeed > 0 ) {
		SEISCOMP_DEBUG("playback speed factor %g", _playbackSpeed);
	}

	objectsStartTime = playbackStartTime = Core::Time::GMT();
	objectCount = 0;

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::runFromEPFile(const char *filename) {
	SEISCOMP_INFO("App::runFromEPFile");
	IO::XMLArchive ar;
	if ( !ar.open(filename)) {
		SEISCOMP_ERROR("unable to open XML file: %s", filename);
		return false;
	}

	ar >> _ep;
	ar.close();

	if ( !_ep ) {
		SEISCOMP_ERROR("No event parameters found: %s", filename);
		return false;
	}

	SEISCOMP_INFO("finished reading event parameters from XML");
	SEISCOMP_INFO("  number of picks:      %d", _ep->pickCount());
	SEISCOMP_INFO("  number of amplitudes: %d", _ep->amplitudeCount());
	SEISCOMP_INFO("  number of origins:    %d", _ep->originCount());

	std::cerr << "Read from file: " << _ep->originCount() << " origin(s), "
	     << _ep->pickCount() << " pick(s), "
	     << _ep->amplitudeCount() << " amplitudes(s)"<< std::endl;

	// Tuple to be used in DSU sorting. The second member is used to place picks
	// before amplitudes with identical creation times.
	typedef std::tuple<Core::Time, int, std::string, DataModel::PublicObjectPtr> TimeObject;
	typedef std::vector<TimeObject> TimeObjectVector;

	// retrieval of relevant objects from event parameters
	// and subsequent DSU sort
	TimeObjectVector objs;

	for ( size_t i = 0; i < _ep->pickCount(); ++i ) {
		DataModel::PickPtr pick = _ep->pick(i);
		bool add = true;
		try {
			if ( pick->evaluationStatus() == DataModel::REJECTED ) {
				if ( !_config.allowRejectedPicks ) {
					add = false;
					SEISCOMP_DEBUG("Ignoring pick %s with evaluation status %s",
					               pick->publicID().c_str(), pick->evaluationStatus().toString());
					continue;
				}
				else {
					SEISCOMP_DEBUG("Considering pick %s with evaluation status %s",
					               pick->publicID().c_str(), pick->evaluationStatus().toString());
				}
			}
		}
		catch ( ... ) {}

		Core::Time t;
		try {
			t = pick->creationInfo().creationTime();
		}
		catch ( ... ) {
			add = false;
			SEISCOMP_WARNING("Ignore pick %s: no creation time set",
			                 pick->publicID().c_str());
			continue;
		}

		if ( add ) {
			objs.push_back(TimeObject(t, 0, pick->publicID(), pick));
		}
	}

	for ( size_t i = 0; i < _ep->amplitudeCount(); ++i ) {
		DataModel::AmplitudePtr amplitude = _ep->amplitude(i);
		try {
			Core::Time t = amplitude->creationInfo().creationTime();
			// t += Core::TimeSpan(0.00001);
			objs.push_back(TimeObject(t, 1, amplitude->publicID(), amplitude));
		}
		catch ( ... ) {
			SEISCOMP_WARNING("Ignore amplitude %s: no creation time set",
			                 amplitude->publicID().c_str());
		}
	}

	for ( size_t i = 0; i < _ep->originCount(); ++i ) {
		DataModel::OriginPtr origin = _ep->origin(i);
		try {
			Core::Time t = origin->creationInfo().creationTime();
			objs.push_back(TimeObject(t, 2, origin->publicID(), origin));
		}
		catch ( ... ) {
			SEISCOMP_WARNING("Ignore origin %s: no creation time set",
			                 origin->publicID().c_str());
		}
	}

	std::sort(objs.begin(), objs.end());
	for ( TimeObject &obj : objs ) {
		_objects.push(std::get<3>(obj));
	}

	while ( !_objects.empty() && !isExitRequested() ) {
		DataModel::PublicObjectPtr o = _objects.front();

		_objects.pop();
		addObject("", o.get());
		++objectCount;
	}

	_flush();


	ar.create("-");
	ar.setFormattedOutput(true);
	ar << _ep;
	ar.close();

	std::cerr << "Output to XML: " << objectCount << " objects(s)" << std::endl;

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::sync(const Seiscomp::Core::Time &t) {
	syncTime = t;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const Seiscomp::Core::Time App::now() const {

	if (_inputFileXML.size() || _inputEPFile.size())
		return syncTime;

	return Core::Time::GMT();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::timeStamp() const {
	SEISCOMP_DEBUG_S("Timestamp: "+now().toString("%F %T.%f"));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::run() {
	if ( !_inputEPFile.empty() )
		return runFromEPFile(_inputEPFile.c_str());

	// normal online mode
	if ( ! Autoloc3::config().offline )
		return Application::run();

	// XML playback: first fill object queue, then run()
	if ( _config.playback && _inputFileXML.size() > 0) {
		runFromXMLFile(_inputFileXML.c_str());
		return Application::run();
	}

/*
	// OBSOLETE not that the XML playback is available
	else if ( ! _exitRequested )
		runFromPickFile(); // pick file read from stdin
*/

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::done() {
	_exitRequested = true;
	shutdown();

	// final log of public object counts
	logObjectCounts();

	Application::done();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::handleMessage(Core::Message* msg) {
	// Call the original method to make sure that the
	// interpret callbacks (addObject, updateObject -> see below)
	// will be called
	Application::handleMessage(msg);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::handleTimeout() {

	if ( !_config.playback || _inputFileXML.empty() ) {
		_flush();
		return;
	}


	// The following is relevant (and executed) only for XML playback.

	while ( ! _objects.empty() && !isExitRequested() ) {

		Core::Time t;
		DataModel::PublicObjectPtr o = _objects.front();

		// retrieve the creationTime...
		if (DataModel::Pick::Cast(o.get()))
			t = DataModel::Pick::Cast(o.get())->creationInfo().creationTime();
		else if (DataModel::Amplitude::Cast(o.get()))
			t = DataModel::Amplitude::Cast(o.get())->creationInfo().creationTime();
		else if (DataModel::Origin::Cast(o.get()))
			t = DataModel::Origin::Cast(o.get())->creationInfo().creationTime();
		else continue;

		// at the first object:
		if (objectCount == 0)
			objectsStartTime = t;

		if (_playbackSpeed > 0) {
			double dt = t - objectsStartTime;
			Core::TimeSpan dp = dt/_playbackSpeed;
			t = playbackStartTime + dp;
			if (Core::Time::GMT() < t)
				break; // until next handleTimeout() call
		} // otherwise no speed limit :)

		_objects.pop();
		addObject("", o.get());
		objectCount++;
	}

	// for an XML playback, we're done once the object queue is empty
	if ( _objects.empty() )
		quit();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::handleAutoShutdown() {
//	_flush();
	Client::Application::handleAutoShutdown();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::addObject(const std::string& parentID, DataModel::Object* o) {
	DataModel::PublicObject *publicObject = DataModel::PublicObject::Cast(o);
	if ( !publicObject ) {
		return;
	}

	bool extra_debug = false;

	DataModel::Pick *pick = DataModel::Pick::Cast(o);
	if ( pick ) {
		logObject(_inputPicks, Core::Time::GMT());
		if ( ! feed(pick))
			return;
		if (extra_debug)
			logObjectCounts();
		return;
	}

	DataModel::Amplitude *amplitude = DataModel::Amplitude::Cast(o);
	if ( amplitude ) {
		logObject(_inputAmps, Core::Time::GMT());
		if ( ! feed(amplitude))
			return;
		if (extra_debug)
			logObjectCounts();
		return;
	}

	DataModel::Origin *origin = DataModel::Origin::Cast(o);
	if ( origin ) {
		logObject(_inputOrgs, Core::Time::GMT());
		if ( ! feed(origin))
			return;
		if (extra_debug)
			logObjectCounts();
		return;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::feed(DataModel::Pick *scpick) {

	const std::string &pickID = scpick->publicID();
	try {
		if ( scpick->evaluationStatus() == DataModel::REJECTED ) {
			if ( !_config.allowRejectedPicks ) {
				SEISCOMP_DEBUG("Ignoring pick %s with evaluation status %s",
				               scpick->publicID().c_str(),
				               scpick->evaluationStatus().toString());
				return false;
			}
			else {
				SEISCOMP_DEBUG("Considering pick %s with evaluation status %s",
				               scpick->publicID().c_str(),
				               scpick->evaluationStatus().toString());
			}
		}
	}
	catch ( ... ) {}

	if (_inputFileXML.size() || _inputEPFile.size()) {
		try {
			const Core::Time &creationTime = scpick->creationInfo().creationTime();
			sync(creationTime);
		}
		catch ( ... ) {
			SEISCOMP_WARNING_S("Pick "+pickID+": no creation time set!");
		}
	}

	if (objectAgencyID(scpick) != agencyID()) {
		if ( isAgencyIDBlocked(objectAgencyID(scpick)) ) {
			SEISCOMP_INFO_S("Blocked pick from agency " + objectAgencyID(scpick));
			return false;
		}

		SEISCOMP_INFO("Pick %s from agency %s", pickID.c_str(), objectAgencyID(scpick).c_str());

	}

	const std::string &author = objectAuthor(scpick);
	const int priority = _authorPriority(author);
	if (priority == 0) {
		SEISCOMP_INFO("Pick %s not processed: author %s is not considered",
		              pickID.c_str(), author.c_str());
		return false;
	}

	try {
		if (scpick->evaluationMode() == DataModel::MANUAL) {
		}
	}
	catch ( ... ) {
		SEISCOMP_WARNING("Pick %s: evaluation mode not set",
		                 scpick->publicID().c_str());
		scpick->setEvaluationMode(DataModel::EvaluationMode(DataModel::AUTOMATIC));
	}

	// configure station if needed
	initOneStation(scpick->waveformID(), scpick->time().value());

	::Autoloc::PickPtr pick = convertFromSC(scpick);
	if ( ! pick )
		return false;

	if ( _config.offline )
		timeStamp();

	::Autoloc::Autoloc3::feed(pick.get());

	if ( _config.offline )
		_flush();

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::feed(DataModel::Amplitude *scampl) {

	const std::string &amplID = scampl->publicID();

	if (_inputFileXML.size() || _inputEPFile.size()) {
		try {
			const Core::Time &creationTime = scampl->creationInfo().creationTime();
			sync(creationTime);
		}
		catch ( ... ) {
			SEISCOMP_WARNING("Amplitude %s: creation time not set",
			                 amplID.c_str());
		}
	}

	if (objectAgencyID(scampl) != agencyID()) {
		if ( isAgencyIDBlocked(objectAgencyID(scampl)) ) {
			SEISCOMP_INFO_S("Blocked amplitude from agency " + objectAgencyID(scampl));
			return false;
		}
		SEISCOMP_INFO("Amplitude %s from agency %s",
		              amplID.c_str(), objectAgencyID(scampl).c_str());
	}

	const std::string &atype  = scampl->type();
	const std::string &pickID = scampl->pickID();

	if ( atype != _amplTypeAbs && atype != _amplTypeSNR )
		return false;

	::Autoloc::Pick *pick = (::Autoloc::Pick *) Autoloc3::pick(pickID);
	if ( ! pick ) {
		// TODO: debug message not here
		SEISCOMP_DEBUG_S("Pick " + pickID + " not found for " + atype + " amplitude");
		return false;
	}

	try {
		// note that for testing it is allowed to use the same amplitude as
		// _amplTypeSNR and _amplTypeAbs  -> no 'else if' here
		if ( atype == _amplTypeSNR )
			pick->snr = scampl->amplitude().value();
		if ( atype == _amplTypeAbs ) {
			pick->amp = scampl->amplitude().value();
			pick->per = (_amplTypeAbs == "mb") ? scampl->period().value() : 1;
		}
	}
	catch ( ... ) {
		return false;
	}

	::Autoloc::Autoloc3::feed(pick);
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::feed(DataModel::Origin *scorigin) {

	if ( ! scorigin ) {
		SEISCOMP_ERROR("This should never happen: origin=NULL");
		return false;
	}

	SEISCOMP_INFO_S("got origin " + scorigin->publicID() +
			"   agency: " + objectAgencyID(scorigin));

	const bool ownOrigin = objectAgencyID(scorigin) == agencyID();

	if ( ownOrigin ) {
		if ( manual(scorigin) ) {
			if ( ! _config.useManualOrigins ) {
				SEISCOMP_INFO_S("Ignored origin from " + objectAgencyID(scorigin) + " because autoloc.useManualOrigins = false");
				return false;
			}
		}
		else {
			// own origin which is not manual -> ignore
			SEISCOMP_INFO_S("Ignored origin from " + objectAgencyID(scorigin) + " because not a manual origin");
			return false;
		}
	}
	else {
		// imported origin

		if ( ! _config.useImportedOrigins ) {
			SEISCOMP_INFO_S("Ignored origin from " + objectAgencyID(scorigin) + " because autoloc.useImportedOrigins = false");
			return false;
		}

		if ( isAgencyIDBlocked(objectAgencyID(scorigin)) ) {
			SEISCOMP_INFO_S("Ignored origin from " + objectAgencyID(scorigin) + " due to blocked agency ID");
			return false;
		}
	}

	// now we know that the origin is either
	//  * imported from a trusted external source or
	//  * an internal, manual origin

	// TODO: Vorher konsistente Picks/Arrivals sicher stellen.
	size_t arrivalCount = scorigin->arrivalCount();
	for ( size_t i=0; i<arrivalCount; i++ ) {
		const std::string &pickID = scorigin->arrival(i)->pickID();
		DataModel::Pick *scpick = DataModel::Pick::Find(pickID);
		if ( ! scpick) {
			SEISCOMP_ERROR_S("Pick " + pickID + " not found");
		}
	}

	::Autoloc::Origin *origin = convertFromSC(scorigin);
	if ( ! origin ) {
		SEISCOMP_ERROR_S("Failed to convert origin " + objectAgencyID(scorigin));
		return false;
	}

	// mark and log imported origin
	if ( objectAgencyID(scorigin) == agencyID() ) {
		SEISCOMP_INFO_S("Using origin from agency " + objectAgencyID(scorigin));
		origin->imported = false;
	}
	else {
		SEISCOMP_INFO_S("Using origin from agency " + objectAgencyID(scorigin));
		origin->imported = true;
	}

	::Autoloc::Autoloc3::feed(origin);

	if ( _config.offline )
		_flush();

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::_report(const ::Autoloc::Origin *origin) {

	// Log object flow
	logObject(_outputOrgs, now());

	if ( _config.offline || _config.test ) {
		std::string reportStr = ::Autoloc::printDetailed(origin);
		SEISCOMP_INFO("Reporting origin %ld\n%s", origin->id, reportStr.c_str());
		SEISCOMP_INFO ("Origin %ld not sent (test/offline mode)", origin->id);

		if ( _ep ) {
			DataModel::OriginPtr scorigin = convertToSC(origin, _config.reportAllPhases);
			DataModel::CreationInfo ci;
			ci.setAgencyID(agencyID());
			ci.setAuthor(author());
			ci.setCreationTime(now());
			scorigin->setCreationInfo(ci);

			_ep->add(scorigin.get());

			std::cerr << reportStr << std::endl;
		}
		else
			std::cout << reportStr << std::endl;

		return true;
	}

	DataModel::OriginPtr scorigin = convertToSC(origin, _config.reportAllPhases);
	DataModel::CreationInfo ci;
	ci.setAgencyID(agencyID());
	ci.setAuthor(author());
	ci.setCreationTime(now());
	scorigin->setCreationInfo(ci);

	DataModel::EventParameters ep;
	bool wasEnabled = DataModel::Notifier::IsEnabled();
	DataModel::Notifier::Enable();
	ep.add(scorigin.get());
	DataModel::Notifier::SetEnabled(wasEnabled);

	DataModel::NotifierMessagePtr nmsg = DataModel::Notifier::GetMessage(true);
	connection()->send(nmsg.get());

	if ( origin->preliminary ) {
		SEISCOMP_INFO("Sent preliminary origin %ld (heads up)", origin->id);

		// create and send journal entry
		std::string str = "";
		try {
			str = scorigin->evaluationStatus().toString();
		}
		catch ( Core::ValueException & ) {}

		if ( !str.empty() ) {
			DataModel::JournalEntryPtr journalEntry = new DataModel::JournalEntry;
			journalEntry->setAction("OrgEvalStatOK");
			journalEntry->setObjectID(scorigin->publicID());
			journalEntry->setSender(SCCoreApp->author().c_str());
			journalEntry->setParameters(str);
			journalEntry->setCreated(Core::Time::GMT());

			DataModel::NotifierMessagePtr jm = new DataModel::NotifierMessage;
			jm->attach(new DataModel::Notifier(DataModel::Journaling::ClassName(),
			           DataModel::OP_ADD, journalEntry.get()));

			if ( connection()->send(jm.get()) ) {
				SEISCOMP_DEBUG("Sent origin journal entry for origin %s to the message group: %s",
				               scorigin->publicID().c_str(), primaryMessagingGroup().c_str());
			}
			else {
				SEISCOMP_ERROR("Sending origin journal entry failed with error: %s",
				               connection()->lastError().toString());
			}
		}

	}
	else {
		SEISCOMP_INFO("Sent origin %ld", origin->id);
	}

	SEISCOMP_INFO_S(::Autoloc::printOrigin(origin, false));

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


} // namespace Autoloc

} // namespace Applications

} // namespace Seiscomp
