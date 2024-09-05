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



#define SEISCOMP_COMPONENT scquery

#include <iostream>
#include <memory>
#include <algorithm>

#include <seiscomp/client/application.h>

#include "dbquery.h"
#include "dbconnection.h"



using namespace Seiscomp;
using namespace boost;


void showQueries(const Config::Config& conf)
{
	std::vector<std::string> sqlQueries;

	try {
		sqlQueries = conf.getStrings("queries");
	}
	catch ( Config::Exception & ) {
		std::cerr << "No query found" << std::endl;
		return;
	}

	std::cout << "[ " << sqlQueries.size() << " queries found ]\n"  << std::endl;
	for ( size_t i = 0; i < sqlQueries.size(); ++i ) {
		std::string desc, query;

		try { desc = conf.getString("query." + sqlQueries[i] + ".description"); } catch ( ... ) {}
		try { query = conf.getString("query." + sqlQueries[i]); } catch ( ... ) {}

		DBQuery q(sqlQueries[i], desc, query);
		std::cout << "Query name: " << q.name() << std::endl;
		std::cout << "Description: " << q.description() << std::endl;
		if (q.hasParameter()) {
			std::cout << "number of parameters: " << q.parameter().size() << std::endl;
			std::cout << "Parameter: ";
			for (std::vector<std::string>::const_iterator it = q.parameter().begin();
			        it < q.parameter().end(); ++it)
				std::cout << *it << " ";
		}
		else {
			std::cout << "number of parameters: none";
		}
		std::cout << std::endl;
		std::cout << std::endl;
	}
}




DBQuery* findQuery(const Config::Config& conf, const std::string& name)
{
	DBQuery* q = nullptr;

	std::vector<std::string> sqlQueries;
	try
	{
		sqlQueries = conf.getStrings("queries");
	}
	catch (const Config::Exception& e)
	{
		std::cout << e.what() << std::endl;
		return q;
	}

	for (size_t i = 0; i < sqlQueries.size(); ++i)
	{
		std::string desc, query;

		if (name == sqlQueries[i])
		{
			try { desc = conf.getString("query." + sqlQueries[i] + ".description"); } catch ( ... ) {}
			try { query = conf.getString("query." + sqlQueries[i]); } catch ( ... ) {}

			q = new DBQuery(sqlQueries[i], desc, query);
			break;
		}
	}
	return q;
}


class AppQuery : public Client::Application {
	public:
		AppQuery(int argc, char** argv) :
			Client::Application(argc, argv) {
			setMessagingEnabled(false);
			setDatabaseEnabled(true, false);
		}

	protected:
		void createCommandLineDescription() {
			commandline().addGroup("Commands");
			commandline().addOption("Commands", "showqueries",
			                        "Show the queries defined in 'queries.cfg'.");
			commandline().addOption("Commands", "delimiter",
			                        "Column delimiter. If found, this character "
			                        "will be escaped in output values.",
			                        &_columnDelimiter);
			commandline().addOption("Commands", "print-column-name",
			                        "Print the name of each output column in a "
			                        "header.");
			commandline().addOption("Commands", "print-header",
			                        "Print the query parameters and the query filter "
			                        "description as a header of the query output.");
			commandline().addOption("Commands", "print-query-only",
			                        "Only print the full query to stdout and "
			                        "then quit.");
			commandline().addOption("Commands", "query,Q",
			                        "Execute the given query instead of applying "
			                        "queries pre-defined by configuration.",
			                        &_query);
		}

		bool validateParameters() {
			if ( !Seiscomp::Client::Application::validateParameters() ) {
				return false;
			}

			if ( commandline().hasOption("showqueries") ) {
				 setDatabaseEnabled(false, false);
			}
			else {
				setDatabaseEnabled(true, false);
			}
			return true;
		}

		void printUsage() const {
			std::cout << "Usage:" << std::endl << "  scquery [options] [queryname] parameter0 parameter1 ..."
		              << std::endl << std::endl
			          << "Query the database using predefined queries stored in 'queries.cfg'"
			          << std::endl;

			Client::Application::printUsage();

			std::cout << "Examples:" << std::endl;
			std::cout << "List all configured queries" << std::endl
			          << "  scquery --showqueries" << std::endl << std::endl;
			std::cout << "Use the 'eventFilter' query, additionally print the column names as header"
			          << std::endl
			          << "  scquery -d localhost --print-column-name eventFilter 50 52 10.5 12.5 2.5 5 2021-01-01 2022-01-01"
			          << std::endl;
		}

		bool run() {
			Config::Config queriesConf;
			if ( !queriesConf.readConfig(Environment::Instance()->configDir() + "/queries.cfg") ) {
				if ( !queriesConf.readConfig(Environment::Instance()->appConfigDir() + "/queries.cfg") )
					return false;
			}

			if ( commandline().hasOption("showqueries") ) {
				showQueries(queriesConf);
				return true;
			}

			if ( commandline().hasOption("print-column-name") ) {
				_columnName = true;
			}

			if ( commandline().hasOption("print-header") ) {
				_header = true;
			}

			if ( commandline().hasOption("print-query-only") ) {
				_printOnly = true;
			}
			std::vector<std::string> qParameter = commandline().unrecognizedOptions();

			if (!qParameter.empty())
			{
				std::unique_ptr<DBQuery> q(findQuery(queriesConf, qParameter[0]));
				if (q.get())
				{
					std::vector<std::string> params;
					std::vector<std::string>::iterator it = qParameter.begin();
					std::copy(++it, qParameter.end(), std::back_inserter(params));

					if (!q->setParameter(params))
					{
						std::cerr << "The amount of parameter is not corresponding with given query!" << std::endl;
						std::cerr << "Given arguments: ";
						for (size_t i = 0; i < params.size(); ++i) {
							std::cerr << params[i] << " ";
						}
						std::cerr << std::endl;

						std::cerr << "Query parameter: ";
						for (size_t i = 0; i < q->parameter().size(); ++i) {
							std::cerr << q->parameter()[i] << " ";
						}
						std::cerr << std::endl;

						std::cerr << "Query: " << q->query() << std::endl;
						return false;
					}

					if ( _printOnly ) {
						std::cout << "Query:" << std::endl << q->query() << std::endl;
						return true;
					}

					DBConnection dbConnection(database());
					//std::cerr << *q << std::endl;
					if ( !dbConnection.executeQuery(*q, _columnName, _columnDelimiter ) ) {
						std::cerr << "Could not execute query: " << q->query() << std::endl;
					}
					if ( _header ) {
						std::cout << "# Name: " << q->name() << std::endl;
						std::cout << "# Description: " << q->description() << std::endl;
						std::cout << "# Query: " << q->query() << std::endl;
					}
					std::cout << dbConnection.table() << std::endl;
				}
				else
				{
					std::cout << "Could not execute query: " << qParameter[0] << std::endl;
				}
			}
			else if ( !_query.empty() ) {
				if ( _printOnly ) {
					std::cout << "Query:" << std::endl << _query << std::endl;
					return true;
				}

				DBQuery q("default", "default", _query);
				DBConnection dbConnection(database());
				if ( !dbConnection.executeQuery(q, _header, _columnDelimiter) ) {
					std::cerr << "Could not execute query: " << _query << std::endl;
				}
				if ( _header ) {
					std::cout << "# Name: " << q.name() << std::endl;
					std::cout << "# Description: " << q.description() << std::endl;
					std::cout << "# Query: " << q.query() << std::endl;
				}
				std::cout << dbConnection.table() << std::endl;
			}

			return true;
		}

	private:
		std::string _query;
		bool        _columnName{false};
		bool        _header{false};
		char        _columnDelimiter{'|'};
		bool        _printOnly{false};
};



int main(int argc, char* argv[])
{
	AppQuery app(argc, argv);
	return app.exec();
}
