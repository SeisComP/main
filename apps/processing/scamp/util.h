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




#ifndef SEISCOMP_APPLICATIONS_AMPTOOL_UTIL_H__
#define SEISCOMP_APPLICATIONS_AMPTOOL_UTIL_H__


#include <seiscomp/datamodel/types.h>
#include <seiscomp/datamodel/waveformstreamid.h>


namespace Seiscomp {

namespace DataModel {

class Origin;
class Arrival;
class Amplitude;
class WaveformStreamID;

}

namespace Private {

bool equivalent(const DataModel::WaveformStreamID&, const DataModel::WaveformStreamID&);

double
arrivalWeight(const DataModel::Arrival *arr, double defaultWeight=1.);

double
arrivalDistance(const DataModel::Arrival *arr);

DataModel::EvaluationStatus
status(const DataModel::Origin *origin);

char
shortPhaseName(const std::string &phase);

std::string
toStreamID(const DataModel::WaveformStreamID &wfid);

DataModel::WaveformStreamID
setStreamComponent(const DataModel::WaveformStreamID &wfid, char comp);

std::ostream &
operator<<(std::ostream &os, const DataModel::Amplitude &ampl);


}
}


#endif

