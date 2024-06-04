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


#define SEISCOMP_COMPONENT SCQC
#include <seiscomp/logging/log.h>

#include "qcbuffer.h"


namespace Seiscomp {
namespace Applications {
namespace Qc {
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
QcBuffer::QcBuffer()
: _maxBufferSize(-1) {
// buffer size in seconds
	lastEvalTime = Core::Time::GMT();
	_recentlyUsed = false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
QcBuffer::QcBuffer(double maxBufferSize)
: _maxBufferSize(maxBufferSize) {
// buffer size in seconds

// 	lastEvalTime = Core::Time(1970, 01, 01);
	lastEvalTime = Core::Time::GMT();
	_recentlyUsed = false;

}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void QcBuffer::setRecentlyUsed(bool status) {
	_recentlyUsed = status;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool QcBuffer::recentlyUsed() const {
	return _recentlyUsed;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//! push QcParameter at back of buffer and remove elements (from whole buffer),
//! older than _maxBufferSize [sec]
void QcBuffer::push_back(const QcParameter *qcp) {
	if ( empty() ) {
		BufferBase::push_back(qcp);
		return;
	}

	if ( qcp->recordEndTime >= back()->recordEndTime ) {
		// Sorted insert
		BufferBase::push_back(qcp);
	}
	else {
		if ( qcp->recordEndTime < front()->recordEndTime ) {
			BufferBase::push_front(qcp);
		}
		else {
			auto rit = rbegin();
			--rit;
			while ( rit != rend() ) {
				if ( qcp->recordEndTime >= (*rit)->recordEndTime ) {
					BufferBase::insert(rit.base(), qcp);
					break;
				}
			}
		}
	}

	// buffer size is 'unlimited'
	if ( _maxBufferSize == -1 ) {
		return;
	}

	Core::TimeSpan maxSpan = _maxBufferSize * 1.1;
	auto it = begin();
	while ( (it != end()) && (back()->recordEndTime - (*it)->recordEndTime > maxSpan) ) {
		it = erase(it);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//! return qcParameter at specific time
/*
const QcParameter* QcBuffer::qcParameter(const Core::Time &time) const {
	for ( const_iterator it = begin(); it != end(); ++it ) {
		if ( (*it)->recordStartTime >= time && (*it)->recordEndTime < time )
		return (*it).get();
	}

	return NULL;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//! return list of qcParameters for time range
const QcBuffer* QcBuffer::qcParameter(const Core::Time &startTime,
                                      const Core::Time &endTime) const {
	QcBuffer *qcb = new QcBuffer();

	if (empty()) return qcb;

	int count = 0;

	for ( const_iterator it = begin(); it != end(); ++it ) {
		if ( (*it)->recordStartTime >= startTime && (*it)->recordEndTime <= endTime ) {
			qcb->push_back((*it).get());
			count++;
		}
	}

	return qcb;
}
*/
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//! return list of qcParameters for the last n seconds in buffer
QcBuffer *QcBuffer::qcParameter(const Core::TimeSpan &lastNSeconds) const {
	QcBuffer *qcb = new QcBuffer();
	
	if ( empty() ) return qcb;

	const_reverse_iterator from = rbegin();
	const_reverse_iterator to = rbegin();

	for ( const_reverse_iterator it = rbegin(); it != rend(); ++it ) {
		if ( !(*it) ) continue;

		Core::TimeSpan diff = back()->recordEndTime - (*it)->recordStartTime;

		from = it;
		++from;

		if ( diff > lastNSeconds )
			break;
	}

	if ( from != to ) {
		qcb->insert(qcb->begin(), to, from);
		qcb->reverse();
	}

	return qcb;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const Core::Time &QcBuffer::startTime() const {
	return front()->recordStartTime;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const Core::Time &QcBuffer::endTime() const {
	return back()->recordEndTime;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Core::TimeSpan QcBuffer::length() const {
	//! TODO iterate thru' entries -- do not account for 'buffer gaps'
	if ( empty() ) return Core::TimeSpan(0.0);

	return Core::TimeSpan(back()->recordEndTime - front()->recordStartTime);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void QcBuffer::info() const {
	SEISCOMP_DEBUG("Buffer::info start: %s  end: %s  length: %5.1f sec (%ld records)",
                   front()->recordStartTime.iso().c_str(),
                   back()->recordEndTime.iso().c_str(),
                   (double)length(), (long int)size());

}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//! dump buffer content
void QcBuffer::dump() const {
	for ( const_iterator it = begin(); it != end(); ++it )
		std::cout << (*it)->recordStartTime.iso() << " -- "
		          << (*it)->recordEndTime.iso() << " "
		          << (*it)->recordSamplingFrequency << " "
		          << std::endl;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
}
}
}
