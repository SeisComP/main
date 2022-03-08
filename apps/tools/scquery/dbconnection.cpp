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
const std::string& escapeString(std::string &out, const std::string& str, const char delim) {
	std::string::size_type pos = str.find(delim);
	if ( pos == std::string::npos ) {
		out += str;
		return out;
	}

	if ( pos > 0 )
		out.append(str, 0, pos);

	++pos;
	out.push_back('\\');
	out.push_back(delim);
	while ( pos < str.size() ) {
		if ( str[pos] == delim )
			out.push_back('\\');
		out.push_back(str[pos]);
		++pos;
	}

	return out;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
DBConnection::DBConnection(Seiscomp::IO::DatabaseInterface *db) : _db(db)
{}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
DBConnection::~DBConnection() {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool DBConnection::executeQuery(const DBQuery& query, bool withHeader, char delimiter)
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
	std::vector<std::string> rowHeader(rowFieldCount, "");
	bool fetchHeader = withHeader;

	while (_db->fetchRow())
	{
		std::vector<std::string> row(rowFieldCount, "");
		for (int column = 0; column < rowFieldCount; ++column)
		{
			const char* element = static_cast<const char*>(_db->getRowField(column));
			if (element)
			{
				escapeString(row[column], element, delimiter);
				maxRowStrLength[column] = std::max(maxRowStrLength[column], strlen(element));
			}
			else
			{
				row[column] = "NULL";
				maxRowStrLength[column] = std::max(maxRowStrLength[column], strlen("NULL"));
			}

			if ( fetchHeader ) {
				escapeString(rowHeader[column], _db->getRowFieldName(column), delimiter);
				maxRowStrLength[column] = std::max(maxRowStrLength[column], rowHeader[column].size());
			}
		}
		_table.push_back(row);
		fetchHeader = false;
	}
	_db->endQuery();

	// Convert table to string
	std::ostringstream os;

	if ( withHeader and _table.size() > 0 ) {
		for ( int i = 0; i < rowFieldCount; ++i ) {
			os << std::setw(maxRowStrLength[i]) << std::left << rowHeader[i];
			if ( i < rowFieldCount - 1 ) {
				os << " " << delimiter << " ";
			}
		}
		os << std::endl;
		/*
		for ( int i = 0; i < rowFieldCount; ++i ) {
			for ( size_t c = 0; c < maxRowStrLength[i]; ++c ) {
				os << "-";
			}
			if ( i < rowFieldCount - 1 ) {
				os << "-+-";
			}
		}
		os << std::endl;
		*/
	}

	for (size_t row = 0; row < _table.size(); ++row)
	{
		for (int column = 0; column < rowFieldCount; ++column)
		{
			os << std::setw(maxRowStrLength[column]) << std::left << _table[row][column];
			if (column < rowFieldCount - 1)
				os << " " << delimiter << " ";
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
