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
#include <seiscomp/datamodel/inventory.h>
#include <seiscomp/client/application.h>

#include <seiscomp/autoloc/autoloc.h>


namespace Seiscomp {

namespace Autoloc {


class App :
	public Client::Application,
	// Derived from Autoloc3 mainly because we re-implement here
	// the _report() method to allow both XML and messaging output.
        protected Autoloc3
{
	public:
		App(int argc, char **argv);
		~App();

	private:
		bool feed(Seiscomp::DataModel::Pick*);
		bool feed(Seiscomp::DataModel::Amplitude*);
		bool feed(Seiscomp::DataModel::Origin*);

		void createCommandLineDescription();
		bool validateParameters();
		bool initConfiguration();
		bool initInventory();
		void readHistoricEvents();

		bool init();
		bool run();
		void done();

		void handleMessage(Seiscomp::Core::Message* msg);
		void handleTimeout();
		void handleAutoShutdown();

		void addObject(
			const std::string& parentID,
			Seiscomp::DataModel::Object*);
		void removeObject(
			const std::string& parentID,
			Seiscomp::DataModel::Object*);
		void updateObject(
			const std::string& parentID,
			Seiscomp::DataModel::Object*);

		// re-implemented to support XML and messaging output
		virtual bool _report(Seiscomp::DataModel::Origin*);

		bool runFromXMLFile(const char *fname);
		bool runFromEPFile(const char *fname);

		void sync(const Seiscomp::Core::Time &time);
		const Core::Time now() const;
		void timeStamp() const;

	private:
		std::string _inputFileXML; // for XML playback
		std::string _inputEPFile; // for offline processing
		std::string _stationLocationFile;
		std::string _gridConfigFile;

		// object queue used for XML playback
		std::queue<Seiscomp::DataModel::PublicObjectPtr> _objects;

		double _playbackSpeed;
		Core::Time playbackStartTime;
		Core::Time objectsStartTime;
		Core::Time syncTime;
		unsigned int objectCount;

		Seiscomp::DataModel::EventParametersPtr ep;
		Seiscomp::DataModel::InventoryPtr inventory;

		Config config;
		int _keepEventsTimeSpan;
		int _wakeUpTimout;

		ObjectLog *_inputPicks;
		ObjectLog *_inputAmps;
		ObjectLog *_inputOrgs;
		ObjectLog *_outputOrgs;
};


}  // namespace Autoloc

}  // namespace Seiscomp

#endif
