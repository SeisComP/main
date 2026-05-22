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

		// Initialize the internal inventory from the SeisComP inventory
		bool initInventory();

		// Initialize one station at runtime
		bool initOneStation(const DataModel::WaveformStreamID&, const Core::Time&);

	private:
		// Read past events from the database
		void readHistoricEvents();

	private:
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

		// Add a time stamp generated using now() to the debug log
		void timeStamp() const;

	protected:
//		DataModel::Origin *convertToSC  (const Autoloc::Origin* origin, bool allPhases=true);
		Autoloc::Origin *convertFromSC(const DataModel::Origin* scorigin);
		Autoloc::Pick   *convertFromSC(const DataModel::Pick*   scpick);

	private:
		// Processing
		bool feed(DataModel::Pick*);
		bool feed(DataModel::Amplitude*);
		bool feed(DataModel::Origin*);

		// Receive SeisComP objects from messaging
		void addObject(const std::string& parentID, DataModel::Object *o) override;

		bool _report(const Autoloc::Origin *origin) override;

		void handleMessage(Core::Message* msg) override;
		void handleTimeout() override;

		virtual void printUsage() const override;

	private:
		// SeisComP standard shutdown
		void done() override;
		void handleAutoShutdown() override;

	private:
		std::string _inputFileXML; // for XML playback
		std::string _inputEPFile;  // for offline processing
		std::string _stationLocationFile;
		std::string _gridConfigFile{"@DATADIR@/scautoloc/grid.conf"};
		std::string _amplTypeAbs{"mb"};
		std::string _amplTypeSNR{"snr"};
		bool        _formatted{false};

		// sorted objects for playback
		std::queue<DataModel::PublicObjectPtr> _objects;

		double _playbackSpeed;
		Core::Time playbackStartTime;
		Core::Time objectsStartTime;
		Core::Time syncTime;
		size_t objectCount {0};

		DataModel::EventParametersPtr _ep;
		DataModel::InventoryPtr inventory;

		Autoloc::Autoloc3::Config _config;
		int _wakeUpTimout;

		ObjectLog *_inputPicks {nullptr};
		ObjectLog *_inputAmps  {nullptr};
		ObjectLog *_inputOrgs  {nullptr};
		ObjectLog *_outputOrgs {nullptr};
};


}

}

#endif
