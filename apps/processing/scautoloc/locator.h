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


#ifndef SEISCOMP_AUTOLOC_LOCATOR
#define SEISCOMP_AUTOLOC_LOCATOR


#include <string>
#include <map>

#include <seiscomp/seismology/locator/locsat.h>
#include "datamodel.h"


namespace Autoloc {

DEFINE_SMARTPOINTER(MySensorLocationDelegate);

class MySensorLocationDelegate : public Seiscomp::Seismology::SensorLocationDelegate
{
	public:
		~MySensorLocationDelegate();
	public:
		virtual Seiscomp::DataModel::SensorLocation* getSensorLocation(Seiscomp::DataModel::Pick *pick) const;
		void setStation(const Station *station);
	private:
		typedef std::map<std::string, Seiscomp::DataModel::SensorLocationPtr> SensorLocationList;
		SensorLocationList _sensorLocations;
};

class Locator {
	public:
		Locator();
		~Locator();

		// Configuration
		void setProfile(const std::string &name) {
			_sclocator->setProfile(name);
		}

		void setSeiscompConfig(const Seiscomp::Config::Config*);

		// Initialization needed *after* (re)configuration
		bool init();

		void setStation(const Station *station);
		void setMinimumDepth(double);

		void setFixedDepth(double depth, bool use=true) {
			_sclocator->setFixedDepth(depth, use);
		}

		void useFixedDepth(bool use=true) {
			_sclocator->useFixedDepth(use);
		}

	public:
		Origin *relocate(const Origin *origin);


	private:
		// this is the SC-level relocate
		Origin *_screlocate(const Origin *origin);

	private:
		Seiscomp::Seismology::LocatorInterfacePtr _sclocator;
		const Seiscomp::Config::Config *_scconfig;

		MySensorLocationDelegatePtr sensorLocationDelegate;

		double _minDepth;

		size_t _locatorCallCounter;
};



bool determineAzimuthalGaps(const Origin*, double *primary, double *secondary);

}  // namespace Autoloc

#endif
