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


#ifndef SEISCOMP_APPLICATIONS_QCTOOL__
#define SEISCOMP_APPLICATIONS_QCTOOL__

#include <seiscomp/core/datetime.h>
#include <seiscomp/core/record.h>

#include <seiscomp/plugins/qc/qcmessenger.h>
#include <seiscomp/plugins/qc/qcplugin.h>
#include <seiscomp/plugins/qc/qcconfig.h>

#include <boost/version.hpp>
#include <boost/any.hpp>
#include <boost/signals2.hpp>

#include <string>
#include <set>
#include <map>


namespace bsig = boost::signals2;


namespace Seiscomp {
namespace Applications {
namespace Qc {


DEFINE_SMARTPOINTER(QcBuffer);


class QcTool : public QcApp, public bsig::trackable {
	public:
		using TimerSignal = bsig::signal<void()>;

	public:
		QcTool(int argc, char **argv);
		~QcTool();

		QcMessenger* qcMessenger() const override;
		bool exitRequested() const override;

		void addTimeout(const TimerSignal::slot_type& onTimeout) const override;
		bool archiveMode() const override;
		std::string creatorID() const override;


	protected:
		void createCommandLineDescription() override;
		bool validateParameters() override;
		bool initConfiguration() override;

		bool init() override;
		void done() override;

		void handleTimeout() override;
		void handleNewStream(const Record* rec) override;

	private:
		void addStream(std::string net, std::string sta, std::string loc, std::string cha);
		Core::Time findLast(std::string net, std::string sta, std::string loc, std::string cha);

		void initQc(const std::string& networkCode,
		            const std::string& stationCode,
		            const std::string& locationCode,
		            const std::string& channelCode);

		void processorFinished(const Record* rec, Processing::WaveformProcessor* wp) override;


	private:
		bool _archiveMode;
		bool _autoTime;
		Core::Time _beginTime;
		Core::Time _endTime;
		std::string _streamMask;

		bool _useConfiguredStreams;
		bool _use3Components;
		std::set<std::string> _streamIDs;

		std::string _creator;
		int _dbLookBack;
		std::map<std::string, QcConfigPtr> _plugins;
		std::set<std::string> _allParameterNames;

		double _maxGapLength;
		double _ringBufferSize;
		double _leadTime;

		QcMessengerPtr _qcMessenger;

		//! maps streamID's and associated qcPlugins
		using QcPluginMap = std::multimap<const std::string, QcPluginCPtr>;
		QcPluginMap _qcPluginMap;

		mutable TimerSignal _emitTimeout;
		Util::StopWatch _timer;
};


}
}
}


#endif
