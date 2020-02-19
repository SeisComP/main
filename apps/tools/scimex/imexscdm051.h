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

#ifndef __IMEXIMPORTERSCDM051_H___
#define __IMEXIMPORTERSCDM051_H___

#include <seiscomp/datamodel/exchange/scdm051.h>


class ImexImporterScDm051 : public Seiscomp::DataModel::ImporterSCDM051 {
	public:
		ImexImporterScDm051();

};


class ImexExporterScDm051 : public Seiscomp::DataModel::ExporterSCDM051 {
	public:
		ImexExporterScDm051();
};

#endif
