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

#include <seiscomp/datamodel/waveformquality.h>
#include <seiscomp/plugins/qc/qcbuffer.h>
#include <seiscomp/plugins/qc/qcconfig.h>
#include <seiscomp/qc/qcprocessor_overlap.h>
#include "qcplugin_overlap.h"


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
namespace Seiscomp {
namespace Applications {
namespace Qc {


using namespace std;
using namespace Seiscomp::Processing;
using namespace Seiscomp::DataModel;


#define REGISTERED_NAME "QcOverlap"

IMPLEMENT_SC_CLASS_DERIVED(QcPluginOverlap, QcPlugin, "QcPluginOverlap");
ADD_SC_PLUGIN("Qc Parameter Overlap", "GFZ Potsdam <seiscomp-devel@gfz-potsdam.de>", 0, 1, 0)
REGISTER_QCPLUGIN(QcPluginOverlap, REGISTERED_NAME);
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
QcPluginOverlap::QcPluginOverlap(): QcPlugin() {
	_qcProcessor = new QcProcessorOverlap();
	_qcProcessor->subscribe(this);

	_name = REGISTERED_NAME;
	_parameterNames.push_back("overlaps interval");
	_parameterNames.push_back("overlaps length");
	_parameterNames.push_back("overlaps count");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void QcPluginOverlap::generateReport(const QcBuffer* buf) const {
	if ( buf->empty() ) {
		return;
	}

	vector<double> mean = _mean(buf);
	vector<double> stdDev = _stdDev(buf, mean[0], mean[1]);

	SEISCOMP_DEBUG("%s overlap count: %d   interval mean: %f   length mean: %f", _streamID.c_str(), (int)mean[2], mean[0], mean[1]);

	auto obj1 = new WaveformQuality();
	obj1->setWaveformID(getWaveformID(_streamID));
	obj1->setCreatorID(_app->creatorID());
	obj1->setCreated(Core::Time::GMT());
	obj1->setStart(buf->startTime());
	obj1->setEnd(buf->endTime());
	obj1->setType("report");
	obj1->setParameter(_parameterNames[0]);
	obj1->setValue(mean[0]);
	obj1->setLowerUncertainty(stdDev[0]);
	obj1->setUpperUncertainty(stdDev[0]);
	obj1->setWindowLength((double)buf->length());
	pushObject(Object::Cast(obj1));

	auto obj2 = new WaveformQuality();
	obj2->setWaveformID(getWaveformID(_streamID));
	obj2->setCreatorID(_app->creatorID());
	obj2->setCreated(Core::Time::GMT());
	obj2->setStart(buf->startTime());
	obj2->setEnd(buf->endTime());
	obj2->setType("report");
	obj2->setParameter(_parameterNames[1]);
	obj2->setValue(mean[1]);
	obj2->setLowerUncertainty(stdDev[1]);
	obj2->setUpperUncertainty(stdDev[1]);
	obj2->setWindowLength((double)buf->length());
	pushObject(Object::Cast(obj2));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void QcPluginOverlap::generateAlert(const QcBuffer *shortBuffer, const QcBuffer *longBuffer) const {
	if ( shortBuffer->empty() || longBuffer->empty() ) {
		return;
	}

	vector<double> sta = _mean(shortBuffer);
	vector<double> staStdDev = _stdDev(shortBuffer, sta[0], sta[1]);
	vector<double> lta = _mean(longBuffer);

	double iRelative = 0.0;
	double lRelative = 0.0;

	//! HACK
	if ( lta[0] != 0.0 ) {
		iRelative = fabs((lta[0] - sta[0]) / lta[0] * 100.0);
	}
	else if ( sta[0] != 0.0 ) {
		iRelative = 100.0;
	}

	if ( lta[1] != 0.0 ) {
		lRelative = fabs((lta[1] - sta[1]) / lta[1] * 100.0);
	}
	else if ( sta[1] != 0.0 ) {
		lRelative = 100.0;
	}

	if ( iRelative > *(_qcConfig->alertThresholds().begin()) ) { // HACK multi thresholds not yet implemented!
		auto obj1 = new WaveformQuality();
		obj1->setWaveformID(getWaveformID(_streamID));
		obj1->setCreatorID(_app->creatorID());
		obj1->setCreated(Core::Time::GMT());
		obj1->setStart(shortBuffer->startTime());
		obj1->setEnd(shortBuffer->endTime());
		obj1->setType("alert");
		obj1->setParameter(_parameterNames[0]);
		obj1->setValue(sta[0]);
		obj1->setLowerUncertainty(staStdDev[0]);
		obj1->setUpperUncertainty(staStdDev[0]);
		obj1->setWindowLength((double)shortBuffer->length());

		pushObject(Object::Cast(obj1));
	}

	if ( lRelative > *(_qcConfig->alertThresholds().begin()) ) { // HACK multi thresholds not yet implemented!
		auto obj2 = new WaveformQuality();
		obj2->setWaveformID(getWaveformID(_streamID));
		obj2->setCreatorID(_app->creatorID());
		obj2->setCreated(Core::Time::GMT());
		obj2->setStart(shortBuffer->startTime());
		obj2->setEnd(shortBuffer->endTime());
		obj2->setType("alert");
		obj2->setParameter(_parameterNames[1]);
		obj2->setValue(sta[1]);
		obj2->setLowerUncertainty(staStdDev[1]);
		obj2->setUpperUncertainty(staStdDev[1]);
		obj2->setWindowLength((double)shortBuffer->length());

		pushObject(Object::Cast(obj2));
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
vector<double> QcPluginOverlap::_mean(const QcBuffer *buf) const {
	vector<double> returnVector(3);
	returnVector[0] = 0.0; // Interval
	returnVector[1] = 0.0; // Length
	returnVector[2] = 0.0; // Count

	if ( buf->size() < 1 ) {
		return returnVector;
	}

	if ( buf->size() == 1 ) {
		returnVector[0] = 0.0;
		returnVector[1] = boost::any_cast<double>(buf->front()->parameter);
		returnVector[2] = 1.0;
		return returnVector;
	};

	Core::Time lastOverlapTime;
	double iSum = 0.0;
	double lSum = 0.0;
	int count = 0;

	for (auto p = buf->begin(); p != buf->end(); p++) {
		double diff = boost::any_cast<double>((*p)->parameter);

		if ( count != 0 ) {
			iSum += (double)((*p)->recordStartTime - lastOverlapTime);
		}

		lSum += diff;
		count++;
		lastOverlapTime = (*p)->recordStartTime;
	}

	if ( count > 0 ) {
		if ( count > 1 ) {
			returnVector[0] = iSum / (count -1);
		}

		returnVector[1] = lSum / count;
		returnVector[2] = (double)count;
	}

	return returnVector;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
vector<double> QcPluginOverlap::_stdDev(const QcBuffer *buf, double iMean, double lMean) const {
	vector<double> returnVector(2);
	returnVector[0] = 0.0;
	returnVector[1] = 0.0;

	if ( buf->size() < 2 ) {
		return returnVector;
	}

	Core::Time lastOverlapTime;
	double iSum = 0.0;
	double lSum = 0.0;
	int count = 0;

	for ( auto p = buf->begin(); p != buf->end(); p++ ) {
		double diff = boost::any_cast<double>((*p)->parameter);

		if ( count != 0.0 ) {
			iSum += pow(static_cast<double>(((*p)->recordStartTime - lastOverlapTime)) - iMean, 2);
		}

		lSum += pow(diff - lMean, 2);
		++count;
		lastOverlapTime = (*p)->recordStartTime;
	}

	if ( count > 1 ) {
		if ( count > 2 ) {
			returnVector[0] = sqrt(iSum / (count - 2));
		}

		returnVector[1] = sqrt(lSum / (count - 1));
	}

	return returnVector;
}

}
}
}

