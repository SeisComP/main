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
#include <seiscomp/core/datamessage.h>
#include <seiscomp/io/archive/xmlarchive.h>
#include <algorithm>

#include "app.h"
#include <seiscomp/autoloc/datamodel.h>
#include <seiscomp/autoloc/sc3adapters.h>
#include <seiscomp/autoloc/util.h>
#include <seiscomp/autoloc/stationlocationfile.h>

using namespace std;
using namespace Seiscomp::Client;
using namespace Seiscomp::Math;


namespace Seiscomp {

namespace Autoloc {
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




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

	_keepEventsTimeSpan = 86400; // one day
	_wakeUpTimout = 5; // wake up every 5 seconds to check pending operations

	_playbackSpeed = 1;
}

// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
App::~App() {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::createCommandLineDescription() {
	Client::Application::createCommandLineDescription();

	commandline().addGroup("Mode");
	commandline().addOption("Mode", "test", "Do not send any object");
	commandline().addOption("Mode", "offline", "Do not connect to a messaging server. Instead a station-locations.conf file can be provided. This implies --test and --playback");
	commandline().addOption("Mode", "playback", "Flush origins immediately without delay");
	commandline().addOption("Mode", "xml-playback", "TODO"); // TODO
	commandline().addGroup("Input");
	commandline().addOption("Input", "input,i", "XML input file for --xml-playback",&_inputFileXML, false);
	commandline().addOption("Input", "ep", "Event parameters XML file for offline processing of all contained picks and amplitudes" ,&_inputEPFile, false);

	commandline().addGroup("Settings");
	commandline().addOption("Settings", "station-locations", "The station-locations.conf file to use when in offline mode. If no file is given the database is used.", &_stationLocationFile, false);
	commandline().addOption("Settings", "station-config", "The station configuration file", &config.staConfFile, false);
	commandline().addOption("Settings", "grid", "The grid configuration file", &_gridConfigFile, false);
	commandline().addOption("Settings", "pick-log", "The pick log file. Providing a "
	                        "file name enables logging picks even when disabled by configuration.",
	                        &config.pickLogFile, false);

	commandline().addOption("Settings", "default-depth", "Default depth for locating", &config.defaultDepth);
	commandline().addOption("Settings", "default-depth-stickiness", "", &config.defaultDepthStickiness);
	commandline().addOption("Settings", "max-sgap", "Maximum secondary azimuthal gap", &config.maxAziGapSecondary);
	commandline().addOption("Settings", "max-rms", "Maximum RMS residual"
	                        "to be considered", &config.maxRMS);
	commandline().addOption("Settings", "max-residual", "Maximum travel-time residual"
	                        "per station to be considered", &config.maxResidualUse);
	commandline().addOption("Settings", "max-station-distance", "Maximum distance of stations to be used", &config.maxStaDist);
	commandline().addOption("Settings", "max-nucleation-distance-default", "Default maximum distance of stations to be used for nucleating new origins", &config.defaultMaxNucDist);
	commandline().addOption("Settings", "min-pick-affinity", "", &config.minPickAffinity);

	commandline().addOption("Settings", "min-phase-count", "Minimum number of picks for an origin to be reported", &config.minPhaseCount);
	commandline().addOption("Settings", "min-score", "Minimum score for an origin to be reported", &config.minScore);
	commandline().addOption("Settings", "min-pick-snr", "Minimum SNR for a pick to be processed", &config.minPickSNR);

	commandline().addOption("Settings", "xxl-enable", "", &config.xxlEnabled);
	commandline().addOption("Settings", "xxl-min-phase-count", "Minimum number of picks for an XXL origin to be reported", &config.xxlMinPhaseCount);
	commandline().addOption("Settings", "xxl-min-amplitude", "Flag pick as XXL if BOTH snr and amplitude exceed a threshold", &config.xxlMinAmplitude);
	commandline().addOption("Settings", "xxl-min-snr", "Flag pick as XXL if BOTH snr and amplitude exceed a threshold", &config.xxlMinSNR);
	commandline().addOption("Settings", "xxl-max-distance", "", &config.xxlMaxStaDist);
	commandline().addOption("Settings", "xxl-max-depth", "", &config.xxlMaxDepth);
	commandline().addOption("Settings", "xxl-dead-time", "", &config.xxlDeadTime);

	commandline().addOption("Settings", "min-sta-count-ignore-pkp", "Minimum station count for which we ignore PKP phases", &config.minStaCountIgnorePKP);
	commandline().addOption("Settings", "min-score-bypass-nucleator", "Minimum score at which the nucleator is bypassed", &config.minScoreBypassNucleator);

	commandline().addOption("Settings", "keep-events-timespan", "The timespan to keep historical events", &_keepEventsTimeSpan);

	commandline().addOption("Settings", "cleanup-interval", "The object cleanup interval in seconds", &config.cleanupInterval);
	commandline().addOption("Settings", "max-age", "During cleanup all objects older than maxAge (in seconds) are removed (maxAge == 0 => disable cleanup)", &config.maxAge);

	commandline().addOption("Settings", "wakeup-interval", "The interval in seconds to check pending operations", &_wakeUpTimout);
	commandline().addOption("Settings", "speed", "Set this to greater 1 to increase XML playback speed", &_playbackSpeed);
	commandline().addOption("Settings", "dynamic-pick-threshold-interval", "The interval in seconds in which to check for extraordinarily high pick activity, resulting in a dynamically increased pick threshold", &config.dynamicPickThresholdInterval);

	commandline().addOption("Settings", "use-manual-picks", "allow use of manual picks for nucleation and location");
	commandline().addOption("Settings", "use-manual-origins", "allow use of manual origins from our own agency");
	commandline().addOption("Settings", "use-imported-origins", "allow use of imported origins from trusted agencies as configured in 'processing.whitelist.agencies'. Imported origins are not relocated and only used for phase association");
//	commandline().addOption("Settings", "resend-imported-origins", "Re-send imported origins after phase association");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::validateParameters() {
	if ( commandline().hasOption("offline") ) {
		config.offline = true;
		config.playback = true;
		config.test = true;
	}
	else
		config.offline = false;

	if ( !_inputEPFile.empty() ) {
		config.playback = true;
		config.offline = true;
	}


	if ( config.offline ) {
		setMessagingEnabled(false);
		if ( !_stationLocationFile.empty() )
			setDatabaseEnabled(false, false);
	}

	// If a station location file was specified.
	if ( _stationLocationFile.size() ) {
		// inventory will be read from station location file
		setLoadStationsEnabled(false);
		setDatabaseEnabled(false, false);
	}
	else {
		// inventory will be loaded from database
		setLoadStationsEnabled(true);

		if ( !isInventoryDatabaseEnabled() )
			setDatabaseEnabled(false, false);
		else
			setDatabaseEnabled(true, true);
	}

	// Maybe we do want to allow sending of origins in offline mode?
	if ( commandline().hasOption("test") )
		config.test = true;

	if ( commandline().hasOption("playback") )
		config.playback = true;

	if ( commandline().hasOption("use-manual-picks") )
		config.useManualPicks = true;

	if ( commandline().hasOption("use-manual-origins") )
		config.useManualOrigins = true;

	if ( commandline().hasOption("use-imported-origins") )
		config.useImportedOrigins = true;

	if ( commandline().hasOption("try-default-depth") )
		config.tryDefaultDepth = true;

	if ( commandline().hasOption("adopt-manual-depth") )
		config.adoptManualDepth = true;

	config.maxResidualKeep = 3*config.maxResidualUse;

	if ( !config.pickLogFile.empty() ) {
		config.pickLogEnable = true;
	}

	return Client::Application::validateParameters();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::initConfiguration() {
	if ( !Client::Application::initConfiguration() ) return false;

	// support deprecated configuration, deprecated since 2020-11-16
	try {
		config.maxAge = configGetDouble("autoloc.maxAge");
		SEISCOMP_ERROR("Configuration parameter autoloc.maxAge is deprecated."
		               " Use buffer.pickKeep instead!");
	}
	catch (...) {}
	// override deprecated configuration if value is set
	try { config.maxAge = configGetDouble("buffer.pickKeep"); }
	catch (...) {}

	try {
		config.authors = configGetStrings("autoloc.authors");
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
	try { config.cleanupInterval = configGetDouble("autoloc.cleanupInterval");
		SEISCOMP_ERROR("Configuration parameter autoloc.cleanupInterval is deprecated."
		               " Use buffer.cleanupIntervalinstead!");}
	catch (...) {}
	try { config.cleanupInterval = configGetDouble("buffer.cleanupInterval"); }
	catch (...) {}

	try { config.defaultDepth = configGetDouble("locator.defaultDepth"); }
	catch (...) {}

	try { config.defaultDepthStickiness = configGetDouble("autoloc.defaultDepthStickiness"); }
	catch (...) {}

	try { config.tryDefaultDepth = configGetBool("autoloc.tryDefaultDepth"); }
	catch (...) {}

	try { config.adoptManualDepth = configGetBool("autoloc.adoptManualDepth"); }
	catch (...) {}

	try { config.minimumDepth = configGetDouble("locator.minimumDepth"); }
	catch (...) {}

	try { config.maxAziGapSecondary = configGetDouble("autoloc.maxSGAP"); }
	catch (...) {}

	try { config.maxRMS = configGetDouble("autoloc.maxRMS"); }
	catch (...) {}

	try { config.maxDepth = configGetDouble("autoloc.maxDepth"); }
	catch (...) {}

	try { config.maxResidualUse = configGetDouble("autoloc.maxResidual"); }
	catch (...) {}

	try { config.maxStaDist = configGetDouble("autoloc.maxStationDistance"); }
	catch (...) {}

	try { config.defaultMaxNucDist = configGetDouble("autoloc.defaultMaxNucleationDistance"); }
	catch (...) {}

	try { config.xxlEnabled = configGetBool("autoloc.xxl.enable"); }
	catch (...) {}

	try { config.xxlMinAmplitude = configGetDouble("autoloc.xxl.minAmplitude"); }
	catch (...) {
		try {
			// deprecated since 2013-06-26
			config.xxlMinAmplitude = configGetDouble("autoloc.thresholdXXL");
			SEISCOMP_ERROR("Config parameter autoloc.thresholdXXL is deprecated.  Use autoloc.xxl.minAmplitude instead!");
		}
		catch (...) {}
	}

	try { config.xxlMaxStaDist = configGetDouble("autoloc.xxl.maxStationDistance"); }
	catch (...) {
		try {
			// deprecated since 2013-06-26
			config.xxlMaxStaDist = configGetDouble("autoloc.maxStationDistanceXXL");
			SEISCOMP_ERROR("Configuration parameter autoloc.maxStationDistanceXXL"
			               " is deprecated. Use autoloc.xxl.maxStationDistance instead!");
		}
		catch (...) {}
	}

	try { config.xxlMinPhaseCount = configGetInt("autoloc.xxl.minPhaseCount"); }
	catch (...) {
		try {
			// deprecated since 2013-06-26
			config.xxlMinPhaseCount = configGetInt("autoloc.minPhaseCountXXL");
			SEISCOMP_ERROR("Configuration parameter autoloc.minPhaseCountXXL is"
			               " deprecated. Use autoloc.xxl.minPhaseCount instead!");
		}
		catch (...) {}
	}

	try { config.xxlMinSNR = configGetDouble("autoloc.xxl.minSNR"); }
	catch (...) {}

	try { config.xxlMaxDepth = configGetDouble("autoloc.xxl.maxDepth"); }
	catch (...) {}

	try { config.xxlDeadTime = configGetDouble("autoloc.xxl.deadTime"); }
	catch (...) {}

	try { config.minPickSNR = configGetDouble("autoloc.minPickSNR"); }
	catch (...) {}

	try { config.minPickAffinity = configGetDouble("autoloc.minPickAffinity"); }
	catch (...) {}

	try { config.minPhaseCount = configGetInt("autoloc.minPhaseCount"); }
	catch (...) {}

	try { config.minScore = configGetDouble("autoloc.minScore"); }
	catch (...) {}

	try { config.minScoreBypassNucleator = configGetDouble("autoloc.minScoreBypassNucleator"); }
	catch (...) {}

	try { config.minStaCountIgnorePKP = configGetInt("autoloc.minStaCountIgnorePKP"); }
	catch (...) {}

	try { config.reportAllPhases = configGetBool("autoloc.reportAllPhases"); }
	catch (...) {}

	try { config.useManualPicks = configGetBool("autoloc.useManualPicks"); }
	catch (...) {
		config.useManualPicks = false;
	}

	try { config.useManualOrigins = configGetBool("autoloc.useManualOrigins"); }
	catch (...) {}

	try { config.useImportedOrigins = configGetBool("autoloc.useImportedOrigins"); }
	catch (...) {}

	try { _wakeUpTimout = configGetInt("autoloc.wakeupInterval"); }
	catch (...) {}

	try { config.maxRadiusFactor = configGetDouble("autoloc.gridsearch._maxRadiusFactor"); }
	catch (...) {}

	try { config.publicationIntervalTimeSlope = configGetDouble("autoloc.publicationIntervalTimeSlope"); }
	catch ( ... ) {}

	try { config.publicationIntervalTimeIntercept = configGetDouble("autoloc.publicationIntervalTimeIntercept"); }
	catch ( ... ) {}

	try { config.publicationIntervalPickCount = configGetInt("autoloc.publicationIntervalPickCount"); }
	catch ( ... ) {}

	try { config.dynamicPickThresholdInterval = configGetDouble("autoloc.dynamicPickThresholdInterval"); }
	catch ( ... ) {}

	try { _gridConfigFile = Environment::Instance()->absolutePath(configGetString("autoloc.grid")); }
	catch (...) { _gridConfigFile = Environment::Instance()->shareDir() + "/scautoloc/grid.conf"; }

	try { config.staConfFile = Environment::Instance()->absolutePath(configGetString("autoloc.stationConfig")); }
	catch (...) { config.staConfFile = Environment::Instance()->shareDir() + "/scautoloc/station.conf"; }

	try { config.pickLogFile = configGetString("autoloc.pickLog"); }
	catch (...) { config.pickLogFile = ""; }

	try { config.pickLogEnable = configGetBool("autoloc.pickLogEnable"); }
	catch (...) { config.pickLogEnable = false; }
	if ( !config.pickLogEnable ) {
		config.pickLogFile = "";
	}

	try { config.amplTypeSNR = configGetString("autoloc.amplTypeSNR"); }
	catch (...) {}

	try { config.amplTypeAbs = configGetString("autoloc.amplTypeAbs"); }
	catch (...) {}

	try { _stationLocationFile = configGetString("autoloc.stationLocations"); }
	catch (...) {}

	// support deprecated configuration, deprecated since 2020-11-13
	try {
		config.locatorProfile = configGetString("autoloc.locator.profile");
		SEISCOMP_ERROR("Configuration parameter autoloc.locator.profile is deprecated."
		                 " Use locator.profile instead!");
	}
	catch (...) {}

	// override deprecated configuration if value is set
	try { config.locatorProfile = configGetString("locator.profile"); }
	catch (...) {}

	try { config.playback = configGetBool("autoloc.playback"); }
	catch ( ... ) {}

	try { config.test = configGetBool("autoloc.test"); }
	catch ( ... ) {}

	config.pickLogFile = Environment::Instance()->absolutePath(config.pickLogFile);
	_stationLocationFile = Environment::Instance()->absolutePath(_stationLocationFile);

	// network type
	std::string ntp = "global";
	try { ntp = configGetString("autoloc.networkType"); }
	catch ( ... ) {}
	if      ( ntp == "global" ) {
		config.networkType = Autoloc::GlobalNetwork;
	}
	else if ( ntp == "regional" ) {
		config.networkType = Autoloc::RegionalNetwork;
	}
	else if ( ntp == "local" ) {
		config.networkType = Autoloc::LocalNetwork;
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

	if ( ! Client::Application::init() ) return false;

	_inputPicks = addInputObjectLog("pick");
	_inputAmps = addInputObjectLog("amplitude");
	_inputOrgs = addInputObjectLog("origin");
	_outputOrgs = addOutputObjectLog("origin", primaryMessagingGroup());

	SEISCOMP_INFO("Starting Autoloc");
	setConfig(config);
	dumpConfig();
	if ( ! setGridFile(_gridConfigFile) )
		return false;

	if ( ! initInventory() )
		return false;

	if ( ! config.pickLogFile.empty() ) {
		setPickLogFilePrefix(config.pickLogFile);
	}
	else {
		SEISCOMP_DEBUG("Do not set a pick log file: Disabled by configuration "
		               "of autolog.pickLogEnable");
	}

	if ( config.playback ) {
		if ( _inputEPFile.empty() ) {
			// XML playback, set timer to 1 sec
			SEISCOMP_DEBUG("Playback mode - enable timer of 1 sec");
			enableTimer(1);
		}
	}
	else {
		// Read historical preferred origins in case we missed something
		readHistoricEvents();

		if ( _wakeUpTimout > 0 ) {
			SEISCOMP_DEBUG("Enable timer of %d secs", _wakeUpTimout);
			enableTimer(_wakeUpTimout);
		}
	}

	config.agencyID = agencyID();
	
	return Autoloc3::init();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::initInventory() {
	if ( _stationLocationFile.empty() ) {
		SEISCOMP_DEBUG("Initializing station inventory from DB");
		inventory = Inventory::Instance()->inventory();
	}
	else {
		SEISCOMP_DEBUG_S("Initializing station inventory from file '" +
				 _stationLocationFile + "'");
		inventory = Seiscomp::Autoloc::Util::inventoryFromStationLocationFile(_stationLocationFile);
	}

	if ( ! inventory ) {
		SEISCOMP_ERROR("No station inventory!");
		return false;
	}

	Autoloc3::setInventory(inventory.get());

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::readHistoricEvents() {
	if ( _keepEventsTimeSpan <= 0 || ! query() ) return;

	SEISCOMP_DEBUG("readHistoricEvents: reading %d seconds of events", _keepEventsTimeSpan);

	typedef std::list<Seiscomp::DataModel::OriginPtr> OriginList;
	typedef std::set<std::string> PickIds;

	// Fetch all historic events out of the database. The endtime is
	// probably in the future but because of timing differences between
	// different computers: safety first!
	Seiscomp::Core::Time now = Seiscomp::Core::Time::GMT();
	Seiscomp::DataModel::DatabaseIterator it =
		query()->getPreferredOrigins(now - Seiscomp::Core::TimeSpan(_keepEventsTimeSpan),
		                             now + Seiscomp::Core::TimeSpan(_keepEventsTimeSpan), "");

	OriginList preferredOrigins;
	PickIds pickIds;

	// Store all preferred origins
	for ( ; it.get() != NULL; ++it ) {
		Seiscomp::DataModel::OriginPtr o =
			Seiscomp::DataModel::Origin::Cast(it.get());
		if ( o ) preferredOrigins.push_back(o);
	}
	it.close();

	// Store all pickIDs of all origins and remove duplicates
	for ( OriginList::iterator it = preferredOrigins.begin();
	      it != preferredOrigins.end(); ++it ) {
		Seiscomp::DataModel::OriginPtr origin = *it;
		if ( origin->arrivalCount() == 0 ) {
			query()->loadArrivals(it->get());
			for ( size_t i = 0; i < origin->arrivalCount(); ++i )
				pickIds.insert(origin->arrival(i)->pickID());
		}

		SEISCOMP_DEBUG_S("read historical origin "+origin->publicID());

		// Feed it!
		//feedOrigin(it->get());
	}

	// Read all picks out of the database
	for ( PickIds::iterator it = pickIds.begin();
	      it != pickIds.end(); ++it ) {

		Seiscomp::DataModel::ObjectPtr obj =
			query()->getObject(DataModel::Pick::TypeInfo(), *it);
		if ( !obj ) continue;
		Seiscomp::DataModel::PickPtr pick =
			Seiscomp::DataModel::Pick::Cast(obj);
		if ( !pick ) continue;

		// Feed it!
		//feedPick(pick.get());
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::runFromXMLFile(const char *fname)
{
	Seiscomp::DataModel::EventParametersPtr ep =
		new Seiscomp::DataModel::EventParameters();
	Seiscomp::IO::XMLArchive ar;

	if ( ! ar.open(fname)) {
		SEISCOMP_ERROR("unable to open XML playback file '%s'", fname);
		return false;
	}

	ar >> ep;
	SEISCOMP_INFO("finished reading event parameters from XML");
	SEISCOMP_INFO("  number of picks:      %ld", (long int)ep->pickCount());
	SEISCOMP_INFO("  number of amplitudes: %ld", (long int)ep->amplitudeCount());
	SEISCOMP_INFO("  number of origins:    %ld", (long int)ep->originCount());

	typedef std::pair<Seiscomp::Core::Time, Seiscomp::DataModel::PublicObjectPtr> TimeObject;
	typedef std::vector<TimeObject> TimeObjectVector;

	// retrieval of relevant objects from event parameters
	// and subsequent DSU sort
	TimeObjectVector objs;
	while (ep->pickCount() > 0) {
		Seiscomp::DataModel::PickPtr pick = ep->pick(0);
		ep->removePick(0);
		Seiscomp::DataModel::PublicObjectPtr o(pick);
		Seiscomp::Core::Time t = pick->creationInfo().creationTime();
		objs.push_back(TimeObject(t,o));
	}
	while (ep->amplitudeCount() > 0) {
		Seiscomp::DataModel::AmplitudePtr amplitude = ep->amplitude(0);
		ep->removeAmplitude(0);
		Seiscomp::DataModel::PublicObjectPtr o(amplitude);
		Seiscomp::Core::Time t = amplitude->creationInfo().creationTime();
		objs.push_back(TimeObject(t,o));
	}
	while (ep->originCount() > 0) {
		Seiscomp::DataModel::OriginPtr origin = ep->origin(0);
		ep->removeOrigin(0);
		Seiscomp::DataModel::PublicObjectPtr o(origin);
		Seiscomp::Core::Time t = origin->creationInfo().creationTime();
		objs.push_back(TimeObject(t,o));
	}

	std::sort(objs.begin(),objs.end());
	for (TimeObjectVector::iterator
	     it = objs.begin(); it != objs.end(); ++it) {
		_objects.push(it->second);
	}

	if ( _objects.empty() )
		return false;

	if (_playbackSpeed > 0) {
		SEISCOMP_DEBUG("playback speed factor %g", _playbackSpeed);
	}

	objectsStartTime = playbackStartTime = Core::Time::GMT();
	objectCount = 0;
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::runFromEPFile(const char *fname) {
	Seiscomp::IO::XMLArchive ar;

	if ( !ar.open(fname)) {
		SEISCOMP_ERROR("unable to open XML file: %s", fname);
		return false;
	}

	ar >> ep;
	ar.close();

	if ( ! ep ) {
		SEISCOMP_ERROR("No event parameters found: %s", fname);
		return false;
	}

	SEISCOMP_INFO("finished reading event parameters from XML");
	SEISCOMP_INFO("  number of picks:      %ld", (long int)ep->pickCount());
	SEISCOMP_INFO("  number of amplitudes: %ld", (long int)ep->amplitudeCount());
	SEISCOMP_INFO("  number of origins:    %ld", (long int)ep->originCount());

	typedef std::pair<Seiscomp::Core::Time, Seiscomp::DataModel::PublicObjectPtr> TimeObject;
	typedef std::vector<TimeObject> TimeObjectVector;

	// retrieval of relevant objects from event parameters
	// and subsequent DSU sort
	TimeObjectVector objs;

	for ( size_t i = 0; i < ep->pickCount(); ++i ) {
		Seiscomp::DataModel::PickPtr pick = ep->pick(i);
		try {
			Seiscomp::Core::Time t =
				pick->creationInfo().creationTime();
			objs.push_back(TimeObject(t, pick));
		}
		catch ( ... ) {
			SEISCOMP_WARNING("Ignore pick %s: no creation time set",
			                 pick->publicID().c_str());
		}
	}

	for ( size_t i = 0; i < ep->amplitudeCount(); ++i ) {
		Seiscomp::DataModel::AmplitudePtr amplitude = ep->amplitude(i);
		try {
			Seiscomp::Core::Time t =
				amplitude->creationInfo().creationTime();
			objs.push_back(TimeObject(t, amplitude));
		}
		catch ( ... ) {
			SEISCOMP_WARNING("Ignore amplitude %s: no creation time set",
			                 amplitude->publicID().c_str());
		}
	}

	for ( size_t i = 0; i < ep->originCount(); ++i ) {
		Seiscomp::DataModel::OriginPtr origin = ep->origin(i);
		try {
			Seiscomp::Core::Time t = origin->creationInfo().creationTime();
			objs.push_back(TimeObject(t, origin));
		}
		catch ( ... ) {
			SEISCOMP_WARNING("Ignore origin %s: no creation time set",
			                 origin->publicID().c_str());
		}
	}

	std::sort(objs.begin(), objs.end());
	for (TimeObjectVector::iterator
	     it = objs.begin(); it != objs.end(); ++it) {
		_objects.push(it->second);
	}

	while ( !_objects.empty() && !isExitRequested() ) {
		Seiscomp::DataModel::PublicObjectPtr o = _objects.front();

		_objects.pop();
		addObject("", o.get());
		++objectCount;
	}

	_flush();

	ar.create("-");
	ar.setFormattedOutput(true);
	ar << ep;
	ar.close();

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
	SEISCOMP_DEBUG_S("Timestamp: "+now().toString("%F %T"));
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
	if ( config.playback && _inputFileXML.size() > 0) {
		runFromXMLFile(_inputFileXML.c_str());
		return Application::run();
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::done() {
	_exitRequested = true;
// FIXME	_flush();
	shutdown();
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

	if ( ! config.playback || _inputFileXML.empty() ) {
		_flush();
		return;
	}


	// The following is relevant (and executed) only for XML playback.

	while ( ! _objects.empty() && !isExitRequested() ) {

		Core::Time t;
		Seiscomp::DataModel::PublicObjectPtr o = _objects.front();

		// retrieve the creationTime...
		if (Seiscomp::DataModel::Pick::Cast(o.get()))
			t = Seiscomp::DataModel::Pick::Cast(o.get())->creationInfo().creationTime();
		else if (Seiscomp::DataModel::Amplitude::Cast(o.get()))
			t = Seiscomp::DataModel::Amplitude::Cast(o.get())->creationInfo().creationTime();
		else if (Seiscomp::DataModel::Origin::Cast(o.get()))
			t = Seiscomp::DataModel::Origin::Cast(o.get())->creationInfo().creationTime();
		else continue;

		// at the first object:
		if (objectCount == 0)
			objectsStartTime = t;

		if (_playbackSpeed > 0) {
			double dt = t - objectsStartTime;
			Seiscomp::Core::TimeSpan dp = dt/_playbackSpeed;
			t = playbackStartTime + dp;
			if (Seiscomp::Core::Time::GMT() < t)
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
//	SEISCOMP_DEBUG("Autoshutdown: flushing pending results");
// XXX FIXME: The following causes the shutdown to hang.
//	_flush();
	Seiscomp::Client::Application::handleAutoShutdown();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



static bool manual(const Seiscomp::DataModel::Origin *origin) {
	try {
		switch (origin->evaluationMode()) {
		case Seiscomp::DataModel::MANUAL:
			return true;
		default:
			break;
		}
	}
	catch ( Seiscomp::Core::ValueException & ) {}
	return false;
}

/*
static bool preliminary(const Seiscomp::DataModel::Origin *origin) {
	try {
		switch (origin->evaluationStatus()) {
		case Seiscomp::DataModel::PRELIMINARY:
			return true;
		default:
			break;
		}
	}
	catch ( Seiscomp::Core::ValueException & ) {}
	return false;
}
*/


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::addObject(
	const std::string& parentID, Seiscomp::DataModel::Object* o) {

	Seiscomp::DataModel::PublicObject *po =
	       	Seiscomp::DataModel::PublicObject::Cast(o);
	if ( po == NULL )
		return;
	// SEISCOMP_DEBUG("adding  %-12s %s", po->className(), po->publicID().c_str());

	Seiscomp::DataModel::Pick *pick =
		Seiscomp::DataModel::Pick::Cast(o);
	if ( pick ) {
		logObject(_inputPicks, Seiscomp::Core::Time::GMT());
		feed(pick);
		return;
	}

	Seiscomp::DataModel::Amplitude *amplitude =
		Seiscomp::DataModel::Amplitude::Cast(o);
	if ( amplitude ) {
		logObject(_inputAmps, Seiscomp::Core::Time::GMT());
		feed(amplitude);
		return;
	}

	Seiscomp::DataModel::Origin *origin =
		Seiscomp::DataModel::Origin::Cast(o);
	if ( origin ) {
		logObject(_inputOrgs, Seiscomp::Core::Time::GMT());
		feed(origin);
		return;
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::removeObject(const std::string& parentID, Seiscomp::DataModel::Object* o) {
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void App::updateObject(const std::string& parentID, Seiscomp::DataModel::Object* o) {
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::feed(Seiscomp::DataModel::Pick *scpick) {

	const std::string &pickID = scpick->publicID();

	if (_inputFileXML.size() || _inputEPFile.size()) {
		try {
			const Seiscomp::Core::Time &creationTime = scpick->creationInfo().creationTime();
			sync(creationTime);
		}
		catch(...) {
			SEISCOMP_WARNING_S("Pick "+pickID+" without creation time!");
		}
	}

	if (objectAgencyID(scpick) != config.agencyID) {
		if ( isAgencyIDBlocked(objectAgencyID(scpick)) ) {
			SEISCOMP_INFO_S("Blocked pick from agency '" + objectAgencyID(scpick) + "'");
			return false;
		}

		SEISCOMP_INFO("pick '%s' from agency '%s'", pickID.c_str(), objectAgencyID(scpick).c_str());

	}

	const std::string &author = objectAuthor(scpick);
	const int priority = _authorPriority(author);
	SEISCOMP_INFO("pick '%s' from author '%s' has priority %d", pickID.c_str(), author.c_str(), priority);
	if (priority == 0) {
		SEISCOMP_INFO("pick '%s' not processed", pickID.c_str());
		return false;
	}

	try {
		if (scpick->evaluationMode() == Seiscomp::DataModel::MANUAL) {
		}
	}
	catch ( ... ) {
		SEISCOMP_WARNING_S("pick has no evaluation mode: " + pickID);
		scpick->setEvaluationMode(Seiscomp::DataModel::EvaluationMode(Seiscomp::DataModel::AUTOMATIC));
	}

	// configure station if needed
//	initOneStation(scpick->waveformID(), scpick->time().value());

	timeStamp();

	if ( ! Autoloc3::feed(scpick))
		return false;

	if ( config.offline )
		_flush();

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::feed(Seiscomp::DataModel::Amplitude *scampl) {

	const std::string &amplID = scampl->publicID();

	if (_inputFileXML.size() || _inputEPFile.size()) {
		try {
			const Seiscomp::Core::Time &creationTime = scampl->creationInfo().creationTime();
			sync(creationTime);
		}
		catch(...) {
			SEISCOMP_WARNING_S("Pick "+amplID+" without creation time!");
		}
	}

	if (objectAgencyID(scampl) != config.agencyID) {
		if ( isAgencyIDBlocked(objectAgencyID(scampl)) ) {
			SEISCOMP_INFO_S("Blocked amplitude from agency '" + objectAgencyID(scampl) + "'");
			return false;
		}
		SEISCOMP_INFO("ampl '%s' from agency '%s'", amplID.c_str(), objectAgencyID(scampl).c_str());
	}

	Autoloc3::feed(scampl);
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::feed(Seiscomp::DataModel::Origin *scorigin) {

	if ( ! scorigin ) {
		SEISCOMP_ERROR("This should never happen: origin=NULL");
		return false;
	}

	SEISCOMP_INFO_S("got origin " + scorigin->publicID() +
			"   agency: " + objectAgencyID(scorigin));

	const bool ownOrigin = objectAgencyID(scorigin) == config.agencyID;

	if ( ownOrigin ) {
		if ( manual(scorigin) ) {
			if ( ! config.useManualOrigins ) {
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

		if ( ! config.useImportedOrigins ) {
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

	Autoloc3::feed(scorigin);

	if ( config.offline )
		_flush();

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool App::_report(Seiscomp::DataModel::Origin *scorigin) {

	using namespace Seiscomp::DataModel;

	// Log object flow
	logObject(_outputOrgs, now());

	CreationInfo ci;
	ci.setAgencyID(config.agencyID);
	ci.setAuthor(author());
	ci.setCreationTime(now());
	scorigin->setCreationInfo(ci);

	if (config.offline || config.test) {
		SEISCOMP_INFO ("Origin %s not sent (test/offline mode)", scorigin->publicID().c_str());

		if (ep)
			ep->add(scorigin);

		return true;
	}

	EventParameters ep;
	bool wasEnabled = Notifier::IsEnabled();
	Notifier::Enable();
	ep.add(scorigin);
	Notifier::SetEnabled(wasEnabled);
	NotifierMessagePtr nmsg =
		Notifier::GetMessage(true);
	connection()->send(nmsg.get());
	SEISCOMP_INFO ("Origin %s sent", scorigin->publicID().c_str());

	if (scorigin->evaluationStatus() == PRELIMINARY) {
		SEISCOMP_INFO("Sent preliminary origin %s (heads up)", scorigin->publicID().c_str());

		// create and send journal entry
		string str = "";
		try {
			str = scorigin->evaluationStatus().toString();
		}
		catch ( Seiscomp::Core::ValueException & ) {}

		if ( !str.empty() ) {
			JournalEntryPtr journalEntry =
				new JournalEntry;
			journalEntry->setAction("OrgEvalStatOK");
			journalEntry->setObjectID(scorigin->publicID());
			journalEntry->setSender(SCCoreApp->author().c_str());
			journalEntry->setParameters(str);
			journalEntry->setCreated(Core::Time::GMT());

			NotifierMessagePtr jm = new NotifierMessage;
			jm->attach(new Notifier(
				Journaling::ClassName(),
			        OP_ADD,
				journalEntry.get()));

			if ( connection()->send(jm.get()) ) {
				SEISCOMP_DEBUG("Sent origin journal entry for origin %s to the message group: %s",
				               scorigin->publicID().c_str(), primaryMessagingGroup().c_str());
			}
			else {
				SEISCOMP_ERROR("Sending of origin journal entry failed with error: %s",
				               connection()->lastError().toString());
			}
		}

	}
	else {
		SEISCOMP_INFO_S("Sent origin " + scorigin->publicID());
	}

//	SEISCOMP_INFO_S(Autoloc::printOrigin(origin, false));

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
}  // namespace Autoloc

}  // namespace Seiscomp
