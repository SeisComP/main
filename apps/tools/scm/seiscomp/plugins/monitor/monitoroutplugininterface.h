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


#ifndef SEISCOMP_APPLICATIONS_MONITOROUTPLUGININTERFACE_H__
#define SEISCOMP_APPLICATIONS_MONITOROUTPLUGININTERFACE_H__

#include <string>

#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/interfacefactory.h>
#include <seiscomp/config/config.h>
#include <seiscomp/plugins/monitor/api.h>
#include <seiscomp/plugins/monitor/types.h>


namespace Seiscomp {
namespace Applications {


DEFINE_SMARTPOINTER(MonitorOutPluginInterface);


class SC_MPLUGIN_API MonitorOutPluginInterface : public Seiscomp::Core::BaseObject {
	DECLARE_SC_CLASS(MonitorOutPluginInterface);

	// ----------------------------------------------------------------------
	// X'struction
	// ----------------------------------------------------------------------
	public:
		MonitorOutPluginInterface(const std::string& name);
		virtual ~MonitorOutPluginInterface() {}

	// ----------------------------------------------------------------------
	// Public interface
	// ----------------------------------------------------------------------
	public:
		virtual bool initOut(const Config::Config &cfg) = 0;
		virtual bool deinitOut() = 0;
		virtual bool print(const ClientTable& table) = 0;
		virtual bool refreshOut() = 0;
		virtual bool clearOut() = 0;

		const std::string& name() const;

		static MonitorOutPluginInterface* Create(const std::string& service);

	// ----------------------------------------------------------------------
	// Private data members
	// ---------------------------------------------------------------------
	private:
		std::string _name;

};


DEFINE_INTERFACE_FACTORY(MonitorOutPluginInterface);

#define REGISTER_MONITOR_OUT_PLUGIN_INTERFACE(Class, Service) \
Seiscomp::Core::Generic::InterfaceFactory<Seiscomp::Applications::MonitorOutPluginInterface, Class> __##Class##InterfaceFactory__(Service)

} // namespace Applications
} // namespace Seiscomp

#endif
