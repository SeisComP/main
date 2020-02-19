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

#ifndef SEISCOMP_APPLICATIONS_SCOREPROCESSOR_H__
#define SEISCOMP_APPLICATIONS_SCOREPROCESSOR_H__


#include <string>
#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/interfacefactory.h>
#include <seiscomp/config/config.h>
#include <seiscomp/plugins/events/api.h>


namespace Seiscomp {


namespace DataModel {

class Origin;
class FocalMechanism;

}


namespace Client {


DEFINE_SMARTPOINTER(ScoreProcessor);


class SC_EVPLUGIN_API ScoreProcessor : public Seiscomp::Core::BaseObject {
	// ----------------------------------------------------------------------
	// X'struction
	// ----------------------------------------------------------------------
	public:
		ScoreProcessor();


	// ----------------------------------------------------------------------
	// Virtual public interface
	// ----------------------------------------------------------------------
	public:
		//! Setup all configuration parameters
		virtual bool setup(const Config::Config &config) = 0;

		//! Evaluates an origin.
		//! This method should return a score value. The higher the score
		//! the higher the origins priority in the process of selecting the
		//! preferred origin.
		virtual double evaluate(DataModel::Origin *origin) = 0;
		virtual double evaluate(DataModel::FocalMechanism *fm) = 0;
};


DEFINE_INTERFACE_FACTORY(ScoreProcessor);


}
}


#define REGISTER_ORIGINSCOREPROCESSOR(Class, Service) \
Seiscomp::Core::Generic::InterfaceFactory<Seiscomp::Client::ScoreProcessor, Class> __##Class##InterfaceFactory__(Service)


#endif
