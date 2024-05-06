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




#ifndef SEISCOMP_APPLICATIONS_LOCATOR__
#define SEISCOMP_APPLICATIONS_LOCATOR__

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

namespace Autoloc {


class App : public Client::Application, protected ::Autoloc::Autoloc3
{
	public:
		App(int argc, char **argv);
		~App() override = default;

	public:
		bool feed(DataModel::Pick*);
		bool feed(DataModel::Amplitude*);
		bool feed(DataModel::Origin*);
		virtual void printUsage() const override;

	protected:
		void createCommandLineDescription() override;
		bool validateParameters() override;
		bool initConfiguration() override;
		bool initInventory();
		// initialize one station at runtime
		bool initOneStation(const DataModel::WaveformStreamID&, const Core::Time&);

		void readHistoricEvents();

		bool init() override;
		bool run() override;
		void done() override;

		void handleMessage(Core::Message* msg) override;
		void handleTimeout() override;
		void handleAutoShutdown() override;

		void addObject(const std::string& parentID, DataModel::Object *o) override;

		bool _report(const ::Autoloc::Origin *origin) override;
//		bool runFromPickFile();
		bool runFromXMLFile(const char *fname);
		bool runFromEPFile(const char *fname);

		void sync(const Core::Time &time);
		const Core::Time now() const;
		void timeStamp() const;

	protected:
//		DataModel::Origin *convertToSC  (const ::Autoloc::Origin* origin, bool allPhases=true);
		::Autoloc::Origin *convertFromSC(const DataModel::Origin* scorigin);
		::Autoloc::Pick   *convertFromSC(const DataModel::Pick*   scpick);

	private:
		std::string _inputFileXML; // for XML playback
		std::string _inputEPFile;  // for offline processing
		std::string _stationLocationFile;
		std::string _gridConfigFile{"@DATADIR@/scautoloc/grid.conf"};
		std::string _amplTypeAbs{"mb"};
		std::string _amplTypeSNR{"snr"};

		// sorted objects for playback
		std::queue<DataModel::PublicObjectPtr> _objects;

		double _playbackSpeed;
		Core::Time playbackStartTime;
		Core::Time objectsStartTime;
		Core::Time syncTime;
		size_t objectCount;

		DataModel::EventParametersPtr _ep;
		DataModel::InventoryPtr inventory;

		::Autoloc::Autoloc3::Config _config;
		int _keepEventsTimeSpan;
		int _wakeUpTimout;

		ObjectLog *_inputPicks;
		ObjectLog *_inputAmps;
		ObjectLog *_inputOrgs;
		ObjectLog *_outputOrgs;
};


}

}

}

#endif
