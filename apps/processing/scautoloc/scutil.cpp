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




# define SEISCOMP_COMPONENT Autoloc
#include <seiscomp/logging/log.h>
#include <seiscomp/datamodel/publicobject.h>
#include <map>

#include "scutil.h"

namespace Seiscomp {

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void logObjectCounts()
{
	// This is expensive so don't overuse it!

	DataModel::PublicObject::Lock();
	SEISCOMP_DEBUG("%-16s count = %d", "PublicObject", DataModel::PublicObject::ObjectCount());
	std::map<const char*, std::size_t> count;
	for (DataModel::PublicObject::Iterator
			it  = DataModel::PublicObject::Begin();
			it != DataModel::PublicObject::End(); ++it) {
		++count[(*it).second->className()];
	}
	for (auto& item: count)
		SEISCOMP_DEBUG("%-16s count = %d", item.first, item.second);
	DataModel::PublicObject::Unlock();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool manual(const DataModel::Origin *origin) {
	try {
		switch (origin->evaluationMode()) {
		case DataModel::MANUAL:
			return true;
		default:
			break;
		}
	}
	catch ( Core::ValueException & ) {}
	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

} // namespace Seiscomp
