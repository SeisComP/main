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

#ifndef SEISCOMP_APPLICATIONS_EVENTPROCESSOR_H__
#define SEISCOMP_APPLICATIONS_EVENTPROCESSOR_H__


#include <string>
#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/interfacefactory.h>
#include <seiscomp/config/config.h>
#include <seiscomp/plugins/events/api.h>


namespace Seiscomp {


namespace DataModel {

class Event;

}


namespace Client {


DEFINE_SMARTPOINTER(EventProcessor);


class SC_EVPLUGIN_API EventProcessor : public Seiscomp::Core::BaseObject {
	// ----------------------------------------------------------------------
	// X'struction
	// ----------------------------------------------------------------------
	public:
		EventProcessor();


	// ----------------------------------------------------------------------
	// Virtual public interface
	// ----------------------------------------------------------------------
	public:
		//! Setup all configuration parameters
		virtual bool setup(const Config::Config &config) = 0;

		//! Processes an event. The preferred object (Origin, Magnitude,
		//! FocalMechanism) are guaranteed to be found with *::Find(id)
		//! methods.
		//! This method should return true if the event objects needs
		//! an update.
		virtual bool process(DataModel::Event *event) = 0;
};


DEFINE_INTERFACE_FACTORY(EventProcessor);


}
}


#define REGISTER_EVENTPROCESSOR(Class, Service) \
Seiscomp::Core::Generic::InterfaceFactory<Seiscomp::Client::EventProcessor, Class> __##Class##InterfaceFactory__(Service)


#endif
