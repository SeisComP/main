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


#ifndef SEISCOMP_AUTOLOC_PICKLOG_H
#define SEISCOMP_AUTOLOC_PICKLOG_H


#include <string>
#include <iostream>
#include <fstream>


namespace Seiscomp {

namespace AutolocInternal {


class PickLog {
	public:
		PickLog() = default;
		void setPrefix(const std::string &p);
		void setEnabled(bool e);

		void log(const Pick *pick);

	private:
		void setFileName(const std::string &f);

		bool enabled {false};
		std::string prefix;
		std::string filename;
		std::ofstream file;
};



}  // namespace AutolocInternal

}  // namespace Seiscomp

#endif
