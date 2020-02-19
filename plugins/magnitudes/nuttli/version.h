/***************************************************************************
 * Copyright (C) gempa GmbH                                                *
 * All rights reserved.                                                    *
 * Contact: gempa GmbH (seiscomp-dev@gempa.de)                             *
 *                                                                         *
 * Author: Jan Becker                                                      *
 * Email: jabe@gempa.de                                                    *
 *                                                                         *
 * GNU Affero General Public License Usage                                 *
 * This file may be used under the terms of the GNU Affero                 *
 * Public License version 3.0 as published by the Free Software Foundation *
 * and appearing in the file LICENSE included in the packaging of this     *
 * file. Please review the following information to ensure the GNU Affero  *
 * Public License version 3.0 requirements will be met:                    *
 * https://www.gnu.org/licenses/agpl-3.0.html.                             *
 *                                                                         *
 * Other Usage                                                             *
 * Alternatively, this file may be used in accordance with the terms and   *
 * conditions contained in a signed written agreement between you and      *
 * gempa GmbH.                                                             *
 ***************************************************************************/


#ifndef SEISCOMP_CUSTOM_AMPLITUDE_VERSION_H__
#define SEISCOMP_CUSTOM_AMPLITUDE_VERSION_H__


#define MN_VERSION_MAJOR 0
#define MN_VERSION_MINOR 2
#define MN_VERSION_PATCH 0

#define MN_STR_HELPER(x) #x
#define MN_STR(x) MN_STR_HELPER(x)
#define MN_VERSION (MN_STR(MN_VERSION_MAJOR) "." MN_STR(MN_VERSION_MINOR) "." MN_STR(MN_VERSION_PATCH))

#endif
