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


#include <seiscomp/core/strings.h>
#include "config.h"


namespace Seiscomp {
namespace Client {


bool fromString(BindAddress &bind, const std::string &str) {
	size_t p;

	if ( str.empty() )
		return false;

	if ( str[0] == '[' ) {
		// IPv6 notion
		p = str.find(']');
		if ( p == std::string::npos )
			// Closing square bracket is required
			return false;

		std::string ip = str.substr(1, p-2);

		p = str.find(':', p+1);
		if ( p == std::string::npos ) {
			// Bind without port does not make sense
			return false;
		}

		if ( !bind.address.fromString(ip.c_str()) )
			return false;

		return Seiscomp::Core::fromString(bind.port, str.substr(p+1));
	}

	// IPv4 notion
	p = str.find(':');

	if ( p == std::string::npos ) {
		// It is just a port. Keep the address as it is.
		//bind.address = Seiscomp::Wired::Socket::IPAddress();
		return Seiscomp::Core::fromString(bind.port, str);
	}
	else {
		if ( !bind.address.fromString(str.substr(0, p).c_str()) )
			return false;
		return Seiscomp::Core::fromString(bind.port, str.substr(p+1));
	}
}


}
}
