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


#ifndef SEISCOMP_AUTOLOC_ASSOCIATOR_H_INCLUDED
#define SEISCOMP_AUTOLOC_ASSOCIATOR_H_INCLUDED

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

typedef std::vector<Association> AssociationVector;


// Simple pick-to-origin associator. Can only associate P phases.

class Associator
{
	public:
		class Phase;

	public:
		Associator();
		~Associator();

	public:
		void setStations(const Autoloc::DataModel::StationMap *stations);
		void setOrigins(const Autoloc::DataModel::OriginVector *origins);

	public:
		// Feed a pick and try to associate it with known origins, under the
		// assumption that this is a P pick.
		//
		// The associated origins are then retrieved using associations()
		bool feed(const Autoloc::DataModel::Pick* pick);

		// Retrieve associations made during the last call of feed()
		const AssociationVector& associations() const;

		Association* associate(
			      Autoloc::DataModel::Origin *origin,
			const Autoloc::DataModel::Pick *pick,
			const std::string &phase);

	public:
		void reset();
		void shutdown();

	protected:
		// these are not owned:
		const Autoloc::DataModel::StationMap *_stations;
		const Autoloc::DataModel::OriginVector  *_origins;

	private:
		AssociationVector _associations;
		std::vector<Phase> _phases;
};


class Associator::Phase
{
	public:
		Phase(const std::string&, double, double);
		std::string code;
		double dmin, dmax;
};


} // namespace Seiscomp

#endif
