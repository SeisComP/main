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



#include "filter.h"

namespace Seiscomp
{
namespace Applications
{


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
std::map<std::string, Filter*> Filter::_Registry;
std::map<std::string, Filter::Operator> Filter::_Operators;
bool OriginFilter::_wasAccepted = false;
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
IMPLEMENT_MESSAGE_FILTER(PickFilter)
IMPLEMENT_MESSAGE_FILTER(AmplitudeFilter)
IMPLEMENT_MESSAGE_FILTER(OriginFilter)
IMPLEMENT_MESSAGE_FILTER(ArrivalFilter)
IMPLEMENT_MESSAGE_FILTER(EventFilter)
IMPLEMENT_MESSAGE_FILTER(StationMagnitudeFilter)
IMPLEMENT_MESSAGE_FILTER(MagnitudeFilter)
IMPLEMENT_MESSAGE_FILTER(WaveformQualityFilter)
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


} // namespace Applications
} // namespace Seiscomp

