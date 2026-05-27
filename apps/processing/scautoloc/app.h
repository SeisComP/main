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




#ifndef SEISCOMP_APPLICATIONS_AUTOLOC_APP_H
#define SEISCOMP_APPLICATIONS_AUTOLOC_APP_H

#include <queue>
#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/amplitude.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/waveformstreamid.h>
#include <seiscomp/client/application.h>
#include "autoloc.h"


namespace Seiscomp {

namespace Applications {


class AutolocApp : public Client::Application, protected Autoloc::Autoloc3
{
	public:
		AutolocApp(int argc, char **argv);
		~AutolocApp() override = default;

	private:
		// SeisComP standard startup
		void createCommandLineDescription() override;
		bool validateParameters() override;
		bool initConfiguration() override;
		bool init() override;
		bool run() override;

	private:
		// Read past events from the database
		void readHistoricEvents();

		// Playback
		bool runFromXMLFile(const char *fname);
		bool runFromEPFile(const char *fname);

		void sync(const Core::Time &time);

		// Return the current time.
		//
		// The current time is either
		// - the system time or
		// - a 'fake system time' from the creation times of the received objects
		// The latter is needed in playbacks.
		const Core::Time now() const;

		double _playbackSpeed {1.};

	private:
		// Processing
		bool feed(DataModel::Pick *scpick);
		bool feed(DataModel::Amplitude *scamplitude);
		bool feed(DataModel::Origin *scorigin);

		// Receive SeisComP objects from messaging
		void addObject(const std::string& parentID, DataModel::Object *o) override;

		bool _report(const Autoloc::Origin *origin) override;
		bool _report(DataModel::Origin *scorigin);  // not const

		void handleMessage(Core::Message* msg) override;
		void handleTimeout() override;

		virtual void printUsage() const override;

	private:
		// SeisComP standard shutdown
		void handleAutoShutdown() override;
		void done() override;

	private:
		Autoloc::AutolocConfig _config;

		std::string _inputFileXML; // for XML playback
		std::string _inputEPFile;  // for offline processing
		bool        _formatted{false};

		// sorted objects for playback
		std::queue<DataModel::PublicObjectPtr> _objects;

		Core::Time playbackStartTime;
		Core::Time objectsStartTime;
		Core::Time syncTime;
		size_t objectCount {0};

		DataModel::EventParametersPtr _ep;

		// Wake up every 5 seconds to check pending operations
		int _wakeUpTimout {5};

		ObjectLog *_inputPicks {nullptr};
		ObjectLog *_inputAmps  {nullptr};
		ObjectLog *_inputOrgs  {nullptr};
		ObjectLog *_outputOrgs {nullptr};
};


}

}

#endif
