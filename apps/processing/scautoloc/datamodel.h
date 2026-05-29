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




#ifndef SEISCOMP_AUTOLOC_DATAMODEL_H
#define SEISCOMP_AUTOLOC_DATAMODEL_H
#include <string>
#include <map>
#include <vector>

#include <seiscomp/core/baseobject.h>
#include <seiscomp/datamodel/station.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/amplitude.h>


namespace Seiscomp {

namespace Autoloc {

// namespace DataModel {

typedef std::size_t OriginID;
typedef double Time;


DEFINE_SMARTPOINTER(Station);
class Station;

DEFINE_SMARTPOINTER(Origin);
class Origin;

DEFINE_SMARTPOINTER(Pick);
class Pick;


class Station : public Seiscomp::Core::BaseObject {
  	public:
		Station() = delete;
		Station(const Seiscomp::DataModel::Station*);
		Station(const std::string &code, const std::string &net, double lat, double lon, double alt);

		std::string code;
		std::string net;
		std::string loc;  // to be used in the future
		double lat;
		double lon;
		double alt;

		// Max. nucleation distance
		double maxNucDist {180.};

		// Max. location distance. Stations farther away from
		// epicenter may be associated but not used in location.
		double maxLocDist {180.};

		// Is this station enabled at all?
		bool enabled {true};
};


typedef std::map<std::string, StationCPtr> StationMap;


class Pick : public Seiscomp::Core::BaseObject {
	public:
		typedef enum {
			Automatic,
			Manual,
			Confirmed,
			IgnoredAutomatic
		} Mode;

	public:
		static size_t count();


	public:
		Pick() = delete;
		Pick(const Seiscomp::DataModel::Pick*);
//		Pick(const std::string &id, const std::string &label, const std::string &net, const std::string &sta, const Time &time);
		~Pick();

		const std::string& id() const {
			return scpick->publicID();
		}

		const std::string& net() const {
			return scpick->waveformID().networkCode();
		}

		const std::string& sta() const {
			return scpick->waveformID().stationCode();
		}

		const std::string& loc() const {
			return scpick->waveformID().locationCode();
		}

		const std::string& cha() const {
			return scpick->waveformID().channelCode();
		}

		const Station *station() const {
			return _station.get();
		}

		void setStation(const Station *sta) const;

		// get and set the origin this pick is associated with
		OriginID originID() const;
		void setOriginID(OriginID originID) const;

		// sets the amplitude to either the SNR or absolute amplitude
		void setAmplitudeSNR(const Seiscomp::DataModel::Amplitude*);
		void setAmplitudeAbs(const Seiscomp::DataModel::Amplitude*);

		Time time;	// pick time
		float amp {0};	// linear amplitude
		float per {0};	// period in seconds
		float snr {0};	// signal-to-noise ratio
		float normamp {0};	// normalized amplitude

		Mode mode {Automatic};
		bool xxl {false};	// Does it look like a pick from a big event?

		mutable bool blacklisted {false};
		mutable int priority {0};

	public:
		// Attached SC objects. The pick must never be null.
		Seiscomp::DataModel::PickCPtr      scpick;
		Seiscomp::DataModel::AmplitudeCPtr scamplAbs;
		Seiscomp::DataModel::AmplitudeCPtr scamplSNR;

		Seiscomp::Core::BaseObjectCPtr attachment;

		const std::string label;

	private:
		// The (one and only) origin this pick is associated to.
		// Need to be able to re-associate the pick to another origin.
		mutable OriginID _originID {0};

		// Station information
		mutable StationPtr _station {nullptr};
};



class Hypocenter : public Seiscomp::Core::BaseObject {
	public:
		Hypocenter() = default;
		Hypocenter(double lat, double lon, double dep)
			: lat(lat), lon(lon), dep(dep) { }

		double lat {0.};
		double lon {0.};
		double dep {0.};
		double laterr {0.};
		double lonerr {0.};
		double deperr {0.};
};



class Arrival {
	public:
		Arrival() = delete;
		Arrival(const Pick *pick, const std::string &phase="P", double residual=0);
		Arrival(const Origin *origin, const Pick *pick, const std::string &phase="P", double residual=0, double affinity=1);
		Arrival(const Arrival &) = default;

		const Origin* origin;
		PickCPtr pick;
		std::string phase;
		float residual {0.};
		float distance {0.};
		float azimuth {0.};
		float affinity {0.};
		float score {0.};
		bool backazimuthUsed{true};
		bool slownessUsed{true};

		float ascore {0.};
		float dscore {0.};
		float tscore {0.};

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
			TemporarilyExcluded = 32,

			// pick has been blacklisted
			BlacklistedPick = 64
		};

		ExcludeReason excluded {NotExcluded};
};




class ArrivalVector : public std::vector<Arrival> {
	public:
		bool sort();
};




class OriginQuality {
	public:
		OriginQuality() : aziGapPrimary(360), aziGapSecondary(360) {}

		double aziGapPrimary;
		double aziGapSecondary;
};




class OriginError {
	public:
		OriginError() : sdepth(0), stime(0), sdobs(0), conf(0) {}

		double sdepth, stime, sdobs, conf;
};




class Origin : public Seiscomp::Core::BaseObject {
	public:
		enum ProcessingStatus {
			New,
			Updated,
			Deleted
		};

		enum LocationStatus {
			// purely automatic
			Automatic,

			// automatic, but confirmed
			ConfirmedAutomatic,

			// manual with automatic additions
			SemiManual,

			// purely manual
			Manual  
		};

		enum DepthType {
			// depth is sufficiently well constrained by the data
			DepthFree,

			// depth is constrained by depth phases
			DepthPhases,

			// locator wants to put origin at zero depth
			//   -> use reasonable default
			DepthMinimum,

			// no depth resolution -> use reasonable default
			DepthDefault,

			// must not be overridden by autoloc
			DepthManuallyFixed
		};

	public:
		Origin() = delete;
//		Origin(const Seiscomp::DataModel::Origin*);  // TODO but not possible at the moment
		Origin(double lat, double lon, double dep, const Time &time);
		Origin(const Origin&);
		~Origin();

		static size_t count();

		void updateFrom(const Origin*);

		// Add an arrival to the origin. This checks if the pick
		// references by the arrival is already associated to the
		// origin and if so, the arrival is not added and false is
		// returned. Upon success, true is returned.
		bool add(const Arrival &arr);

		// Adopt a pick. This means that the origin assumes
		// "ownership" of this pick, which prevents it to be "owned"
		// by another origin.
		bool adopt(const Pick *pick) const;

		// Return index of arrival referencing the exact same pick
		// or -1 if not found
		int findArrival(const Pick *pick) const;

		// Count the defining phases, optionally within a distance range
		size_t phaseCount(double dmin=0., double dmax=180.) const;
		size_t definingPhaseCount(double dmin=0., double dmax=180.) const;

		// Count the associated stations
		size_t definingStationCount() const;
		size_t associatedStationCount() const;

		// Compute the pick residual RMS for all picks used in the solution.
		double rms() const;

		// Compute the median station distance.
		//
		// Returns a negative number if the number of used
		// stations is zero. Perhaps replace this by an
		// exception.
		double medianStationDistance() const;

		void geoProperties(double &min, double &max, double &gap) const;

	public:
		OriginID id{0};

		bool imported {false};
		bool manual {false};
		bool preliminary {false};

		std::string publicID;
		std::string methodID;
		std::string earthModelID;

		ProcessingStatus processingStatus{New};
		LocationStatus locationStatus{Automatic};

		double score{0};

		Hypocenter hypocenter;
		Time time{0}, timestamp{0};
		double timeerr{0};
		DepthType depthType{DepthFree};
		ArrivalVector arrivals;
		OriginQuality quality;
		OriginError error;

	public:
//		Seiscomp::DataModel::OriginCPtr scmanual;
		Seiscomp::DataModel::OriginCPtr scorigin;
};




class OriginVector : public std::vector<OriginPtr> {

	public:
		// Return true if the origin instance is in the vector.
		bool find(const Origin *) const;

		// Return origin with the origin ID if found, NULL otherwise
		Origin *find(const OriginID &id);
		const Origin *find(const OriginID &id) const;

		// Try to find the best Origin which possibly belongs to the
		// same event
		const Origin *bestEquivalentOrigin(const Origin *start) const;

		// Try to find Origins which possibly belong to the same
		// event and try to merge the picks
		int mergeEquivalentOrigins(const Origin *start=0);
};

typedef std::vector<PickPtr> PickVector;
typedef std::vector<Pick*>   PickGroup;

// }  // namespace DataModel

}  // namespace Autoloc

}  // namespace Seiscomp

#endif
