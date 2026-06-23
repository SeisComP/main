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

void PublicObjectQueue::setOrderByCreationTime(bool v) {
	orderByCreationTime = v;
}

bool PublicObjectQueue::fill(const EventParameters *ep) {
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
		if ( orderByCreationTime ) {
			try {
				Time t = pick->creationInfo().creationTime();
				objs.push_back(TimeObject(t, 0, pick));
			}
			catch ( ... ) {
				SEISCOMP_WARNING("Pick %s: no creation time set", pick->publicID());
			}
		}
		else {
			// Use the pick time as object time
			try {
				Time t = pick->time().value();
				objs.push_back(TimeObject(t, 0, pick));
			}
			catch ( ... ) {
				SEISCOMP_WARNING("Pick %s: no pick time set", pick->publicID());
			}
		}
	}

	for ( size_t i = 0; i < ep->amplitudeCount(); ++i ) {
		AmplitudePtr amplitude = ep->amplitude(i);
		if ( orderByCreationTime ) {
			try {
				Time t = amplitude->creationInfo().creationTime();
				objs.push_back(TimeObject(t, 1, amplitude));
			}
			catch ( ... ) {
				SEISCOMP_WARNING("Amplitude %s: no creation time set", amplitude->publicID());
			}
		}
		else {
			// Use the amplitude reference time as object time
			try {
				Time t = amplitude->timeWindow().reference();
				objs.push_back(TimeObject(t, 1, amplitude));
			}
			catch ( ... ) {
				SEISCOMP_WARNING("Amplitude %s: no reference time set", amplitude->publicID());
			}
		}
	}

	for ( size_t i = 0; i < ep->originCount(); ++i ) {
		OriginPtr origin = ep->origin(i);
		if ( orderByCreationTime ) {
			try {
				Time t = origin->creationInfo().creationTime();
				objs.push_back(TimeObject(t, 2, origin));
			}
			catch ( ... ) {
				SEISCOMP_WARNING("Origin %s: no creation time set", origin->publicID());
			}
		}
		else {
			// Use the pick time of the last associated pick as object time
			Time t = origin->time().value();
			for (size_t iarr = 0; iarr < origin->arrivalCount(); ++i) {
				const std::string pickID = origin->arrival(iarr)->pickID();
				const Pick *pick = Pick::Find(pickID);
				if ( pick ) {
					try {
						if ( pick->time().value() > t) {
							t = pick->time().value();
						}
					}
					catch ( ... ) {
						SEISCOMP_WARNING("Pick %s: no pick time set", pick->publicID());
					}
				}
			}
			objs.push_back(TimeObject(t, 2, origin));
		}
	}

	sort(objs.begin(), objs.end());
	for ( TimeObject &obj : objs ) {
		q.push(get<2>(obj));
	}

	return !q.empty();
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
