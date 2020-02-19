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



#ifndef __DBQUERY_H__
#define __DBQUERY_H__

#include <string>
#include <vector>

#include <seiscomp/io/database.h>


class DBQuery
{
	
	// ------------------------------------------------------------------
	// X'struction
	// ------------------------------------------------------------------
public:
	DBQuery(const std::string& name,
	        const std::string& description,
	        const std::string& query);
	~DBQuery();

	
	// ------------------------------------------------------------------
	// Public interface
	// ------------------------------------------------------------------
public:
	void setQuery(const std::string& query);
	const std::string& query() const;
	
	const std::string& name() const;
	const std::string& description() const;
	
	bool hasParameter() const;
	const std::vector<std::string>& parameter() const;
	bool setParameter(const std::vector<std::string>& param);
		
	
	// ------------------------------------------------------------------
	// Private data members
	// ------------------------------------------------------------------
private:
	std::string              _name;
	std::string              _description;
	std::string              _query;
	std::string              _stopWord;
	std::vector<std::string> _parameter;
};


// ------------------------------------------------------------------
// Operators
// ------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, const DBQuery& query);

#endif
