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


#ifndef SEISCOMP_QCMESSENGER_H__
#define SEISCOMP_QCMESSENGER_H__

#include <string>
#include <list>

#include <seiscomp/core/exceptions.h>
#include <seiscomp/core/message.h>
#include <seiscomp/utils/timer.h>
#include <seiscomp/plugins/qc/api.h>
#include <seiscomp/messaging/connection.h>
#include <seiscomp/datamodel/object.h>
#include <seiscomp/datamodel/waveformquality.h>
#include <seiscomp/datamodel/notifier.h>
#include <seiscomp/core/datamessage.h>
#include <seiscomp/plugins/qc/qcplugin.h>

#include <boost/thread.hpp>

namespace Seiscomp {
namespace Applications {
namespace Qc {

using namespace std;
using namespace Seiscomp;
using namespace Seiscomp::Core;
using namespace Seiscomp::Client;
using namespace Seiscomp::DataModel;


class SC_QCPLUGIN_API ConfigException: public GeneralException {
	public:
		ConfigException(): GeneralException("config exception") {}
		ConfigException(const string& what): GeneralException(what) {}
};

class SC_QCPLUGIN_API ConnectionException: public GeneralException {
	public:
	ConnectionException(): GeneralException("connection exception") {}
	ConnectionException(const string& what): GeneralException(what) {}
};

class SC_QCPLUGIN_API DatabaseException: public GeneralException {
	public:
	DatabaseException(): GeneralException("database exception") {}
	DatabaseException(const string& what): GeneralException(what) {}
};


struct SC_QCPLUGIN_API QcIndex {
	QcIndex() {}
	QcIndex(const std::string &k, const Core::Time &s) : key(k), startTime(s) {}

	std::string key;
	Core::Time  startTime;
};


class SC_QCPLUGIN_API QcIndexMap {
	public:
		QcIndexMap() {}

		bool find(const QcIndex &idx) const {
			std::map<std::string, Core::Time>::const_iterator it;
			it = _indexMap.find(idx.key);
			return it != _indexMap.end() && it->second == idx.startTime;
		}

		void insert(const QcIndex &idx) {
			_indexMap[idx.key] = idx.startTime;
		}

		size_t size() const {
			return _indexMap.size();
		}
	
	private:
		std::map<std::string, Core::Time> _indexMap;
};


DEFINE_SMARTPOINTER(QcMessenger);

class SC_QCPLUGIN_API QcMessenger : public Core::BaseObject {
	public:
		//! Initializing Constructor
		QcMessenger(QcApp* app);
		
		//! Attach object to message and schedule sending 
		//! (if notifier is true send as notifier message; as data message otherwise)
		bool attachObject(DataModel::Object* obj, bool notifier, Operation operation=OP_UNDEFINED);

		//! Scheduler for sending messages (called periodically by application)
		void scheduler();

		//! Send Qc Message
		bool sendMessage(Message* msg);

		void flushMessages();

	private:
		QcIndexMap _qcIndex;
		NotifierMessagePtr _notifierMsg;
		DataMessagePtr _dataMsg;
		QcApp* _app;
		Core::TimeSpan _sendInterval;
		int _maxSize;
		Util::StopWatch _timer;

};


}
}
}

#endif
