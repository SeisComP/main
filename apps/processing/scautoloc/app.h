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

	private:
		// Processing
		bool feed(Seiscomp::DataModel::Pick *scpick);
		bool feed(Seiscomp::DataModel::Amplitude *scamplitude);
		bool feed(Seiscomp::DataModel::Origin *scorigin);

		// Receive SeisComP objects from messaging
		void addObject(const std::string& parentID, Seiscomp::DataModel::Object *o) override;

		// This is the output handler. It takes an origin and depending
		// on the mode of operation (online vs. playback) it sends it
		// to the messaging or adds it to an EventParameters instance
		// that is finally written to an XML file.
		bool _report(Seiscomp::DataModel::Origin *scorigin) override;

		void handleMessage(Seiscomp::Core::Message* msg) override;
		void handleTimeout() override;

		virtual void printUsage() const override;

	private:
		// SeisComP standard shutdown
		void handleAutoShutdown() override;
		void done() override;

	private:
		// Playback
		bool runFromXMLFile(const char *fname);
		bool runFromEPFile(const char *fname);
		double _playbackSpeed {1.};
		// Input XML files for playback and offline processing
		std::string _inputFileXML;
		std::string _inputEPFile;
		// Enable formatted XML output
		bool _formatted{false};
		// Sorted objects for playback
		std::queue<Seiscomp::DataModel::PublicObjectPtr> _objects;
		Seiscomp::DataModel::EventParametersPtr _ep;
		Seiscomp::Core::Time playbackStartTime;
		Seiscomp::Core::Time objectsStartTime;
		Seiscomp::Core::Time syncTime;

	private:
		Autoloc::AutolocConfig _config;

		size_t objectCount {0};

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
