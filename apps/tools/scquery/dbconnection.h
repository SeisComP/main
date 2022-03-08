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



#ifndef __DBCONNECTION_H__
#define __DBCONNECTION_H__

#include <string>
#include <vector>

#include <seiscomp/io/database.h>

#include "dbquery.h"


class DBConnection
{

	// ------------------------------------------------------------------
	// X'struction
	// ------------------------------------------------------------------
	public:
		DBConnection(Seiscomp::IO::DatabaseInterface*);
		~DBConnection();


	// ------------------------------------------------------------------
	// Public interface
	// ------------------------------------------------------------------
	public:
		bool executeQuery(const DBQuery& query, bool withHeader, char delimiter);

		const std::string& table() const;


	// ------------------------------------------------------------------
	// Private data members
	// ------------------------------------------------------------------
	private:
		Seiscomp::IO::DatabaseInterfacePtr     _db;
		std::vector<std::vector<std::string> > _table;
		std::string                            _tableStr;
};

#endif
