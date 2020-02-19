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

#ifndef SEISCOMP_APPLICATIONS_MONITORPLUGININTERFACE_H__
#define SEISCOMP_APPLICATIONS_MONITORPLUGININTERFACE_H__

#include <map>
#include <vector>
#include <string>

#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/interfacefactory.h>
#include <seiscomp/config/config.h>
#include <seiscomp/plugins/monitor/api.h>
#include <seiscomp/plugins/monitor/types.h>

namespace Seiscomp {
namespace Applications {


SC_MPLUGIN_API bool findName(ClientInfoData clientData, std::string name);


DEFINE_SMARTPOINTER(MonitorPluginInterface);

class MFilterParser;
class MFilterInterface;


class SC_MPLUGIN_API MonitorPluginInterface : public Seiscomp::Core::BaseObject {
	DECLARE_SC_CLASS(MonitorPluginInterface);

	// ----------------------------------------------------------------------
	// X'struction
	// ----------------------------------------------------------------------
	public:
		MonitorPluginInterface(const std::string& name);
		virtual ~MonitorPluginInterface();


	// ----------------------------------------------------------------------
	// Public interface
	// ----------------------------------------------------------------------
	public:
		static MonitorPluginInterface *Create(const std::string &service);

		virtual bool init(const Config::Config &cfg);

		virtual void process(const ClientTable &clientTable) = 0;

		bool initFilter(const Config::Config &cfg);
		bool operational() const;
		bool isFilteringEnabled() const;
		void setOperational(bool val);

		const std::string &filterString() const;
		const ClientTable *filter(const ClientTable& clientTable);
		const ClientTable *filterMean(const ClientTable& clientTable);
		void setFilterMeanInterval(double interval);

		const ClientTable *match() const;

		const std::string &name() const;


	// ----------------------------------------------------------------------
	// Private interface
	// ----------------------------------------------------------------------
	private:
		template <Client::Status::ETag tag>
		void sumData(ClientInfoData &lhs, const ClientInfoData &rhs);

		template <Client::Status::ETag tag>
		void calculateMean(ClientInfoData &lhs, size_t count);

	// ----------------------------------------------------------------------
	// Private members
	// ----------------------------------------------------------------------
	private:
		Core::TimeSpan                _filterMeanInterval;
		Core::Time                    _filterMeanTimeMark;
		ClientTable                   _filterMeanClientTable;
		std::map<std::string, size_t> _filterMeanMessageCount;

		ClientTable                   _match;
		std::string                   _name;
		bool                          _operational;
		bool                          _isFilteringEnabled;
		std::string                   _filterStr;
		MFilterParser                *_mFilterParser;
		MFilterInterface             *_filter;

};


DEFINE_INTERFACE_FACTORY(MonitorPluginInterface);


#define REGISTER_MONITOR_PLUGIN_INTERFACE(Class, Service) \
Seiscomp::Core::Generic::InterfaceFactory<Seiscomp::Applications::MonitorPluginInterface, Class> __##Class##InterfaceFactory__(Service)


} // namespace Applications
} // namespace Seiscomp

#endif
