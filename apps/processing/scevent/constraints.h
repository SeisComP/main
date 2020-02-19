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


#ifndef SEISCOMP_APPLICATIONS_EVENTTOOL_CONSTRAINTS_H__
#define SEISCOMP_APPLICATIONS_EVENTTOOL_CONSTRAINTS_H__

#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/focalmechanism.h>

#include <string>


namespace Seiscomp {

namespace Client {


struct Constraints {
	Constraints() : fixMw(false), fixType(false) {}

	std::string                    preferredMagnitudeType;
	std::string                    preferredOriginID;
	std::string                    preferredFocalMechanismID;
	bool                           fixMw;
	bool                           fixType;
	OPT(DataModel::EvaluationMode) preferredOriginEvaluationMode;
	OPT(DataModel::EvaluationMode) preferredFocalMechanismEvaluationMode;

	bool fixedOrigin() const;
	bool fixOrigin(const DataModel::Origin *org) const;
	bool fixOriginMode(const DataModel::Origin *org) const;

	bool fixedFocalMechanism() const;
	bool fixFocalMechanism(const DataModel::FocalMechanism *fm) const;
	bool fixFocalMechanismMode(const DataModel::FocalMechanism *fm) const;
};


}

}


#endif
