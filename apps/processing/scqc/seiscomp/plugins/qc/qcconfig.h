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


#ifndef SEISCOMP_QC_QCCONFIG_H__
#define SEISCOMP_QC_QCCONFIG_H__

#include <seiscomp/core/exceptions.h>
#include <seiscomp/core/baseobject.h>
#include <seiscomp/plugins/qc/api.h>
#include <seiscomp/plugins/qc/qcplugin.h>

#include <string>
#include <vector>

namespace Seiscomp {
namespace Applications {
namespace Qc {


class SC_QCPLUGIN_API QcConfigException: public Core::GeneralException {
public:
	QcConfigException(): Core::GeneralException("Qc config exception") {}
	QcConfigException(const std::string &what): Core::GeneralException(what) {}
};


DEFINE_SMARTPOINTER(QcConfig);
class SC_QCPLUGIN_API QcConfig : public Core::BaseObject {
	DECLARE_SC_CLASS(QcConfig);

	public:
		//! default Constructor
		QcConfig();

		//! initializing Constructor
		QcConfig(QcApp *app, const std::string &pluginName="default");
	
		//! destructor
		virtual ~QcConfig();
	
		//! Read plugin specific configuration from config files. If not found,
		//! use plugin.defaults. If not found, use compiled-in default.
		std::string readConfig(const std::string& pluginName,
		                       const std::string& keyWord,
		                       const std::string& defaultValue) const;

		//! Returns true if configured in realtime only mode; false otherwise
		bool realtimeOnly() const;

		//! Returns the over all buffer length
		int buffer() const;

		//! Returns the archive interval time span
		int archiveInterval() const;

		//! Returns the configured archive buffer length
		//! if it is <= buffer length; buffer length otherwise
		int archiveBuffer() const;

		//! Returns the report interval time span
		int reportInterval() const;

		//! Returns the configured report buffer length
		//! if it is <= buffer length; buffer length otherwise
		int reportBuffer() const;

		//! Returns the configured timeout for realtime only processors
		//! throws exception in case of archive mode
		int reportTimeout() const;

		//! Returns the alert interval time span if in realtime processing mode
		//! throws exception in case of archive mode
		int alertInterval() const;

		//! Returns the configured report buffer length
		//! if it is <= buffer length; buffer length otherwise in realtime mode
		//! throws exception in case of archive mode
		int alertBuffer() const;

		//! Returns the thresholds triggering alarm in realtime mode
		//! throws exception in case of archive mode
		std::vector<int> alertThresholds() const;

		//! Returns true if configured in realtime only mode; false otherwise
		static bool RealtimeOnly(const QcApp *app, const std::string &pluginName);

	private:
		QcApp* _app;
		bool _realtime;
		int _buffer;
		int _archiveInterval;
		int _archiveBuffer;
		int _reportInterval;
		int _reportBuffer;
		int _reportTimeout;
		int _alertInterval;
		int _alertBuffer;
		std::vector<int> _alertThresholds;

		void setQcConfig(const std::string &pluginName);
};



}
}
}

#endif
