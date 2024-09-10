/***************************************************************************
 * Copyright (C) gempa GmbH                                                *
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


#define SEISCOMP_COMPONENT repicker

#include <seiscomp/logging/log.h>
#include <seiscomp/io/archive/xmlarchive.h>
#include <seiscomp/datamodel/inventory_package.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/parameterset.h>
#include <seiscomp/datamodel/utils.h>
#include <seiscomp/io/recordstream.h>
#include <seiscomp/utils/keyvalues.h>
#include <seiscomp/utils/misc.h>

#include <iostream>

#include "repicker.h"


using namespace std;
using namespace Seiscomp::DataModel;


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
namespace Seiscomp {
namespace Applications {
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Repicker::Settings::accept(SettingsLinker &linker) {

	linker & cfg(epFile, "ep");
	linker & cliAsPath(
		epFile,
		"Input", "ep",
		"Name of input XML file (SCML) with all picks for offline processing."
	);

	linker & cfg(pickerInterface, "picker");
	linker & cli(
		pickerInterface, "Picker",
		"picker,P", "Picker interface to be used for repicking.",
		true
	);

	linker & cfg(anyPhase, "anyPhase");
	linker & cliSwitch(
		anyPhase, "Picker",
		"any-phase,A", "Allow any phase to be repicked and not just P."
	);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Repicker::Repicker(int argc, char **argv)
: Client::Application(argc, argv) {
	setMessagingEnabled(false);
	setDatabaseEnabled(true, false);
	setLoadConfigModuleEnabled(true);
	setLoadStationsEnabled(true);
	setRecordStreamEnabled(true);
	setLoggingToStdErr(true);
	bindSettings(&_settings);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Repicker::printUsage() const {
	cout << name() << " < STDIN > STDOUT" << endl;
	Client::Application::printUsage();
	cout << "Examples:" << endl;
	cout << "Repick waveforms provided in a file based on picks from stdin" << endl;
	cout << "  screpick -d localhost -I data.mseed < picks.xml > picks_repick.xml"
	     << endl << endl;
	cout << "Repick waveforms provided by stdin based on picks from a XML file provied by a file" << endl;
	cout << "  cat *.mseed | screpick -d localhost -I - --ep picks.xml > picks_repick.xml" << endl;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Repicker::validateParameters() {
	if ( !Client::Application::validateParameters() ) {
		return false;
	}

	if ( !isConfigDatabaseEnabled() && !isInventoryDatabaseEnabled() ) {
		setDatabaseEnabled(false, false);
	}

	if ( _settings.pickerInterface.empty() ) {
		SEISCOMP_ERROR("No picker interface defined.");
		return false;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Repicker::init() {
	if ( !Client::Application::init() ) {
		return false;
	}

	_picker = Processing::PickerFactory::Create(_settings.pickerInterface);
	if ( !_picker ) {
		SEISCOMP_ERROR("Picker interface '%s' is not available.",
		               _settings.pickerInterface.c_str());
		return false;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Repicker::run() {
	EventParametersPtr ep;
	IO::XMLArchive ar;

	if ( !_settings.epFile.empty() ) {
		if ( !ar.open(_settings.epFile.c_str()) ) {
			cerr << "failed to open event parameter file: " << _settings.epFile << endl;
			return false;
		}
	}
	else {
		ar.open("-");
	}

	ar >> ep;
	if ( !ep ) {
		SEISCOMP_ERROR("No event parameters found");
		return false;
	}

	size_t processedPicks = 0;
	size_t failedPicks = 0;

	for ( size_t i = 0; i < ep->pickCount() && !isExitRequested(); ++i ) {
		auto pick = ep->pick(i);
		const auto &wid = pick->waveformID();

		if ( !_settings.anyPhase ) {
			try {
				if ( Util::getShortPhaseName(pick->phaseHint().code()) != 'P' ) {
					SEISCOMP_WARNING("%s: invalid phase '%s'",
					                 pick->publicID(), pick->phaseHint().code());
					++failedPicks;
					continue;
				}
			}
			catch ( ... ) {
				// No set phase is OK
			}
		}

		_picker->reset();
		_picker->setTrigger(pick->time().value());

		bool setupDisabled = false;
		ConfigStation *config = nullptr;

		for ( size_t cs = 0; cs < configModule()->configStationCount(); ++cs ) {
			auto configStation = configModule()->configStation(cs);
			if ( configStation->networkCode() != wid.networkCode() ) {
				continue;
			}

			if ( configStation->stationCode() != wid.stationCode() ) {
				continue;
			}

			if ( !configStation->enabled() ) {
				setupDisabled = true;
			}
			else {
				config = configStation;
				break;
			}
		}

		Util::KeyValues keys;

		if ( !config ) {
			if ( setupDisabled ) {
				SEISCOMP_WARNING("%s.%s: station setup is disabled, ignoring pick %s",
				                 wid.networkCode(), wid.stationCode(),
				                 pick->publicID());
				++failedPicks;
				continue;
			}
		}
		else {
			auto setup = findSetup(config, name(), true);
			if ( setup ) {
				keys.init(ParameterSet::Find(setup->parameterSetID()));
			}
		}

		Processing::Settings procSettings(
			configModuleName(),
			wid.networkCode(), wid.stationCode(),
			wid.locationCode(), wid.channelCode(),
			&configuration(), &keys
		);

		auto loc = Client::Inventory::Instance()->getSensorLocation(pick);
		if ( !loc ) {
			SEISCOMP_WARNING("%s: no sensor location found in inventory: %s.%s.%s",
			                 pick->publicID(), wid.networkCode(),
			                 wid.stationCode(), wid.locationCode());
			++failedPicks;
			continue;
		}

		ThreeComponents tc;
		getThreeComponents(tc, loc,
		                   wid.channelCode().substr(0, 2).c_str(),
		                   pick->time().value());

		for ( size_t i = 0; i < 3; ++i ) {
			if ( tc.comps[i] ) {
				_picker->streamConfig(static_cast<Processing::WaveformProcessor::Component>(i)).init(
					wid.networkCode(), wid.stationCode(),
					wid.locationCode(), tc.comps[i]->code(),
					pick->time().value()
				);
			}
		}

		if ( !_picker->setup(procSettings) ) {
			SEISCOMP_WARNING("%s: picker failed to initialize: %s (%f)",
			                 pick->publicID(),
			                 _picker->status().toString(),
			                 _picker->statusValue());
			++failedPicks;
			continue;
		}

		Processing::Picker::Result pickResult;
		bool gotPick = false;

		_picker->setPublishFunction([&pickResult, &gotPick](const Processing::Picker *, const Processing::Picker::Result &r) {
			pickResult = r;
			gotPick = true;
		});

		_picker->computeTimeWindow();

		if ( _picker->isFinished() ) {
			SEISCOMP_WARNING("%s: picker finished already: %s (%f)",
			                 pick->publicID(),
			                 _picker->status().toString(),
			                 _picker->statusValue());
			++failedPicks;
			continue;
		}

		auto rs = IO::RecordStream::Open(recordStreamURL().c_str());
		if ( !rs ) {
			SEISCOMP_ERROR("Failed to open recordstream: %s",
			               recordStreamURL());
			return false;
		}

		if ( !rs->setTimeWindow(_picker->timeWindow()) ) {
			SEISCOMP_WARNING("%s: picker failed to set data time window",
			                 pick->publicID());
			++failedPicks;
			continue;
		}

		switch ( _picker->usedComponent() ) {
			case Processing::WaveformProcessor::Vertical:
			case Processing::WaveformProcessor::FirstHorizontal:
			case Processing::WaveformProcessor::SecondHorizontal:
				// Add stream as indicated in the pick stream
				rs->addStream(wid.networkCode(), wid.stationCode(),
				              wid.locationCode(), wid.channelCode());
				break;
			case Processing::WaveformProcessor::Horizontal:
				// Use both horizontals
				if ( !tc.comps[Processing::WaveformProcessor::FirstHorizontalComponent] ||
				     !tc.comps[Processing::WaveformProcessor::SecondHorizontalComponent] ) {
					SEISCOMP_WARNING("%s: picker failed to initialize: meta data not found for two horizontals: %s.%s.%s.%s",
					                 pick->publicID(),
					                 _picker->status().toString(),
					                 _picker->statusValue(),
					                 wid.networkCode(), wid.stationCode(),
					                 wid.locationCode(), wid.channelCode().substr(0, 2));
					++failedPicks;
					continue;
				}
				rs->addStream(wid.networkCode(),
				              wid.stationCode(),
				              wid.locationCode(),
				              tc.comps[Processing::WaveformProcessor::FirstHorizontalComponent]->code());
				rs->addStream(wid.networkCode(),
				              wid.stationCode(),
				              wid.locationCode(),
				              tc.comps[Processing::WaveformProcessor::SecondHorizontalComponent]->code());
				break;
			case Processing::WaveformProcessor::Any:
				// Use all three components
				if ( !tc.comps[Processing::WaveformProcessor::FirstHorizontalComponent] ||
				     !tc.comps[Processing::WaveformProcessor::SecondHorizontalComponent] ||
				     !tc.comps[Processing::WaveformProcessor::VerticalComponent] ) {
					SEISCOMP_WARNING("%s: picker failed to initialize: meta data not found for three components",
					                 pick->publicID(),
					                 _picker->status().toString(),
					                 _picker->statusValue(),
					                 wid.networkCode(), wid.stationCode(),
					                 wid.locationCode(), wid.channelCode().substr(0, 2));
					++failedPicks;
					continue;
				}
				rs->addStream(wid.networkCode(),
				              wid.stationCode(),
				              wid.locationCode(),
				              tc.comps[Processing::WaveformProcessor::FirstHorizontalComponent]->code());
				rs->addStream(wid.networkCode(),
				              wid.stationCode(),
				              wid.locationCode(),
				              tc.comps[Processing::WaveformProcessor::SecondHorizontalComponent]->code());
				rs->addStream(wid.networkCode(),
				              wid.stationCode(),
				              wid.locationCode(),
				              tc.comps[Processing::WaveformProcessor::VerticalComponent]->code());
				break;
		}

		while ( auto rec = rs->next() ) {
			_picker->feed(rec);
			if ( _picker->isFinished() ) {
				break;
			}
		}

		if ( _picker->status() != Processing::WaveformProcessor::Finished ) {
			SEISCOMP_WARNING("%s: picker did not finish: %s (%f)",
			                 pick->publicID(),
			                 _picker->status().toString(),
			                 _picker->statusValue());
			++failedPicks;
			continue;
		}

		if ( gotPick ) {
			TimeQuantity time;
			time.setValue(pickResult.time);

			if ( pickResult.timeLowerUncertainty > 0 ) {
				time.setLowerUncertainty(pickResult.timeLowerUncertainty);
			}
			if ( pickResult.timeUpperUncertainty > 0 ) {
				time.setUpperUncertainty(pickResult.timeUpperUncertainty);
			}

			pick->setTime(time);
			pick->setMethodID(_picker->methodID());
			pick->setFilterID(_picker->filterID());

			if ( pickResult.slowness ) {
				pick->setHorizontalSlowness(RealQuantity(*pickResult.slowness));
			}

			if ( pickResult.backAzimuth ) {
				pick->setBackazimuth(RealQuantity(*pickResult.backAzimuth));
			}

			if ( pickResult.polarity ) {
				switch ( *pickResult.polarity ) {
					case Processing::Picker::POSITIVE:
						pick->setPolarity(PickPolarity(POSITIVE));
						break;
					case Processing::Picker::NEGATIVE:
						pick->setPolarity(PickPolarity(NEGATIVE));
						break;
					case Processing::Picker::UNDECIDABLE:
						pick->setPolarity(PickPolarity(UNDECIDABLE));
						break;
				}
			}
		}

		++processedPicks;
	}

	if ( isExitRequested() ) {
		cerr << "Aborted processing" << endl;
		return false;
	}

	cerr << "Processed picks: " << processedPicks << endl;
	cerr << "Failed picks: " << failedPicks << endl;

	ar.create("-");
	ar.setFormattedOutput(true);
	ar << ep;

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
