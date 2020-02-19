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


#ifndef __SYNC_STAXML_CONVERTER_H__
#define __SYNC_STAXML_CONVERTER_H__


#include <seiscomp/core/baseobject.h>


namespace Seiscomp {


class Converter {
	public:
		Converter() : _interrupted(false) {}
		virtual ~Converter() {}

	public:
		virtual void interrupt() { _interrupted = true; }

	protected:
		bool _interrupted;
};


}


#endif
