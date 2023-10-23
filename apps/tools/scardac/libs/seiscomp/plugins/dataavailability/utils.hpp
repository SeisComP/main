/***************************************************************************
 * Copyright (C) 2020 by gempa GmbH                                        *
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

#ifndef SEISCOMP_DATAAVAILABILITY_UTILS_HPP
#define SEISCOMP_DATAAVAILABILITY_UTILS_HPP

#include <seiscomp/core/strings.h>
#include <seiscomp/datamodel/waveformstreamid.h>

#include <vector>

namespace Seiscomp {
namespace DataAvailability {

inline std::string streamID(const DataModel::WaveformStreamID &wid) {
	return wid.networkCode() + "." + wid.stationCode() + "." +
	       wid.locationCode() + "." + wid.channelCode();
}

inline bool wfID(DataModel::WaveformStreamID &wid, const std::string &id) {
	std::vector<std::string> toks;
	if ( Seiscomp::Core::split(toks, id.c_str(), ".", false) != 4 ) {
		return false;
	}

	wid.setNetworkCode(toks[0]);
	wid.setStationCode(toks[1]);
	wid.setLocationCode(toks[2]);
	wid.setChannelCode(toks[3]);

	return true;
}

} // ns DataAvailability
} // ns Seiscomp

#endif // UTILS_HPP
