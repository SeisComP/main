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




#ifndef _SEISCOMP_AUTOLOC_NUCLEATOR_
#define _SEISCOMP_AUTOLOC_NUCLEATOR_

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <map>

#include "datamodel.h"
#include "locator.h"


namespace Autoloc {

class Nucleator
{
	public:
		virtual bool init() = 0;
		virtual void setStation(const Station *station);

		virtual void setSeiscompConfig(const Seiscomp::Config::Config*) = 0;

	public:
		virtual bool feed(const Pick *pick) = 0;
		const OriginVector &newOrigins() const;

		virtual int  cleanup(const Time& minTime) = 0;
		virtual void reset() = 0;
		virtual void shutdown() = 0;

	protected:
		// finalize configuration - to be called e.g. to set up a grid
		virtual void setup() = 0;

	protected:
		StationMap _stations;
//		double _config_maxDistanceXXL;

		std::set<std::string> _configuredStations;

	public:
		OriginVector _newOrigins;
};


class GridPoint;
DEFINE_SMARTPOINTER(GridPoint);
typedef std::vector<GridPointPtr> Grid;


class GridSearch : public Nucleator
{
	public:
		GridSearch();
		virtual bool init();

	public:
		// Configuration parameters controlling the behaviour
		// of the Nucleator
		struct Config {
			// minimum number of stations for new origin
			int nmin{5};
		
			// maximum distance of stations contributing to
			// a new origin
			double dmax{180.0};

			// configurable multiplier, 0 means it is ignored
			double maxRadiusFactor{1.0};
	
			// minimum cumulative amplitude of all picks
			double amin{5.0 * nmin};
		
			int verbosity{0};
		};

	public:
		void setStation(const Station *station);

		const Config &config() const { return _config; }
		void setConfig(const Config &config) { _config = config; }

		bool setGridFile(const std::string &gridfile);

		void setLocatorProfile(const std::string &profile);

		void setSeiscompConfig(const Seiscomp::Config::Config*);

	public:
		// The Nucleator reads Pick's and Amplitude's. Only picks
		// with associated amplitude can be fed into the Nucleator.
		bool feed(const Pick *pick);
	
		int cleanup(const Time& minTime);
	
		void reset()
		{
			_newOrigins.clear();
		}
		void shutdown()
		{
			_abort = true;
			_grid.clear();
		}

	protected:
		virtual void setup();

		// setup a single station - ideally "on the fly"
		bool _setupStation(const Station *station);

	private:
		bool _readGrid(const std::string &gridfile);

	private:
		Grid    _grid;
		Locator _relocator;

		bool _abort;

		const Seiscomp::Config::Config *_scconfig{nullptr};

	public: // FIXME
		Config  _config;
};



// From a GridPoint point of view, a station has a
// distance, azimuth, traveltime etc. These are stored
// in StationWrapper, together with the corresponding
// StationPtr.
DEFINE_SMARTPOINTER(StationWrapper);

class StationWrapper : public Seiscomp::Core::BaseObject {

	public:
		StationWrapper(const Station *station, const std::string &phase, float distance, float azimuth, float ttime, float hslow)
			: Seiscomp::Core::BaseObject(), station(station), distance(distance), azimuth(azimuth), ttime(ttime), hslow(hslow), phase(phase)  {}

		StationWrapper(const StationWrapper &other)
			: Seiscomp::Core::BaseObject(), station(other.station), distance(other.distance), azimuth(other.azimuth), ttime(other.ttime), hslow(other.hslow), phase(other.phase) {}

		// Since there will be of the order
		// 10^5 ... 10^6 StationWrapper instances,
		// we need to use floats
		const Station *station;
		float distance, azimuth; //, backazimuth;
		float ttime, hslow; //, vslow;
		// to further save space, make this a
		// pointer to a static phase list entry:
		std::string phase;
};



// A Pick projected in back time, corresponding
// to the grid point location
//DEFINE_SMARTPOINTER(ProjectedPick);
//class ProjectedPick : public Seiscomp::Core::BaseObject {
class ProjectedPick {

	public:
		ProjectedPick(const Time &t);
		ProjectedPick(PickCPtr p, StationWrapperCPtr w);
		ProjectedPick(const ProjectedPick&);
		~ProjectedPick();

		static size_t Count();

		bool operator<(const ProjectedPick &p) const {
			return (this->projectedTime() < p.projectedTime()); }
		bool operator>(const ProjectedPick &p) const {
			return (this->projectedTime() > p.projectedTime()); }


		Time projectedTime() const { return _projectedTime; }

	// private:
		PickCPtr p;
		StationWrapperCPtr wrapper;
	private:
		Time _projectedTime;
};



class GridPoint : public Seiscomp::Core::BaseObject
{
	public:
		// normal grid point
		GridPoint(double latitude, double longitude, double depth);

		~GridPoint() {
			_wrappers.clear();
			_picks.clear();
		}

	public:
		// feed a new pick and perhaps get a new origin
		const Origin* feed(const Pick*);

		// remove all picks older than tmin
		int cleanup(const Time& minTime);

	public:
		// void setStations(const StationMap *stations);

		bool setupStation(const Station *station);

	public:
		Hypocenter hypocenter;

	public: // private:
		// config
		double _radius, _dt;
		double maxStaDist;
		size_t _nmin;

	private:
		std::map<std::string, StationWrapperCPtr> _wrappers;
		std::multiset<ProjectedPick> _picks;
};


double originScore(const Origin *origin, double maxRMS=3.5, double radius=0.);

}

#endif
