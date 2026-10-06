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



using namespace boost;
using namespace std;
using namespace Seiscomp;


void showQueries(const Config::Config &conf) {
	vector<string> sqlQueries;

	try {
		sqlQueries = conf.getStrings("queries");
	}
	catch ( Config::Exception & ) {
		cerr << "No query found" << endl;
		return;
	}

	cout << "[ " << sqlQueries.size() << " queries found ]\n"  << endl;
	for ( size_t i = 0; i < sqlQueries.size(); ++i ) {
		string desc, query;

		try { desc = conf.getString("query." + sqlQueries[i] + ".description"); } catch ( ... ) {}
		try { query = conf.getString("query." + sqlQueries[i]); } catch ( ... ) {}

		DBQuery q(sqlQueries[i], desc, query);
		cout << "Query name: " << q.name() << endl;
		cout << "Description: " << q.description() << endl;
		if ( q.hasParameter() ) {
			cout << "number of parameters: " << q.parameter().size() << endl;
			cout << "Parameter: ";
			for ( auto it = q.parameter().begin(); it < q.parameter().end(); ++it ) {
				cout << *it << " ";
			}
		}
		else {
			cout << "number of parameters: none";
		}
		cout << endl;
		cout << endl;
	}
}




DBQuery *findQuery(const Config::Config &conf, const string &name) {
	DBQuery *q = nullptr;

	vector<string> sqlQueries;
	try {
		sqlQueries = conf.getStrings("queries");
	}
	catch ( const Config::Exception &e ) {
		cout << e.what() << endl;
		return q;
	}

	for ( size_t i = 0; i < sqlQueries.size(); ++i ) {
		string desc, query;

		if ( name == sqlQueries[i] ) {
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
			cout << "Usage:" << endl
			     << "  scquery [options] [queryname] parameter0 parameter1 ..."
			     << endl << endl
			     << "Query the database using predefined queries stored in 'queries.cfg'"
			     << endl;

			Client::Application::printUsage();

			cout << "Examples:" << endl;
			cout << "List all configured queries" << endl
			     << "  scquery --showqueries" << endl << endl;
			cout << "Use the 'eventFilter' query, additionally print the column names as header"
			     << endl
			     << "  scquery -d localhost --print-column-name eventFilter 50 52 10.5 12.5 2.5 5 2021-01-01 2022-01-01"
			     << endl;
		}

		bool run() {
			Config::Config queriesConf;
			if ( !queriesConf.readConfig(Environment::Instance()->configDir() + "/queries.cfg") ) {
				if ( !queriesConf.readConfig(Environment::Instance()->appConfigDir() + "/queries.cfg") ) {
					return false;
				}
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

			auto qParameter = commandline().unrecognizedOptions();

			if ( !qParameter.empty() ) {
				unique_ptr<DBQuery> q(findQuery(queriesConf, qParameter[0]));
				if ( q.get() ) {
					vector<string> params;
					auto it = qParameter.begin();
					copy(++it, qParameter.end(), back_inserter(params));

					if ( !q->setParameter(params) ) {
						cerr << "The amount of parameter is not corresponding with given query!" << endl;
						cerr << "Given arguments: ";
						for ( size_t i = 0; i < params.size(); ++i ) {
							cerr << params[i] << " ";
						}
						cerr << endl;

						cerr << "Query parameter: ";
						for ( size_t i = 0; i < q->parameter().size(); ++i ) {
							cerr << q->parameter()[i] << " ";
						}
						cerr << endl;

						cerr << "Query: " << q->query() << endl;
						return false;
					}

					if ( _printOnly ) {
						cout << "Query:" << endl << q->query() << endl;
						return true;
					}

					DBConnection dbConnection(database());
					//cerr << *q << endl;
					if ( !dbConnection.executeQuery(*q, _columnName, _columnDelimiter ) ) {
						cerr << "Could not execute query: " << q->query() << endl;
					}
					if ( _header ) {
						cout << "# Name: " << q->name() << endl;
						cout << "# Description: " << q->description() << endl;
						cout << "# Query: " << q->query() << endl;
					}
					cout << dbConnection.table() << endl;
				}
				else {
					cout << "Could not execute query: " << qParameter[0] << endl;
				}
			}
			else if ( !_query.empty() ) {
				if ( _printOnly ) {
					cout << "Query:" << endl << _query << endl;
					return true;
				}

				DBQuery q("default", "default", _query);
				DBConnection dbConnection(database());

				if ( !dbConnection.executeQuery(q, _header, _columnDelimiter) ) {
					cerr << "Could not execute query: " << _query << endl;
				}

				if ( _header ) {
					cout << "# Name: " << q.name() << endl;
					cout << "# Description: " << q.description() << endl;
					cout << "# Query: " << q.query() << endl;
				}

				cout << dbConnection.table() << endl;
			}

			return true;
		}

	private:
		string _query;
		bool   _columnName{false};
		bool   _header{false};
		char   _columnDelimiter{'|'};
		bool   _printOnly{false};
};


int main(int argc, char* argv[]) {
	AppQuery app(argc, argv);
	return app.exec();
}
