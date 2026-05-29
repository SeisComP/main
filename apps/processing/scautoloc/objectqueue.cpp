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



#define SEISCOMP_COMPONENT Autoloc
#include "objectqueue.h"

#include <algorithm>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/amplitude.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/logging/log.h>


namespace Seiscomp {

namespace DataModel {

bool PublicObjectQueue::fill(const EventParameters *ep)
{
	using namespace Seiscomp::Core;
	using namespace std;

	// Tuple to be used in DSU sorting. The second member is used to
	// place picks before amplitudes with identical creation times.
	typedef tuple<Time, int, PublicObjectPtr> TimeObject;
	typedef vector<TimeObject> TimeObjectVector;

	// retrieval of relevant objects from event parameters
	// and subsequent DSU sort
	TimeObjectVector objs;

	for ( size_t i = 0; i < ep->pickCount(); ++i ) {
		PickPtr pick = ep->pick(i);
		try {
			Time t = pick->creationInfo().creationTime();
			objs.push_back(TimeObject(t, 0, pick));
		}
		catch ( ... ) {
			SEISCOMP_WARNING(
				"Pick %s: no creation time set",
				pick->publicID().c_str());
		}
	}

	for ( size_t i = 0; i < ep->amplitudeCount(); ++i ) {
		AmplitudePtr amplitude = ep->amplitude(i);
		try {
			Time t = amplitude->creationInfo().creationTime();
			objs.push_back(TimeObject(t, 1, amplitude));
		}
		catch ( ... ) {
			SEISCOMP_WARNING(
				"Amplitude %s: no creation time set",
				amplitude->publicID().c_str());
		}
	}

	for ( size_t i = 0; i < ep->originCount(); ++i ) {
		OriginPtr origin = ep->origin(i);
		try {
			Time t = origin->creationInfo().creationTime();
			objs.push_back(TimeObject(t, 2, origin));
		}
		catch ( ... ) {
			SEISCOMP_WARNING(
				"Origin %s: no creation time set",
				origin->publicID().c_str());
		}
	}

	sort(objs.begin(), objs.end());
	for ( TimeObject &obj : objs )
		q.push(get<2>(obj));

	return ! q.empty();
}


void PublicObjectQueue::push(PublicObject *o) {
	q.push(o);
}

PublicObject*
PublicObjectQueue::front() {
	return q.front().get();
}

void PublicObjectQueue::pop() {
	q.pop();
}

bool PublicObjectQueue::empty() const {
	return q.empty();
}

size_t PublicObjectQueue::size() const {
	return q.size();
}


} // namespace DataModel

} // namespace Seiscomp
