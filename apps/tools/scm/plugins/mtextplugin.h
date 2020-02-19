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

#ifndef SEISCOMP_APPLICATIONS_MTEXTPLUGIN_CPP___
#define SEISCOMP_APPLICATIONS_MTEXTPLUGIN_CPP___

#include <map>
#include <string>

#include <seiscomp/plugins/monitor/monitoroutplugininterface.h>


namespace Seiscomp {
namespace Applications {

DEFINE_SMARTPOINTER(MTextPlugin);

class MTextPlugin : public MonitorOutPluginInterface {
	DECLARE_SC_CLASS(MTextPlugin);

	// ----------------------------------------------------------------------
	// Nested types
	// ----------------------------------------------------------------------
	private:
		typedef std::map<std::string, std::string> Clients;

	// ----------------------------------------------------------------------
	// X'struction
	// ----------------------------------------------------------------------
	public:
		MTextPlugin();
		virtual ~MTextPlugin() {}

	// ----------------------------------------------------------------------
	// Public interface
	// ----------------------------------------------------------------------
	public:
		virtual bool print(const ClientTable& table);
		virtual bool initOut(const Config::Config& cfg);
		virtual bool deinitOut();
		virtual bool refreshOut();
		virtual bool clearOut();


	private:
		bool        _init;
		std::string _descrFileName;
		std::string _outputDir;
		Clients     _clients;
};

} // namespace Applications
} // namespace Seiscomp


#endif
