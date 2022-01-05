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


#ifndef SEISCOMP_LIBAUTOLOC_ASSOCIATOR_H_INCLUDED
#define SEISCOMP_LIBAUTOLOC_ASSOCIATOR_H_INCLUDED

#include <seiscomp/seismology/ttt.h>
#include <seiscomp/autoloc/datamodel.h>

namespace Seiscomp {


typedef Autoloc::DataModel::Arrival Association;

/*
class Association
{
	public:
		Association(
			const Autoloc::DataModel::Pick *pick,
			const Autoloc::DataModel::Origin *origin,
			double affinity=0, double residual=0)
			: pick(pick), origin(origin), affinity(affinity), residual(residual)
		{ }

	public:
		const Autoloc::DataModel::Pick   *pick;
		const Autoloc::DataModel::Origin *origin;

		double affinity, residual;
};
*/

class PhaseRange;

typedef std::vector<Association> AssociationVector;
typedef std::vector<PhaseRange> PhaseRangeVector;

// Simple pick-to-origin associator. Can only associate P phases.

class Associator
{
	public:
		Associator();
		~Associator();

	public:
		void setStations(const Autoloc::DataModel::StationMap *stations);
		void setOrigins(const Autoloc::DataModel::OriginVector *origins);
		void setPickPool(const Autoloc::DataModel::PickPool*);

	public:
		// Feed a pick and try to associate it with known origins, under the
		// assumption that this is a P pick.
		//
		// The associated origins are then retrieved using associations()
		bool feed(const Autoloc::DataModel::Pick* pick);

		// Associate a pick with an origin. The origin is updated.
		Association* associate(
			      Autoloc::DataModel::Origin *origin,
			const Autoloc::DataModel::Pick *pick,
			const std::string &phase);

		// For an origin find all matching picks
		// irrespective of association to another origin.
		//
		// The matches are appended as Associations to the
		// AssociationVector. The origin itself is not modified.
		bool findMatchingPicks(
			const Autoloc::DataModel::Origin*,
			AssociationVector &associations) const;

		// For a pick find all matching origins.
		//
		// The matches are appended as Associations to the
		// AssociationVector. The pick itself is not modified.
		bool findMatchingOrigins(
			const Autoloc::DataModel::Pick*,
			AssociationVector &associations) const;

	private:
		bool findPhaseRange(const std::string &code, PhaseRange &) const;

	public:
		void reset();
		void shutdown();

	protected:
		// these are not owned:
		const Autoloc::DataModel::StationMap *_stations;
		const Autoloc::DataModel::OriginVector *_origins;
		const Autoloc::DataModel::PickPool *pickPool;

	private:
		PhaseRangeVector _phaseRanges;
		mutable Seiscomp::TravelTimeTable ttt;
};


class PhaseRange
{
	public:
		PhaseRange() = default;
		PhaseRange(
			const std::string &code,
			double dmin,   double dmax,
			double zmin=0, double zmax=800);

		bool contains(double delta, double depth) const;

	public:
		std::string code;
		double dmin, dmax;
		double zmin, zmax;
};


} // namespace Seiscomp

#endif
