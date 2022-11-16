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



#ifndef SEISCOMP_APPLICATIONS_IMPORT_H__
#define SEISCOMP_APPLICATIONS_IMPORT_H__


#include <map>
#include <string>
#include <thread>

#include <seiscomp/client/application.h>


namespace Seiscomp {
namespace Applications {


class Import : public Client::Application {
	// ----------------------------------------------------------------------
	// Nested types
	// ----------------------------------------------------------------------
	private:
		enum Mode {
			RELAY = 0x0,
			IMPORT
		};

	
	// ----------------------------------------------------------------------
	// X'struction
	// ----------------------------------------------------------------------
	public:
		Import(int argc, char* argv[]);
		~Import();

	
	// ------------------------------------------------------------------
	// Private interface
	// ------------------------------------------------------------------
	private:
		virtual bool init() override;
		virtual bool initConfiguration() override;
		virtual void createCommandLineDescription() override;
		virtual void handleNetworkMessage(const Client::Packet *packet) override;
		virtual void handleMessage(Core::Message *msg) override;
		virtual void done() override;

		Client::Result connectToSink(const std::string &sink);
		bool filterObject(Core::BaseObject *obj);
		bool buildImportRoutingtable();
		void buildRelayRoutingtable(bool routeUnknownGroup = false);
		void readSinkMessages();


	// ------------------------------------------------------------------
	// Private implementation
	// ------------------------------------------------------------------
	private:
		typedef std::map<std::string, std::string> RoutingTable;
		Client::ConnectionPtr  _sink;
		Client::PacketCPtr     _lastPacket;
		RoutingTable           _routingTable;
		std::thread           *_sinkMessageThread;

		bool                   _filter;
		bool                   _routeUnknownGroup;
		Mode                   _mode;
		bool                   _useSpecifiedGroups;
		bool                   _test;
};


} // namepsace Applications
} // namespace Seiscomp

#endif
