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



#ifndef SEISCOMP_APPLICATIONS_IMEX_H__
#define SEISCOMP_APPLICATIONS_IMEX_H__

#include <string>
#include <list>
#include <map>
#include <utility>

#include <boost/shared_ptr.hpp>

#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/amplitude.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/event.h>
#include <seiscomp/client/application.h>
#include <seiscomp/core/datamessage.h>


namespace Seiscomp {
namespace Applications {



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
class ImExImpl;
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
class ImEx : public Client::Application {

	// ----------------------------------------------------------------------
	// Nested Types
	// ----------------------------------------------------------------------
	public:
		enum Mode { IMPORT = 0x0, EXPORT, UNDEFINED, Mode_COUNT };

		typedef std::list<DataModel::PickPtr>      PickList;
		typedef std::list<DataModel::AmplitudePtr> AmplitudeList;
		typedef std::list<DataModel::OriginPtr>    OriginList;
		typedef std::list<DataModel::EventPtr>     EventList;

	private:
		typedef std::list<std::pair<Core::Time, Core::MessagePtr> > MessageList;

	// ----------------------------------------------------------------------
	// X'struction
	// ----------------------------------------------------------------------
	public:
		ImEx(int argc, char* argv[]);
		~ImEx();

		const PickList       &pickList() const;
		const AmplitudeList  &amplitudeList() const;
		const OriginList     &originList() const;
		const Mode           &mode() const;
		const Core::TimeSpan &cleanUpInterval() const;


	protected:
		virtual bool init();
		virtual void handleNetworkMessage(const Client::Packet *msg);
		virtual void handleMessage(Core::Message* message);
		virtual void done();
		virtual bool validateParameters();
		virtual void createCommandLineDescription();

		// ------------------------------------------------------------------
		// Private interface
		// ------------------------------------------------------------------
	private:
		void updateData(DataModel::NotifierMessage *notifierMessage);
		void updateData(Core::DataMessage *dataMessage);
		void updateEventData(DataModel::Event *event);

		Core::MessagePtr convertMessage(Core::Message *message);
		void dispatchMessage(Core::Message *msg);

		template <typename T>
		void cleanUp(T &container);
		void cleanUp(EventList &container);
		void cleanUp(AmplitudeList &container);
		template <typename T>
		void cleanUpSpecial(T &container);


		// ----------------------------------------------------------------------
		// Public data members
		// ----------------------------------------------------------------------
	public:
		PickList       _pickList;
		AmplitudeList  _amplitudeList;
		OriginList     _originList;

		Mode           _mode;
		Core::TimeSpan _cleanUpInterval;


		// ------------------------------------------------------------------
		// Private data members
		// ------------------------------------------------------------------
	private:
		std::vector<boost::shared_ptr<ImExImpl> > _imexImpls;
		Core::Time                                _lastCleanUp;
		EventList                                 _eventList;

		Client::PacketCPtr                        _lastNetworkMessage;
		std::string                               _importMessageConversion;

};
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<





} // namepsace Applications
} // namespace Seiscomp

#endif
