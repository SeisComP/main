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



#ifndef SEISCOMP_LIBAUTOLOC_OBJECTQUEUE_H_INCLUDED
#define SEISCOMP_LIBAUTOLOC_OBJECTQUEUE_H_INCLUDED

#include <queue>
#include <seiscomp/datamodel/publicobject.h>
#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/amplitude.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/eventparameters.h>

namespace Seiscomp {

namespace Autoloc {

// object queue used for XML playback
//typedef std::queue<Seiscomp::DataModel::PublicObjectPtr> PublicObjectQueue;
// PublicObject* front();
// pop();
// bool empty() const

class PublicObjectQueue {

public:
	bool fill(const Seiscomp::DataModel::EventParameters*);

	void push(Seiscomp::DataModel::PublicObject *o) {
		q.push(o);
	}

	Seiscomp::DataModel::PublicObject*
	front() {
		return q.front().get();
	}

	void pop() {
		q.pop();
	}

	bool empty() const {
		return q.empty();
	}

	size_t size() const {
		return q.size();
	}
private:
	std::queue<Seiscomp::DataModel::PublicObjectPtr> q;
};


} // namespace Autoloc

} // namespace Seiscomp

#endif
