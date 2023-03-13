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
			"The maximum delay in seconds to keep a trace visible (0 to disable)"
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

		& cfg(filters, "filter") // Deprecated for backward compatibility
		& cfg(filters, "filters")
		& cfg(maxDelay, "maxDelay")
		& cfg(bufferSize, "bufferSize")
		& cfg(initStartTime, "startAtNow")
		& cfg(automaticSortEnabled, "resortAutomatically")
		& cfg(showPicks, "showPicks")
		& cfg(autoApplyFilter, "autoApplyFilter")
		& cfg(warnDataTimeRange, "warnDataTimeRange")

		& cfg(groupConfig, "messaging.groups.config")
		& cfg(groupLocation, "messaging.groups.location")

		& cfg(defaultLocator, "defaultLocator")
		& cfg(defaultEarthModel, "defaultLocatorProfile")
		& cfg(depths, "fixedDepths");
	}

	int                      bufferSize{1800};
	Core::Time               endTime;
	std::string              filter;
	std::vector<std::string> filters{{"RMHP(2)>>ITAPER(5)>>BW(3, 0.5, 8.0)"}};
	bool                     offline{false};
	bool                     initStartTime{false};
	bool                     autoApplyFilter{false};
	bool                     automaticSortEnabled{true};
	bool                     showPicks{true};
	bool                     inventoryDisabled{false};
	bool                     disableTimeWindowRequest{false};
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
