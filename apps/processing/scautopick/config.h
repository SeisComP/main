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


#ifndef APPS_PICKER_CONFIG_H
#define APPS_PICKER_CONFIG_H


#include <string>
#include <set>


namespace Seiscomp {

namespace Client {

	class Application;

}

namespace System {

	class CommandLine;

}

namespace Applications {
namespace Picker {


typedef std::set<std::string> StringSet;


class Config {
	public:
		Config() = default;

		void init(const Client::Application *app);
		void init(const System::CommandLine &config);

		std::string amplitudeGroup{"AMPLITUDE"};
		std::string phaseHint{"P"};

		// allow a comment added to the pick
		std::string commentID;
		std::string commentText;

		// Sets the test mode. When enabled no picks and
		// amplitudes are send via the messaging.
		bool        test{false};

		// Sets the offline mode. When enabled no database
		// is used to read available streams.
		bool        offline{false};
		bool        dumpRecords;

		// Create a picker for every stream data is received
		// for. This flag is set when the database is not used
		// (offline mode) and records are read from files unless
		// explicitly disabled.
		bool        useAllStreams{false};

		// Enables/disables amplitude calculation
		bool        calculateAmplitudes{true};

		// Enables/disables gap interpolation
		// When disabled the picker is going to be resetted
		// after a gap has been detected otherwise the gap
		// is going to be interpolated when is length is
		// under a certain length (see next parameter).
		bool        interpolateGaps{false};

		// The maximum gap length in seconds to handle
		// when interpolateGaps is set to true.
		// If the gap length is larget than this value the
		// picker will be resetted.
		double      maxGapLength{4.5};

		// The default channel to pick on for a station
		// without component code (BH, SH, ...).
		std::string defaultChannel{"BH"};

		// The default filter when no per station configuration
		// has been read.
		std::string defaultFilter{"RMHP(10)>>ITAPER(30)>>BW(4,0.7,2)>>STALTA(2,80)"};

		// The default threshold to trigger a pick.
		double      defaultTriggerOnThreshold{3.0};
		// The default threshold to enable triggering
		// again.
		double      defaultTriggerOffThreshold{1.5};

		// The dead time in seconds after a pick has been
		// triggered.
		double      triggerDeadTime{30.0};

		// The minimum duration of a trigger, negative value = disabled
		double      minDuration{-1};

		// The maximum duration of a trigger, negative value = disabled
		double      maxDuration{-1};

		// The timewindow in seconds to calculate a SNR amplitude.
		double      amplitudeMaxTimeWindow{10.0};

		// The minimum amplitude offset used in the formula to
		// calculate the minimum amplitude to reach when a
		// previous pick on a stream exists.
		// minAmplitude = amplitudeMinOffset + lastAmplitude * exp(-(trigger - lastTrigger)^2)
		double      amplitudeMinOffset{3.0};

		// The default time correction in seconds to apply
		// when a pick is going to be emitted.
		double      defaultTimeCorrection{-0.8};

		// The global record ringbuffer size in seconds.
		double      ringBufferSize{5. * 60.};

		// The timespan in seconds that will be substracted from
		// NOW to acquire records.
		double      leadTime{60.0};

		// The initialization time per stream in seconds after
		// the first records has been received. Within that time
		// the picker is disabled.
		double      initTime{60.0};

		// The amplitude types to calculate
		StringSet   amplitudeList{"MLv", "mb", "mB"};

		// The amplitude types that can be updated
		StringSet   amplitudeUpdateList;

		// The picker type to use
		std::string pickerType;

		// The secondary picker type to use
		std::string secondaryPickerType;

		// Feature extraction
		std::string featureExtractionType;

		// Whether kill previously started secondary pickers when a new
		// primary pick has been declared
		bool        killPendingSecondaryProcessors{true};

		// Send detections as well if a picker is configured?
		bool        sendDetections{false};

		bool        extraPickComments{false};

		// Accept historic data in real-time playbacks?
		bool        playback{false};

		bool        generateSimplifiedIDs{false};

	public:
		void dump() const;
};

}
}
}


#endif
