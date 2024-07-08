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




#ifndef _SEISCOMP_AUTOLOC_ASSOCIATOR_
#define _SEISCOMP_AUTOLOC_ASSOCIATOR_

#include "datamodel.h"

namespace Autoloc {


typedef Arrival Association;

class AssociationVector : public std::vector<Association>
{
};


// Simple pick-to-origin associator. Can only associate P phases.
//
// TODO: Also would be good to be able to associate all picks to one origin,
// also for other phases than P
class Associator
{
	public:
		class Phase;

	public:
		Associator();

	public:
		void setStations(const StationMap *stations);
		void setOrigins(const OriginVector *origins);

	public:
		// Feed a pick and try to associate it with known origins,
		// under the assumption that this is a P pick.
		//
		// The associated origins are retrieved using associations()
		bool feed(const Pick* pick);

		// Retrieve associations made during the last call of feed()
		const AssociationVector& associations() const;

		Association* associate(Origin *origin, const Pick *pick, const std::string &phase);

	public:
		void reset();
		void shutdown();

	protected:
		// these are not owned:
		const StationMap *_stations;
		const OriginVector  *_origins;

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

} // namespace Autoloc

#endif
