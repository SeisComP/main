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


#include <seiscomp/core/datetime.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/origin.h>


bool dumpOrigin(const Seiscomp::DataModel::Origin *origin);

bool equivalent(const Seiscomp::DataModel::WaveformStreamID&, const Seiscomp::DataModel::WaveformStreamID&);

double arrivalWeight(const Seiscomp::DataModel::ArrivalPtr *arr, double defaultWeight=1.);
char getShortPhaseName(const std::string &phase);

bool validArrival(const Seiscomp::DataModel::Arrival *arr, double minWeight = 0.5);

Seiscomp::DataModel::EvaluationStatus
status(const Seiscomp::DataModel::Origin *origin);
