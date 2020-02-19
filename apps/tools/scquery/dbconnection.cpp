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



#include "dbconnection.h"

#include <iomanip>
#include <sstream>
#include <iostream>
#include <string.h>

#define SEISCOMP_COMPONENT scquery
#include <seiscomp/logging/log.h>

#if WIN32
#undef min
#undef max
#endif



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
DBConnection::DBConnection(Seiscomp::IO::DatabaseInterface *db) : _db(db)
{}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
DBConnection::~DBConnection() {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<





// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool DBConnection::executeQuery(const DBQuery& query)
{
	bool success = false;
	if (!_db)
		return success;
	
	if (!_table.empty())
		_table.clear();

	success = _db->beginQuery(query.query().c_str());
	if (!success)
	{
		_db->endQuery();
		return success;
	}

	int rowFieldCount = _db->getRowFieldCount();
	std::vector<size_t> maxRowStrLength(rowFieldCount, 0);
	while (_db->fetchRow())
	{
		std::vector<std::string> row(rowFieldCount, "");
		for (int column = 0; column < rowFieldCount; ++column)
		{
			const char* element = static_cast<const char*>(_db->getRowField(column));
			if (element)
			{
				row[column] = element;
				maxRowStrLength[column] = std::max(maxRowStrLength[column], strlen(element));
			}
			else
			{
				row[column] = "NULL";
				maxRowStrLength[column] = std::max(maxRowStrLength[column], strlen("NULL"));
			}
		}
		_table.push_back(row);
	}
	_db->endQuery();
	// Convert table to string
	std::ostringstream os;
	for (size_t row = 0; row < _table.size(); ++row)
	{
		for (int column = 0; column < rowFieldCount; ++column)
		{
			os << std::setw(maxRowStrLength[column]) << std::left << _table[row][column];
			if (column < rowFieldCount - 1)
				os << " | ";
		}
		os << std::endl;
	}
	os.unsetf(std::cout.flags());
	_tableStr = os.str();
	
	return success;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const std::string& DBConnection::table() const
{
	return _tableStr;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
