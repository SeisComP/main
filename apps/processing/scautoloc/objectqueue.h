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
#include <seiscomp/datamodel/eventparameters.h>

namespace Seiscomp {

namespace DataModel {

// PublicObjectQueue
//
// Queue of PublicObject's read from EventParameters to be used in playbacks.
//
// Only supports Pick, Amplitude, Origin

class PublicObjectQueue {

	public:
		PublicObjectQueue() = default;

		void setOrderByCreationTime(bool = false);

		// Fill the queue with the PublicObjects taken from
		// the specified EventParameters instance. Objects
		// are sorted according to creation time, with the oldest
		// objects at the front.
		bool fill(const Seiscomp::DataModel::EventParameters*);

		void push(Seiscomp::DataModel::PublicObject *o);

		Seiscomp::DataModel::PublicObject* front();

		void pop();

		bool empty() const;

		size_t size() const;

	private:
		bool orderByCreationTime{false};
		std::queue<Seiscomp::DataModel::PublicObjectPtr> q;
};


} // namespace DataModel

} // namespace Seiscomp

#endif
