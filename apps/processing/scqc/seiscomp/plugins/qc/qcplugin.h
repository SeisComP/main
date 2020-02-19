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


#ifndef SEISCOMP_QC_QCPLUGIN_H__
#define SEISCOMP_QC_QCPLUGIN_H__

#include <seiscomp/processing/application.h>
#include <seiscomp/qc/qcprocessor.h>
#include <seiscomp/core/interfacefactory.h>
#include <seiscomp/core/plugin.h>
#include <seiscomp/utils/timer.h>
#include <seiscomp/plugins/qc/api.h>
#include <seiscomp/datamodel/waveformquality.h>

#include <queue>
#include <boost/version.hpp>
#include <boost/thread.hpp>
#include <boost/signals2.hpp>


namespace bsig = boost::signals2;


namespace Seiscomp {
namespace Applications {
namespace Qc {


class QcMessenger;
class QcConfig;
class QcBuffer;

DEFINE_SMARTPOINTER(QcBuffer);

SC_QCPLUGIN_API DataModel::WaveformStreamID getWaveformID(const std::string &streamID);

class SC_QCPLUGIN_API QcApp : public Processing::Application {
	public:
		QcApp(int argc, char **argv) : Processing::Application(argc, argv) {}

		virtual bool exitRequested() const { return false; }
		virtual const QcConfig* qcConfig() const { return NULL; }
		virtual QcMessenger* qcMessenger() const { return NULL; }

		typedef bsig::signal<void()> TimerSignal;
		virtual void addTimeout(const TimerSignal::slot_type& onTimeout) const {}
		virtual bool archiveMode() const { return false; }
		virtual std::string creatorID() const = 0;

 		TimerSignal doneSignal;
};


DEFINE_SMARTPOINTER(QcPlugin);

class QcPlugin : public Processing::QcProcessorObserver {
	DECLARE_SC_CLASS(QcPlugin)

	public:

		QcPlugin();
		virtual ~QcPlugin();

	public:
		//! Initialize all needed ressources
		virtual bool init(QcApp* app, QcConfig *cfg, std::string streamID);

		//! Called from the corresponding QcProcessor to propagate news
		virtual void update();

		//! Returns the plugin specific name given to plugin registry
		virtual std::string registeredName() const = 0;

		//! Returns the plugin specific parameter names
		virtual std::vector<std::string> parameterNames() const = 0;

		//! Returns the corresponding QcProcessor object
		Processing::QcProcessor* qcProcessor();

		//! Finish the work
		void done();
	
	protected:
		void onTimeout();
		virtual void timeoutTask();

		virtual double mean(const QcBuffer* qcb) const;
		virtual double stdDev(const QcBuffer* qcb, double mean) const;

		virtual void generateNullReport() const;
		virtual void generateReport(const QcBuffer* reportBuffer) const;
		virtual void generateAlert(const QcBuffer* staBuffer, const QcBuffer* ltaBuffer) const;

		//! collect objects to be send to qcMessenger
		void pushObject(DataModel::Object* obj) const;

		//! pass the collected objects to qcMessenger
		//! false = data messages; true = notifier messages
		void sendObjects(bool notifier = false);

		void sendMessages(const Core::Time &rectime);

		mutable std::queue<DataModel::ObjectPtr> _objects;
		std::string _name;
		std::vector<std::string> _parameterNames;
		std::string _streamID;
		QcApp* _app;
		QcMessenger* _qcMessenger;
		const QcConfig* _qcConfig;
		mutable QcBufferPtr _qcBuffer;
		Processing::QcProcessorPtr _qcProcessor;

	private:
		mutable Core::Time _lastArchiveTime;
		mutable Core::Time _lastReportTime;
		mutable Core::Time _lastAlertTime;
		mutable bool _firstRecord;
		mutable Util::StopWatch _timer;
};


typedef std::vector<QcPluginPtr> QcPlugins;

DEFINE_INTERFACE_FACTORY(QcPlugin);


}
}
}


#define REGISTER_QCPLUGIN(Class, Service) \
Seiscomp::Core::Generic::InterfaceFactory<Seiscomp::Applications::Qc::QcPlugin, Class> __##Class##InterfaceFactory__(Service)


#endif
