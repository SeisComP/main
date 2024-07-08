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




#ifndef _SEISCOMP_AUTOLOC_DATAMODEL_
#define _SEISCOMP_AUTOLOC_DATAMODEL_
#include <string>
#include <map>
#include <vector>

#include <seiscomp/core/baseobject.h>
#include <seiscomp/datamodel/origin.h>


namespace Autoloc {

typedef std::size_t OriginID;
typedef double Time;

DEFINE_SMARTPOINTER(Station);

class Station : public Seiscomp::Core::BaseObject
{
  	public:
		Station() = delete;
		Station(const std::string &code, const std::string &net, double lat, double lon, double alt);

		std::string code, net;
		double lat, lon, alt;
		bool used;
		double maxNucDist, maxLocDist;
};

typedef std::map<std::string, StationCPtr> StationMap;


DEFINE_SMARTPOINTER(Origin);
class Origin;


DEFINE_SMARTPOINTER(Pick);
class Pick;

class Pick : public Seiscomp::Core::BaseObject
{
	public:
		typedef enum { Automatic, Manual, Confirmed, IgnoredAutomatic } Mode;

   	public:
		Pick() = delete;
		Pick(const std::string &id, const std::string &net, const std::string &sta, const Time &time);
		~Pick();

		static size_t count();

		/* const */ std::string id, net, sta, loc, cha;
		const Station *station() const { return _station.get(); }
		void setStation(const Station *sta) const;

		// get and set the origin this pick is associated with
		const OriginID origin() const { return _originID; }
		void setOrigin(OriginID id) const;

		Time time;	// pick time
		float amp;	// linear amplitude
		float per;	// period in seconds
		float snr;	// signal-to-noise ratio
		float normamp;	// normalized amplitude

		Mode mode;
		bool xxl;	// Does it look like a pick from a big event?

		Seiscomp::Core::BaseObjectCPtr attachment;

	private:
		// The (one and only) origin this pick is associated to.
		// Need to be able to re-associate the pick to another origin.
		mutable OriginID _originID;

		// Station information
		mutable StationPtr _station;
};




class Hypocenter
{
  	public:
		Hypocenter() = delete;
		Hypocenter(double lat, double lon, double dep)
			: lat(lat), lon(lon), dep(dep), laterr(0), lonerr(0), deperr(0) { }

		double lat, lon, dep;
		double laterr, lonerr, deperr;
};




class Arrival
{
	public:
		Arrival() = delete;

		Arrival(const Pick *pick, const std::string &phase="P", double residual=0);
		Arrival(const Origin *origin, const Pick *pick, const std::string &phase="P", double residual=0, double affinity=1);
		Arrival(const Arrival &) = default;

		const Origin* origin;
		PickCPtr pick;
		std::string phase;
		float residual;
		float distance;
		float azimuth;
		float affinity;
		float score;

		float ascore, dscore, tscore;

		// Reasons for why a pick may be excluded from the computation
		// of an origin. This allows exclusion of suspicious picks
		// independent of their weight, which remains unmodified.
		enum ExcludeReason {
			NotExcluded      = 0,

			// if a maximum residual is exceeded
			LargeResidual    = 1,

			// e.g. PKP if many stations in 0-100 deg distance range
			StationDistance  = 2,

			// Such a pick must remain excluded from the computation
			ManuallyExcluded = 4,

			// A pick that deteriorates the solution,
			// e.g. reduces score or badly increases RMS
			DeterioratesSolution = 8,

			// PP, SKP, SKS etc.
			UnusedPhase = 16,

			// pick temporarily excluded from the computation
			// XXX experimental XXX
			TemporarilyExcluded = 32
		};
		ExcludeReason excluded;
};




class ArrivalVector : public std::vector<Arrival>
{
	public:
		bool sort();
};




class OriginQuality
{
	public:
		OriginQuality() : aziGapPrimary(360), aziGapSecondary(360) {}

		double aziGapPrimary;
		double aziGapSecondary;
};




class OriginError
{
	public:
		OriginError() : sdepth(0), stime(0), sdobs(0), conf(0) {}

		double sdepth, stime, sdobs, conf;
};




class Origin : public Seiscomp::Core::BaseObject
{
	public:
		enum ProcessingStatus { New, Updated, Deleted };

		enum LocationStatus {
			Automatic,              // purely automatic
			ConfirmedAutomatic,     // automatic, but confirmed
			SemiManual,             // manual with automatic additions
			Manual                  // purely manual
		};

		enum DepthType {
			DepthFree,              // depth is sufficiently well constrained by the data
			DepthPhases,            // depth is constrained by depth phases
			DepthMinimum,           // locator wants to put origin at zero depth -> use reasonable default
			DepthDefault,           // no depth resolution -> use reasonable default
			DepthManuallyFixed      // must not be overridden by autoloc
		};

	public:
		Origin() = delete;
		Origin(double lat, double lon, double dep, const Time &time);
		Origin(const Origin&);
		~Origin();

		static size_t count();

		void updateFrom(const Origin*);

		bool add(const Arrival &arr);

		// return index of arrival referencing pick, -1 if not found
		int findArrival(const Pick *pick) const;

		// count the defining phases, optionally within a distance range
		size_t phaseCount(double dmin=0., double dmax=180.) const;
		size_t definingPhaseCount(double dmin=0., double dmax=180.) const;

		// count the association stations
		size_t definingStationCount() const;
		size_t associatedStationCount() const;

		double rms() const;

		// compute the median station distance. Returns a negative number if
		// the number of used stations is zero. Perhaps replace this by an
		// exception.
		double medianStationDistance() const;

		void geoProperties(double &min, double &max, double &gap) const;

	public:
		OriginID id;

		Hypocenter hypocenter;
		Time time, timestamp;
		double timeerr;
		DepthType depthType;
		ArrivalVector arrivals;
		OriginQuality quality;
		OriginError error;

		bool imported;
		bool preliminary;
		std::string publicID;

		std::string methodID;
		std::string earthModelID;

		ProcessingStatus processingStatus;
		LocationStatus locationStatus;

		double score;

	public:
//		Seiscomp::DataModel::OriginCPtr scmanual;
		Seiscomp::DataModel::OriginCPtr scorigin;
};




class OriginVector : public std::vector<OriginPtr>
{
	public:
		bool find(const Origin *) const;
		Origin *find(const OriginID &id);

		// Try to find the best Origin which possibly belongs to the same event
		const Origin *bestEquivalentOrigin(const Origin *start) const;

		// try to find Origins which possibly belong to the same event and try
		// to merge the picks
		int mergeEquivalentOrigins(const Origin *start=0);
};


typedef std::vector<PickPtr> PickVector;
typedef std::vector<Pick*>   PickGroup;

}  // namespace Autoloc

#endif
