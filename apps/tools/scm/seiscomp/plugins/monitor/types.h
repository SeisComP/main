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

#ifndef SEISCOMP_APPLICATIONS_TYPES_H__
#define SEISCOMP_APPLICATIONS_TYPES_H__

#include <map>
#include <list>

#include <seiscomp/messaging/status.h>


namespace Seiscomp {
namespace Applications {

typedef std::map<Client::Status::Tag, std::string> ClientInfoData;

struct ClientTableEntry {
	ClientTableEntry(const ClientInfoData &data)
	: info(data), processed(false), printed(false) {}

	ClientTableEntry(const Client::Status &status)
	: processed(false), printed(false) {
		*this = status;
	}

	ClientTableEntry &operator=(const ClientInfoData &data) {
		if ( info != data ) {
			info = data;
			processed = false;
			printed = false;
		}
		return *this;
	}

	ClientTableEntry &operator=(const Client::Status &status) {
		ClientInfoData data;
		for ( int i = 0; i < Client::Status::ETagQuantity; ++i ) {
			Client::Status::Tag tag;
			if ( !status.values[i].empty() ) {
				tag.fromInt(i);
				data[tag] = status.values[i];
			}
		}

		return *this = data;
	}

	operator ClientInfoData &() { return info; }
	operator const ClientInfoData &() const { return info; }

	ClientInfoData info; //! The client information
	bool processed;      //! Has this entry already been seen by a plugin?
	bool printed;        //! Has this entry already been seen by an output
	                     //! plugin?
};

typedef std::list<ClientTableEntry> ClientTable;

} // namespace Applications
} // namespace Seiscomp

#endif
