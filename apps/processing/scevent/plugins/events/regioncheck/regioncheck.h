/***************************************************************************
 * Copyright (C) gempa GmbH                                                *
 * All rights reserved.                                                    *
 * Contact: gempa GmbH (seiscomp-dev@gempa.de)                             *
 *                                                                         *
 * GNU Affero General Public License Usage                                 *
 * This file may be used under the terms of the GNU Affero                 *
 * Public License version 3.0 as published by the Free Software Foundation *
 * and appearing in the file LICENSE included in the packaging of this     *
 * file. Please review the following information to ensure the GNU Affero  *
 * Public License version 3.0 requirements will be met:                    *
 * https://www.gnu.org/licenses/agpl-3.0.html.                             *
 *                                                                         *
 * Other Usage                                                             *
 * Alternatively, this file may be used in accordance with the terms and   *
 * conditions contained in a signed written agreement between you and      *
 * gempa GmbH.                                                             *
 ***************************************************************************/

#include <seiscomp/datamodel/types.h>
#include <seiscomp/geo/featureset.h>
#include <seiscomp/plugins/events/eventprocessor.h>
#include <seiscomp/seismology/regions.h>

namespace Seiscomp {
namespace DataModel {

class Event;
class Origin;

}

class RegionCheckProcessor : public Seiscomp::Client::EventProcessor {
	// ----------------------------------------------------------------------
	// X'truction
	// ----------------------------------------------------------------------
	public:
		//! C'tor
		RegionCheckProcessor() = default;


	// ----------------------------------------------------------------------
	// Public EventProcessor interface
	// ----------------------------------------------------------------------
	public:
		bool setup(const Config::Config &config) override;

		//! Do nothing
		DataModel::Magnitude *preferredMagnitude(const DataModel::Origin *) override;

		bool process(DataModel::Event *event, bool isNewEvent, const Journal &journal) override;


	// ----------------------------------------------------------------------
	// Private members
	// ----------------------------------------------------------------------
	private:
		typedef std::pair<Seiscomp::Geo::GeoFeature*,bool> RegionCheck;
		std::vector<RegionCheck>  _regions;
		OPT(DataModel::EventType) _eventTypePositive;
		OPT(DataModel::EventType) _eventTypeNegative;
		bool                      _hasPositiveRegions{false};
		bool                      _hasNegativeRegions{false};
		bool                      _setType{true};
		bool                      _readEventTypeFromBNA{false};
		bool                      _setEventType{true};
		bool                      _overwriteEventType{true};
		bool                      _overwriteManual{false};

};

}
