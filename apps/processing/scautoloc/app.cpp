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
#include "scutil.h"


namespace Seiscomp {

namespace Applications {


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
AutolocApp::AutolocApp(int argc, char **argv)
: Application(argc, argv), Processing::Autoloc()
{
	setMessagingEnabled(true);
	setPrimaryMessagingGroup("LOCATION");
	addMessagingSubscription("PICK");
	addMessagingSubscription("AMPLITUDE");
	addMessagingSubscription("LOCATION");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AutolocApp::printUsage() const {
	std::cout << "Usage:"  << std::endl << "  scautoloc [options]" << std::endl << std::endl
	     << "Associator of P-phase picks for locating seismic events." << std::endl;

	Client::Application::printUsage();

	std::cout << "Examples:" << std::endl;
	std::cout << "Real-time processing with informative debug output." << std::endl
	     << "  scautoloc --debug" << std::endl;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AutolocApp::createCommandLineDescription() {
	Client::Application::createCommandLineDescription();

	commandline().addGroup("Mode");
	commandline().addOption("Mode", "test", "Do not send any object.");
	commandline().addOption("Mode", "offline",
	                        "Do not connect to a messaging server. Instead a "
	                        "station-locations.conf file can be provided. This "
	                        "implies --test and --playback.");
	commandline().addOption("Mode", "playback",
	                        "Flush origins immediately without delay.");

	commandline().addGroup("Input");
	commandline().addOption("Input", "input,i",
	                        "Identical to --ep and provided for compatibility",
	                        &_inputFileXML, false);
	commandline().addOption("Input", "ep",
	                        "Name of input XML file (SCML) with all picks and "
	                        "origins for offline processing.  Use '-' to read "
	                        "from stdin. The database connection is not received "
	                        "from messaging and must be provided. Results are "
	                        "sent in XML to stdout." ,
	                        &_inputFileXML, false);

	commandline().addGroup("Settings");
	commandline().addOption("Settings", "allow-rejected-picks",
	                        "Allow picks with evaluation status 'rejected' for"
	                        "nucleation and association.");
	commandline().addOption("Settings", "station-locations",
	                        "The station-locations.conf file to use when in "
	                        "offline mode. If no file is given, the database is used.",
	                        &_config.stationLocationFile, false);
	commandline().addOption("Settings", "station-config",
	                        "The station configuration file.",
	                        &_config.staConfFile, false);
	commandline().addOption("Settings", "grid", "The grid configuration file.",
	                        &_config.gridConfigFile, false);
	commandline().addOption("Settings", "pick-log",
	                        "The pick log file. Providing a file name enables "
	                        "logging picks even when disabled by configuration.",
	                        &_config.pickLogPrefix, false);
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
	                        &_config.originKeep);
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

	commandline().addGroup("Output");
	commandline().addOption("Output", "formatted,f",
	                        "Use formatted XML output. Otherwise XML is unformatted.");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool AutolocApp::initConfiguration() {
	if ( !Client::Application::initConfiguration() ) {
		return false;
	}

	// Support deprecated configuration, deprecated since 2020-11-16
	try {
		_config.maxAge = configGetDouble("autoloc.maxAge");
		SEISCOMP_ERROR("Configuration parameter autoloc.maxAge is deprecated."
		               " Use buffer.pickKeep instead!");
	}
	catch ( ... ) {}

	// Override deprecated configuration if value is set
	try {
		_config.maxAge = configGetDouble("buffer.pickKeep");
	}
	catch ( ... ) {}

	try {
		_config.authors = configGetStrings("autoloc.authors");
	}
	catch ( ... ) {}

	// Support deprecated configuration, deprecated since 2020-11-16
	try {
		_config.originKeep = configGetInt("keepEventsTimeSpan");
		SEISCOMP_ERROR("Configuration parameter keepEventsTimeSpan is deprecated."
		               " Use buffer.originKeep instead!");
	}
	catch ( ... ) {}

	// Override deprecated configuration if value is set
	try {
		_config.originKeep = configGetInt("buffer.originKeep");
	}
	catch ( ... ) {}


	// Support deprecated configuration, deprecated since 2020-11-16
	try {
		_config.cleanupInterval = configGetDouble("autoloc.cleanupInterval");
		SEISCOMP_ERROR("Configuration parameter autoloc.cleanupInterval is deprecated."
		               " Use buffer.cleanupIntervalinstead!");}
	catch ( ... ) {}

	try {
		_config.cleanupInterval = configGetDouble("buffer.cleanupInterval");
	}
	catch ( ... ) {}

	try {
		_config.defaultDepthStickiness = configGetDouble("autoloc.defaultDepthStickiness");
	}
	catch ( ... ) {}

	try {
		_config.tryDefaultDepth = configGetBool("autoloc.tryDefaultDepth");
	}
	catch ( ... ) {}

	try {
		_config.adoptManualDepth = configGetBool("autoloc.adoptManualDepth");
	}
	catch ( ... ) {}

	// Support deprecated configuration, deprecated since 2020-11-13
	try {
		_config.locatorProfile = configGetString("autoloc.locator.profile");
		SEISCOMP_ERROR("Configuration parameter autoloc.locator.profile is deprecated."
		                 " Use locator.profile instead!");
	}
	catch ( ... ) {}

	// Override deprecated configuration if value is set
	try {
		_config.locatorProfile = configGetString("locator.profile");
	}
	catch ( ... ) {}

	try {
		_config.defaultDepth = configGetDouble("locator.defaultDepth");
	}
	catch ( ... ) {}

	try {
		_config.minimumDepth = configGetDouble("locator.minimumDepth");
	}
	catch ( ... ) {}

	try {
		_config.maxAziGapSecondary = configGetDouble("autoloc.maxSGAP");
	}
	catch ( ... ) {}

	try {
		_config.maxRMS = configGetDouble("autoloc.maxRMS");
	}
	catch ( ... ) {}

	try {
		_config.maxDepth = configGetDouble("autoloc.maxDepth");
	}
	catch ( ... ) {}

	try {
		_config.maxResidualUse = configGetDouble("autoloc.maxResidual");
	}
	catch ( ... ) {}

	try {
		_config.maxStaDist = configGetDouble("autoloc.maxStationDistance");
	}
	catch ( ... ) {}

	try {
		_config.defaultMaxNucDist = configGetDouble("autoloc.defaultMaxNucleationDistance");
	}
	catch ( ... ) {}

	try {
		_config.xxlEnabled = configGetBool("autoloc.xxl.enable");
	}
	catch ( ... ) {}

	try {
		_config.xxlMinAmplitude = configGetDouble("autoloc.xxl.minAmplitude");
	}
	catch ( ... ) {
		try {
			// deprecated since 2013-06-26
			_config.xxlMinAmplitude = configGetDouble("autoloc.thresholdXXL");
			SEISCOMP_ERROR("Config parameter autoloc.thresholdXXL is deprecated."
			               " Use autoloc.xxl.minAmplitude instead!");
		}
		catch ( ... ) {}
	}

	try {
		_config.xxlMaxStaDist = configGetDouble("autoloc.xxl.maxStationDistance");
		}
	catch ( ... ) {
		try {
			// deprecated since 2013-06-26
			_config.xxlMaxStaDist = configGetDouble("autoloc.maxStationDistanceXXL");
			SEISCOMP_ERROR("Configuration parameter autoloc.maxStationDistanceXXL"
			               " is deprecated. Use autoloc.xxl.maxStationDistance instead!");
		}
		catch ( ... ) {}
	}

	try {
		_config.xxlMinPhaseCount = configGetInt("autoloc.xxl.minPhaseCount");
		}
	catch ( ... ) {
		try {
			// deprecated since 2013-06-26
			_config.xxlMinPhaseCount = configGetInt("autoloc.minPhaseCountXXL");
			SEISCOMP_ERROR("Configuration parameter autoloc.minPhaseCountXXL is"
			               " deprecated. Use autoloc.xxl.minPhaseCount instead!");
		}
		catch ( ... ) {}
	}

	try {
		_config.xxlMinSNR = configGetDouble("autoloc.xxl.minSNR");
	}
	catch ( ... ) {}

	try {
		_config.xxlMaxDepth = configGetDouble("autoloc.xxl.maxDepth");
	}
	catch ( ... ) {}

	try {
		_config.xxlDeadTime = configGetDouble("autoloc.xxl.deadTime");
	}
	catch ( ... ) {}

	try {
		_config.minPickSNR = configGetDouble("autoloc.minPickSNR");
	}
	catch ( ... ) {}

	try {
		_config.minPickAffinity = configGetDouble("autoloc.minPickAffinity");
	}
	catch ( ... ) {}

	try {
		_config.minPhaseCount = configGetInt("autoloc.minPhaseCount");
	}
	catch ( ... ) {}

	try {
		_config.minScore = configGetDouble("autoloc.minScore");
	}
	catch ( ... ) {}

	try {
		_config.minScoreBypassNucleator = configGetDouble("autoloc.minScoreBypassNucleator");
	}
	catch ( ... ) {}

	try {
		_config.minStaCountIgnorePKP = configGetInt("autoloc.minStaCountIgnorePKP");
	}
	catch ( ... ) {}

	try {
		_config.reportAllPhases = configGetBool("autoloc.reportAllPhases");
	}
	catch ( ... ) {}

	try {
		_config.useManualPicks = configGetBool("autoloc.useManualPicks");
	}
	catch ( ... ) {}

	try {
		_config.useManualOrigins = configGetBool("autoloc.useManualOrigins");
	}
	catch ( ... ) {}

	try {
		_config.useImportedOrigins = configGetBool("autoloc.useImportedOrigins");
	}
	catch ( ... ) {}

	try {
		_wakeUpTimout = configGetInt("autoloc.wakeupInterval");
	}
	catch ( ... ) {}

	try {
		_config.maxRadiusFactor = configGetDouble("autoloc.gridsearch._maxRadiusFactor");
	}
	catch ( ... ) {}

	try {
		_config.publicationIntervalTimeSlope = configGetDouble("autoloc.publicationIntervalTimeSlope");
	}
	catch ( ... ) {}

	try {
		_config.publicationIntervalTimeIntercept = configGetDouble("autoloc.publicationIntervalTimeIntercept");
	}
	catch ( ... ) {}

	try {
		_config.publicationIntervalPickCount = configGetInt("autoloc.publicationIntervalPickCount");
	}
	catch ( ... ) {}

	try {
		_config.dynamicPickThresholdInterval = configGetDouble("autoloc.dynamicPickThresholdInterval");
	}
	catch ( ... ) {}

	try {
		_config.gridConfigFile = Environment::Instance()->absolutePath(configGetString("autoloc.grid"));
	}
	catch ( ... ) { _config.gridConfigFile = Environment::Instance()->shareDir() + "/scautoloc/grid.conf"; }

	try {
		_config.staConfFile = Environment::Instance()->absolutePath(configGetString("autoloc.stationConfig"));
	}
	catch ( ... ) { _config.staConfFile = Environment::Instance()->shareDir() + "/scautoloc/station.conf"; }

	try {
		_config.pickLogPrefix = configGetString("autoloc.pickLog");
	}
	catch ( ... ) { _config.pickLogPrefix = ""; }

	try {
		_config.pickLogEnable = configGetBool("autoloc.pickLogEnable");
	}
	catch ( ... ) {
		_config.pickLogEnable = false;
	}

	try {
		_config.amplTypeSNR = configGetString("autoloc.amplTypeSNR");
	}
	catch ( ... ) {}

	try {
		_config.amplTypeAbs = configGetString("autoloc.amplTypeAbs");
	}
	catch ( ... ) {}

	try {
		_config.stationLocationFile = configGetString("autoloc.stationLocations");
	}
	catch ( ... ) {}

	try {
		_config.playback = configGetBool("autoloc.playback");
	}
	catch ( ... ) {}

	try {
		_config.test = configGetBool("autoloc.test");
	}
	catch ( ... ) {}

	_config.pickLogPrefix = Environment::Instance()->absolutePath(_config.pickLogPrefix);
	_config.gridConfigFile = Environment::Instance()->absolutePath(_config.gridConfigFile);
	_config.stationLocationFile = Environment::Instance()->absolutePath(_config.stationLocationFile);

	// network type
	std::string ntp = "global";
	try {
		ntp = configGetString("autoloc.networkType");
	}
	catch ( ... ) {}
	if ( ntp == "global" ) {
		_config.networkType = Processing::AutolocConfig::GlobalNetwork;
	}
	else if ( ntp == "regional" ) {
		_config.networkType = Processing::AutolocConfig::RegionalNetwork;
	}
	else if ( ntp == "local" ) {
		_config.networkType = Processing::AutolocConfig::LocalNetwork;
	}
	else {
		SEISCOMP_ERROR("Illegal value '%s' for autoloc.networkType", ntp);
		return false;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool AutolocApp::validateParameters() {
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

	if ( !_inputFileXML.empty() ) {
		_config.playback = true;
		_config.offline = true;
		// Do not produce any output objects
		_config.test = true;
	}


	if ( _config.offline ) {
		setMessagingEnabled(false);
		if ( !_config.stationLocationFile.empty() ) {
			setDatabaseEnabled(false, false);
		}
	}

	// Load inventory from database only if no station location file was specified.
	if ( _config.stationLocationFile.empty() ) {
		setLoadStationsEnabled(true);

		if ( !isInventoryDatabaseEnabled() ) {
			setDatabaseEnabled(false, false);
		}
		else {
			setDatabaseEnabled(true, true);
		}
	}
	else {
		setLoadStationsEnabled(false);
		setDatabaseEnabled(false, false);
	}

	if ( commandline().hasOption("test") ) {
		_config.test = true;
	}

	if ( commandline().hasOption("playback") ) {
		_config.playback = true;
	}

	if ( commandline().hasOption("use-manual-picks") ) {
		_config.useManualPicks = true;
	}

	if ( commandline().hasOption("use-manual-origins") ) {
		_config.useManualOrigins = true;
	}

	_config.allowRejectedPicks = commandline().hasOption("allow-rejected-picks");

	if ( commandline().hasOption("use-imported-origins") ) {
		_config.useImportedOrigins = true;
	}

	if ( commandline().hasOption("try-default-depth") ) {
		_config.tryDefaultDepth = true;
	}

	if ( commandline().hasOption("adopt-manual-depth") ) {
		_config.adoptManualDepth = true;
	}

	// derived parameter
	_config.maxResidualKeep = 3 * _config.maxResidualUse;

	_formatted = commandline().hasOption("formatted");

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool AutolocApp::init() {
	// Call in this order:
	// - createCommandLineDescription
	// - initConfiguration
	// - validateParameters
	if ( !Client::Application::init() ) {
		return false;
	}

	_inputPicks = addInputObjectLog("pick");
	_inputAmps = addInputObjectLog("amplitude");
	_inputOrgs = addInputObjectLog("origin");
	_outputOrgs = addOutputObjectLog("origin", primaryMessagingGroup());

	SEISCOMP_INFO("Starting Autoloc");

	_config.agencyID = agencyID();
	_config.author = author();

	// This is the SeisComP configuration, which we need to pass through
	// all the way to the locator.
	_config.scconfig = &Client::Application::configuration();

	// TODO	_config.check();
	setConfig(_config);

	dumpConfig();

	if ( !initInventory() ) {
		return false;
	}

	if ( _config.playback || _config.offline ) {
		enableTimer(1);
	}
	else {
		// readHistoricEvents();
		if ( _wakeUpTimout > 0 ) {
			enableTimer(_wakeUpTimout);
		}
	}

	return Processing::Autoloc::init();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AutolocApp::readHistoricEvents() {
	if ( _config.originKeep <= 0 || !query() ) {
		return;
	}

	SEISCOMP_DEBUG("readHistoricEvents: reading %d seconds of events", _config.originKeep);

	typedef std::list<DataModel::OriginPtr> OriginList;
	typedef std::set<std::string> PickIds;

	// Fetch all historic events out of the database. The endtime is
	// probably in the future but because of timing differences between
	// different computers: safety first!
	Core::Time now = Core::Time::UTC();
	OriginList preferredOrigins;
	PickIds pickIds;

	// Store all preferred origins
	DataModel::DatabaseIterator it =
		query()->getPreferredOrigins(now - Core::TimeSpan(_config.originKeep, 0),
		                             now + Core::TimeSpan(_config.originKeep, 0), "");
	for ( ; it.get() != nullptr; ++it ) {
		DataModel::OriginPtr origin = DataModel::Origin::Cast(it.get());
		if ( origin )
			preferredOrigins.push_back(origin);
	}
	it.close();

	// Store all pickIDs of all origins and remove duplicates
	for ( auto& origin : preferredOrigins ) {
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
bool AutolocApp::fillObjectQueueFromXMLFile(const char *filename) {
	IO::XMLArchive ar;
	if ( !ar.open(filename) ) {
		SEISCOMP_ERROR("unable to open XML file '%s'", filename);
		return false;
	}

	ar >> _inputEP;
	ar.close();

	if ( !_inputEP ) {
		SEISCOMP_ERROR("No event parameters found in %s", filename);
		return false;
	}

	SEISCOMP_INFO("Finished reading event parameters from XML");
	SEISCOMP_INFO("  number of picks:      %d", _inputEP->pickCount());
	SEISCOMP_INFO("  number of amplitudes: %d", _inputEP->amplitudeCount());
	SEISCOMP_INFO("  number of origins:    %d", _inputEP->originCount());

	objectQueue.fill(_inputEP.get());
	if ( objectQueue.empty() ) {
		return false;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool AutolocApp::run() {

	if ( !_inputFileXML.empty() ) {
		// Initialize playback
		bool success = fillObjectQueueFromXMLFile(_inputFileXML.c_str());
		if ( !success ) {
			SEISCOMP_ERROR("Could not read input from XML file %s",
			               _inputFileXML);
			return false;
		}
		SEISCOMP_DEBUG("Length of object queue: %d", objectQueue.size());

		// Playback output will be added to the input.
	 	_outputEP = _inputEP;

		if ( _playbackSpeed > 0 ) {
			SEISCOMP_DEBUG("playback speed factor %g", _playbackSpeed);
		}

		objectsStartTime = playbackStartTime = Core::Time::UTC();
		objectCount = 0;
	}
	
	// TODO: Initialize playback from database

	return Application::run();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AutolocApp::done() {
	_exitRequested = true;
	shutdown();

	// final log of public object counts
	logObjectCounts();

	_flush();

	if ( _outputEP ) {
		IO::XMLArchive ar;
		ar.create("-");
		ar.setFormattedOutput(_formatted);
		ar << _outputEP;
		ar.close();
		std::cerr << "Output to XML: " << objectCount << " objects(s)" << std::endl;
		_outputEP = nullptr;
	}

	Application::done();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AutolocApp::handleTimeout() {

	if ( !_config.playback ) {
		_flush();
		return;
	}


	// The following is relevant (and executed) only for XML playback.

	while ( !objectQueue.empty() ) {

		DataModel::PublicObjectPtr optr = objectQueue.front();
		DataModel::PublicObject* o = optr.get();

		Core::Time t;

		// Retrieve the object creationTime...
		if ( DataModel::Pick::Cast(o) )
			t = DataModel::Pick::Cast(o)->creationInfo().creationTime();
		else if ( DataModel::Amplitude::Cast(o) )
			t = DataModel::Amplitude::Cast(o)->creationInfo().creationTime();
		else if ( DataModel::Origin::Cast(o) )
			t = DataModel::Origin::Cast(o)->creationInfo().creationTime();
		else continue;

		// Save time of first object
		if ( objectCount == 0 ) {
			objectsStartTime = t;
		}

		if ( _playbackSpeed > 0 ) {
			double dt = static_cast<double>(t - objectsStartTime);
			Core::TimeSpan dp = dt / _playbackSpeed;
			t = playbackStartTime + dp;
			if ( Core::Time::UTC() < t ) {
				// Pause until next handleTimeout() call
				break;
			}
		}

		objectQueue.pop();
		addObject("", o);
		objectCount++;
	}

	if ( objectQueue.empty() ) {
		quit();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AutolocApp::handleAutoShutdown() {
//	_flush();
	Client::Application::handleAutoShutdown();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void AutolocApp::addObject(const std::string& parentID, DataModel::Object* o) {
	using namespace DataModel;

	PublicObject *publicObject = PublicObject::Cast(o);
	if ( !publicObject ) {
		return;
	}

	bool extra_debug = false;

	Pick *pick = Pick::Cast(o);
	if ( pick ) {
		logObject(_inputPicks, Core::Time::UTC());
		if ( !feed(pick) ) {
			return;
		}
		if ( extra_debug ) {
			logObjectCounts();
		}
		return;
	}

	Amplitude *amplitude = Amplitude::Cast(o);
	if ( amplitude ) {
		logObject(_inputAmps, Core::Time::UTC());
		if ( !feed(amplitude) ) {
			return;
		}
		if ( extra_debug ) {
			logObjectCounts();
		}
		return;
	}

	Origin *origin = Origin::Cast(o);
	if ( origin ) {
		logObject(_inputOrgs, Core::Time::UTC());
		if ( !feed(origin) ) {
			return;
		}
		if ( extra_debug ) {
			logObjectCounts();
		}
		return;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool AutolocApp::feed(DataModel::Pick *scpick) {
	try {
		const Core::Time &creationTime = scpick->creationInfo().creationTime();
		sync(creationTime);
	}
	catch ( ... ) {
		SEISCOMP_WARNING("Pick %s: creation time is not set", scpick->publicID());
	}

	if ( objectAgencyID(scpick) != agencyID() && isAgencyIDBlocked(objectAgencyID(scpick)) ) {
		SEISCOMP_INFO("Ignoring pick %s due to blocked agency is '%s'", scpick->publicID(), objectAgencyID(scpick));
		return false;
	}

	bool feed_status = Processing::Autoloc::feed(scpick);

	if ( _config.offline ) {
		_flush();
	}

	return feed_status;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool AutolocApp::feed(DataModel::Amplitude *scampl) {
	const std::string &atype  = scampl->type();
	const std::string &amplID = scampl->publicID();
	if ( atype != _config.amplTypeAbs && atype != _config.amplTypeSNR ) {
		//	SEISCOMP_DEBUG("Ignoring '%s' amplitude %s: neither of type %s nor %s",
		//	               atype, amplID, _config.amplTypeAbs, _config.amplTypeSNR);
		return false;
	}

	SEISCOMP_INFO("Processing '%s' amplitude %s from pick %s",
	              scampl->type(), amplID, scampl->pickID());

	if ( objectAgencyID(scampl) != agencyID() ) {
		if ( isAgencyIDBlocked(objectAgencyID(scampl)) ) {
			SEISCOMP_INFO_S("  + ignoring amplitude since agency is: " + objectAgencyID(scampl));
			return false;
		}
		SEISCOMP_DEBUG("  + agency is: %s", objectAgencyID(scampl));
	}

	if ( _config.playback ) {
		try {
			const Core::Time &creationTime = scampl->creationInfo().creationTime();
			sync(creationTime);
		}
		catch ( ... ) {
			SEISCOMP_WARNING("Amplitude %s: creation time not set",
			                 amplID);
		}
	}

	return Processing::Autoloc::feed(scampl);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool AutolocApp::feed(DataModel::Origin *scorigin) {

	if ( !scorigin ) {
		return false;
	}

	SEISCOMP_INFO("Got origin %s from agency %s",
	              scorigin->publicID(), objectAgencyID(scorigin));

	const bool ownOrigin = objectAgencyID(scorigin) == agencyID();

	if ( ownOrigin ) {
		if ( manual(scorigin) ) {
			if ( !_config.useManualOrigins ) {
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
		if ( !_config.useImportedOrigins ) {
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
	for ( size_t i = 0; i < arrivalCount; i++ ) {
		const std::string &pickID = scorigin->arrival(i)->pickID();
		DataModel::Pick *scpick = DataModel::Pick::Find(pickID);
		if ( !scpick ) {
			SEISCOMP_ERROR_S("Pick " + pickID + " not found");
		}
	}

	bool status = Processing::Autoloc::feed(scorigin);

	if ( status == true && (_config.offline || _config.playback || _config.test) ) {
		_flush();
	}

	return status;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool AutolocApp::_report(DataModel::Origin *scorigin) {
	// Log object flow
	logObject(_outputOrgs, now());

	if ( _config.offline || _config.playback || _config.test ) {
		// In offline/playback/test mode do not send origin to messaging

		// But add it to the output EventParameters (except test mode)
		if ( _outputEP ) {
			_outputEP->add(scorigin);
		}
	}
	else {
		// Online mode -> send origin to messaging
		DataModel::EventParameters ep;
		bool wasEnabled = DataModel::Notifier::IsEnabled();
		DataModel::Notifier::Enable();
		ep.add(scorigin);
		DataModel::Notifier::SetEnabled(wasEnabled);

		DataModel::NotifierMessagePtr nmsg = DataModel::Notifier::GetMessage(true);
		connection()->send(nmsg.get());

		// For preliminary origins, also create/send a corresponding journal entry
		if ( preliminary(scorigin) ) {
			SEISCOMP_INFO("Sent preliminary origin %s (heads up)", scorigin->publicID());

			// Create and send journal entry
			DataModel::JournalEntryPtr journalEntry = new DataModel::JournalEntry;
			journalEntry->setAction("OrgEvalStatOK");
			journalEntry->setObjectID(scorigin->publicID());
			journalEntry->setSender(SCCoreApp->author().c_str());
			journalEntry->setParameters("preliminary");
			journalEntry->setCreated(Core::Time::UTC());

			DataModel::NotifierMessagePtr jm = new DataModel::NotifierMessage;
			jm->attach(new DataModel::Notifier(DataModel::Journaling::ClassName(),
			           DataModel::OP_ADD, journalEntry.get()));

			if ( connection()->send(jm.get()) ) {
				SEISCOMP_DEBUG("Sent origin journal entry for origin %s to the message group: %s",
				               scorigin->publicID(), primaryMessagingGroup());
			}
			else {
				SEISCOMP_ERROR("Sending origin journal entry failed with error: %s",
				               connection()->lastError().toString());
			}
		}
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




} // namespace Applications

} // namespace Seiscomp
