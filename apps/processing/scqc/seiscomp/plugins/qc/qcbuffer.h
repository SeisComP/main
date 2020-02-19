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


#ifndef SEISCOMP_QC_QCBUFFER_H__
#define SEISCOMP_QC_QCBUFFER_H__


#include <seiscomp/qc/qcprocessor.h>
#include <seiscomp/plugins/qc/api.h>

using namespace Seiscomp::Processing;

namespace Seiscomp {
namespace Applications {
namespace Qc {

typedef std::list<QcParameterCPtr> BufferBase;

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
DEFINE_SMARTPOINTER(QcBuffer);

class SC_QCPLUGIN_API QcBuffer : public Core::BaseObject, public BufferBase {
	public:
		QcBuffer();
		QcBuffer(double maxBufferSize);
		
		mutable Core::Time lastEvalTime;

		void push_back(const QcParameter *qcp);

		/*
		const QcParameter* qcParameter(const Core::Time &time) const;
		const QcBuffer* qcParameter(const Core::Time &startTime, const Core::Time &endTime) const;
		*/
		QcBuffer *qcParameter(const Core::TimeSpan &lastNSeconds) const;

		void info() const;
		void dump() const;
		bool recentlyUsed() const;
		void setRecentlyUsed(bool status);

		const Core::Time& startTime() const;
		const Core::Time& endTime() const;
		Core::TimeSpan length() const;

	protected:
	

	private:
		double _maxBufferSize;
		bool _recentlyUsed;


};
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



}
}
}


#endif
