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


#define SEISCOMP_COMPONENT Autopick

#include <stdio.h>

#include <seiscomp/logging/log.h>
#include <seiscomp/client/application.h>

#include "config.h"


namespace Seiscomp {
namespace Applications {
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Picker::Config::init(const Client::Application *app) {
	try { amplitudeGroup = app->configGetString("connection.amplitudeGroup"); }
	catch ( ... ) {}

	try { phaseHint = app->configGetString("phaseHint"); }
	catch ( ... ) {}

	try { commentID = app->configGetString("comment.ID"); }
	catch ( ... ) {}

	try { commentText = app->configGetString("comment.text"); }
	catch ( ... ) {}

	try { calculateAmplitudes = app->configGetBool("calculateAmplitudes"); }
	catch (...) {}

	try { defaultFilter = app->configGetString("filter"); }
	catch (...) {}

	try { useAllStreams = app->configGetBool("useAllStreams"); }
	catch (...) {}

	try { defaultTimeCorrection = app->configGetDouble("timeCorrection"); }
	catch (...) {}
	try { ringBufferSize = app->configGetDouble("ringBufferSize"); }
	catch (...) {}
	try { leadTime = app->configGetDouble("leadTime"); }
	catch (...) {}
	try { initTime = app->configGetDouble("initTime"); }
	catch (...) {}
	try { interpolateGaps = app->configGetBool("gapInterpolation"); }
	catch (...) {}

	try { defaultTriggerOnThreshold = app->configGetDouble("thresholds.triggerOn"); }
	catch (...) {}
	try { defaultTriggerOffThreshold = app->configGetDouble("thresholds.triggerOff"); }
	catch (...) {}
	try { maxGapLength = app->configGetDouble("thresholds.maxGapLength"); }
	catch (...) {}
	try { triggerDeadTime = app->configGetDouble("thresholds.deadTime"); }
	catch (...) {}
	try { minDuration = app->configGetDouble("thresholds.minDuration"); }
	catch (...) {}
	try { maxDuration = app->configGetDouble("thresholds.maxDuration"); }
	catch (...) {}

	try { amplitudeMaxTimeWindow = app->configGetDouble("thresholds.amplMaxTimeWindow"); }
	catch (...) {}
	try { amplitudeMinOffset = app->configGetDouble("thresholds.minAmplOffset"); }
	catch (...) {}

	try {
		std::vector<std::string> amplitudes = app->configGetStrings("amplitudes");
		amplitudeList.clear();
		amplitudeList.insert(amplitudes.begin(), amplitudes.end());
	}
	catch (...) {}

	try {
		std::vector<std::string> amplitudes = app->configGetStrings("amplitudes.enableUpdate");
		amplitudeUpdateList.clear();
		amplitudeUpdateList.insert(amplitudes.begin(), amplitudes.end());
	}
	catch (...) {}

	try { pickerType = app->configGetString("picker"); }
	catch ( ... ) {}

	try { secondaryPickerType = app->configGetString("spicker"); }
	catch ( ... ) {}

	try { featureExtractionType = app->configGetString("fx"); }
	catch ( ... ) {}

	try { killPendingSecondaryProcessors = app->configGetBool("killPendingSPickers"); }
	catch ( ... ) {}

	try { sendDetections = app->configGetBool("sendDetections"); }
	catch ( ... ) {}

	try { extraPickComments = app->configGetBool("extraPickComments"); }
	catch ( ... ) {}

	try { playback = app->configGetBool("playback"); }
	catch ( ... ) {}

	try { generateSimplifiedIDs = app->configGetBool("simplifiedIDs"); }
	catch ( ... ) {}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Picker::Config::init(const System::CommandLine &commandline) {
	test = commandline.hasOption("test");
	offline = commandline.hasOption("offline") || commandline.hasOption("ep");
	dumpRecords = commandline.hasOption("dump-records");
	sendDetections = commandline.hasOption("send-detections") ? true : sendDetections;
	extraPickComments = commandline.hasOption("extra-comments") ? true : extraPickComments;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Picker::Config::dump() const {
	printf("Configuration:\n");
	printf("amplitude group                  %s\n",     amplitudeGroup.c_str());
	printf("testMode                         %s\n",     test ? "true":"false");
	printf("offline                          %s\n",     offline ? "true":"false");
	printf("useAllStreams                    %s\n",     useAllStreams ? "true":"false");
	printf("calculateAmplitudes              %s\n",     calculateAmplitudes ? "true":"false");
	printf("calculateAmplitudeTypes          ");
	if ( amplitudeList.empty() ) {
		printf("[]\n");
	}
	else {
		for ( StringSet::const_iterator it = amplitudeList.begin();
		      it != amplitudeList.end(); ++it ) {
			if ( it != amplitudeList.begin() ) {
				printf(", ");
			}
			printf("%s", it->c_str());
		}
		printf("\n");
	}

	printf("update amplitude types           ");
	if ( amplitudeUpdateList.empty() ) {
		printf("[]\n");
	}
	else {
		for ( StringSet::const_iterator it = amplitudeUpdateList.begin();
		      it != amplitudeUpdateList.end(); ++it ) {
			if ( it != amplitudeUpdateList.begin() ) {
				printf(", ");
			}
			printf("%s", it->c_str());
		}
		printf("\n");
	}
	printf("interpolateGaps                  %s\n",    interpolateGaps ? "true":"false");
	printf("maxGapLength                     %.2fs\n", maxGapLength);
	printf("defaultFilter                    %s\n",    defaultFilter.c_str());
	printf("defaultTriggerOnThreshold        %.2f\n",  defaultTriggerOnThreshold);
	printf("defaultTriggerOffThreshold       %.2fs\n", defaultTriggerOffThreshold);
	printf("minDuration                      %.2fs\n", minDuration);
	printf("maxDuration                      %.2f\n",  maxDuration);
	printf("triggerDeadTime                  %.2fs\n", triggerDeadTime);
	printf("amplitudeMaxTimeWindow           %.2fs\n", amplitudeMaxTimeWindow);
	printf("amplitudeMinOffset               %.2fs\n", amplitudeMinOffset);
	printf("defaultTimeCorrection            %.2fs\n", defaultTimeCorrection);
	printf("ringBufferSize                   %.0fs\n", ringBufferSize);
	printf("leadTime                         %.0fs\n", leadTime);
	printf("initTime                         %.0fs\n", initTime);
	printf("pickerType                       %s\n",    pickerType.c_str());
	printf("secondaryPickerType              %s\n",    secondaryPickerType.c_str());
	printf("killPendingSPickers              %s\n",    killPendingSecondaryProcessors ? "true" : "false");
	printf("sendDetections                   %s\n",    sendDetections ? "true" : "false");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
}
}
