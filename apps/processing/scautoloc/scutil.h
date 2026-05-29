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


#ifndef SEISCOMP_AUTOLOC_SCUTIL_H_INCLUDED
#define SEISCOMP_AUTOLOC_SCUTIL_H_INCLUDED

#include <seiscomp/datamodel/origin.h>

namespace Seiscomp {

	void logObjectCounts();

	bool manual(const Seiscomp::DataModel::Origin*);
	bool preliminary(const Seiscomp::DataModel::Origin*);

	// Pick label used for logging.
	// The goal is to display much information as possible as a relatively short string.
	std::string pickLabel(const Seiscomp::DataModel::Pick*);

	bool rejected(const Seiscomp::DataModel::Pick*);

	std::string evaluationStatus(const Seiscomp::DataModel::Pick*);
	std::string evaluationStatus(const Seiscomp::DataModel::Origin*);
	std::string evaluationMode(const Seiscomp::DataModel::Origin*);
	std::string depthType(const Seiscomp::DataModel::Origin*);

	std::string summary(const Seiscomp::DataModel::Origin*);

} // namespace Seiscomp

#endif
