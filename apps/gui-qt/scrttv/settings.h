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

#ifndef SEISCOMP_GUI_SCRTTV_CONFIG
#define SEISCOMP_GUI_SCRTTV_CONFIG


#include <seiscomp/system/application.h>


namespace Seiscomp {
namespace Applications {
namespace TraceView {


struct Settings : Seiscomp::System::Application::AbstractSettings {
	void accept(Seiscomp::System::Application::SettingsLinker &linker) {
		linker
		& cli(
			inputFile,
			"Options", "input-file,i",
			"Load picks in given XML file during startup"
		)
		& cli(
			filter,
			"Mode", "filter",
			"Sets the filter to use"
		)
		& cliSwitch(
			offline,
			"Mode", "offline",
			"Do not connect to a messaging server and do not use the database"
		)
		& cliSwitch(
			inventoryDisabled,
			"Mode", "no-inventory",
			"Do not wait for all data when using a file as input but read threaded"
		)
		& cli(
			endTime,
			"Mode", "end-time",
			"Set the end time of acquisition, default: 'gmt'"
		)
		& cli(
			bufferSize,
			"Mode", "buffer-size",
			"Sets the size of the waveformbuffer in seconds)",
			true
		)
		& cli(
			maxDelay,
			"Mode", "max-delay",
			"The maximum delay in seconds to keep a trace visible (0 to disable). "
			"Channels with larger delays will be hidden until the delay is in range."
		)
		& cliSwitch(
			initStartTime,
			"Mode", "start-at-now",
			"Subscribe to data starting at now rather than now - bufferSize"
		)
		& cliSwitch(
			disableTimeWindowRequest,
			"Mode", "rt",
			"Don't ask for time window at data server. This might be important "
			"if e.g. Seedlink does not allow time window extraction."
		)
		& cliSwitch(
			mapPicksToBestMatchingTrace,
			"Mode", "map-picks",
			"Map picks to best matching rows. This is important "
			"if picks created on BHN should be shown but only the BHZ trace "
			"is part of the list. Network code, station code and location code "
			"must match anyway."
		)
		& cliSwitch(
			threeComponents,
			"Mode", "3c",
			"Do not only show the vertical component of the detecStream but all three "
			"components if available."
		)

		& cfg(filters, "filter") // Deprecated for backward compatibility
		& cfg(filters, "filters")
		& cfg(maxDelay, "maxDelay")
		& cfg(bufferSize, "bufferSize")
		& cfg(initStartTime, "startAtNow")
		& cfg(automaticSortEnabled, "resortAutomatically")
		& cfg(showPicks, "showPicks")
		& cfg(mapPicksToBestMatchingTrace, "mapPicks")
		& cfg(autoApplyFilter, "autoApplyFilter")
		& cfg(warnDataTimeRange, "warnDataTimeRange")
		& cfg(threeComponents, "3c")

		& cfg(groupConfig, "messaging.groups.config")
		& cfg(groupLocation, "messaging.groups.location")

		& cfg(defaultLocator, "associator.defaultLocator")
		& cfg(defaultEarthModel, "associator.defaultLocatorProfile")
		& cfg(depths, "associator.fixedDepths")

		& cfg(vstreams, "streams.codes")
		& cli(
		    vstreams,
		    "Mode", "channels",
		    "Channel(s) to display. The channel code may contain wildcards at "
		    "any position but the support of wildcards depends on RecordStream. "
		    "Repeat the option for multiple channel groups. "
		    "Examples:\n"
		    "default : all streams configured by global bindings.\n"
		    "GE.*.*.HH? : all HH streams of all stations from GE network.",
		    true
		);
	}

	int                      bufferSize{1800};
	Core::Time               endTime;
	std::string              inputFile;
	std::string              filter;
	std::vector<std::string> filters{{"RMHP(2)>>ITAPER(5)>>BW(3, 0.5, 8.0)"},{"RMHP(2)>>ITAPER(5)>>BW_HP(3, 3)"}};
	std::vector<std::string> vstreams{{"default"}};
	bool                     offline{false};
	bool                     initStartTime{false};
	bool                     autoApplyFilter{false};
	bool                     automaticSortEnabled{true};
	bool                     showPicks{true};
	bool                     mapPicksToBestMatchingTrace{false};
	bool                     inventoryDisabled{false};
	bool                     disableTimeWindowRequest{false};
	bool                     threeComponents{false};
	int                      maxDelay{0};
	int                      warnDataTimeRange{3600 * 6}; // 6 hours

	std::string              groupConfig{"CONFIG"};
	std::string              groupLocation{"LOCATION"};

	std::string              defaultLocator;
	std::string              defaultEarthModel;
	std::vector<double>      depths{0, 10, 18};

	static Settings global;
};


}
}
}


#endif
