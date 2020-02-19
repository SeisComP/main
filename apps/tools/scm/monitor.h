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

#ifndef SEISCOMP_APPLICATIONS_MONITOR_H__
#define SEISCOMP_APPLICATIONS_MONITOR_H__

#include <iostream>
#include <vector>
#include <map>
#include <string>

#include <boost/utility.hpp>
#include <boost/lexical_cast.hpp>

#include <seiscomp/client/application.h>
#include <seiscomp/core/datetime.h>
#include <seiscomp/plugins/monitor/monitoroutplugininterface.h>
#include <seiscomp/plugins/monitor/monitorplugininterface.h>
#include <seiscomp/plugins/monitor/types.h>


namespace Seiscomp {
namespace Applications {


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
class Monitor : public Client::Application {

	private:
		typedef std::map<std::string, Core::Time>                       ResponseTimes;


	// ------------------------------------------------------------------
	// X'struction
	// ------------------------------------------------------------------
	public:
		Monitor(int argc, char** argv);
		~Monitor();


	// ------------------------------------------------------------------
	// Public Interface
	// ------------------------------------------------------------------
	public:
		virtual bool init();
		virtual void handleNetworkMessage(const Client::Packet* msg);
		virtual void handleDisconnect();

		// Console output specific member functions
		void update();


	// ------------------------------------------------------------------
	// Private Interface
	// ------------------------------------------------------------------
	private:
		virtual void createCommandLineDescription();
		virtual bool validateParameters();
		virtual void handleTimeout();

		void printClientInfoTags() const;
		void handleStatusMessage(const Client::Packet *msg);


	// ------------------------------------------------------------------
	// Private Members
	// ------------------------------------------------------------------
	private:
		std::string                               _clientsToConsiderStr;
		std::vector<std::string>                  _clientsToConsider;
		ClientTable                               _clientTable;
		ResponseTimes                             _responseTimes;
		std::vector<MonitorPluginInterfacePtr>    _plugins;
		std::vector<MonitorOutPluginInterfacePtr> _outPlugins;
		unsigned int                              _refreshInterval;

};
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




} // namespace Application
} // namespace Seiscomp

#endif

