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


#define SEISCOMP_COMPONENT SCDBTool

#include <seiscomp/logging/log.h>
#include <seiscomp/client/application.h>
#include <seiscomp/io/archive/xmlarchive.h>
#include <seiscomp/messaging/messages/database.h>
#include <seiscomp/utils/timer.h>


using namespace std;
using namespace Seiscomp;
using namespace Seiscomp::Core;
using namespace Seiscomp::Client;
using namespace Seiscomp::IO;


class ObjectCounter : protected DataModel::Visitor {
	public:
		ObjectCounter(DataModel::Object* object) : DataModel::Visitor(), _count(0) {
			object->accept(this);
		}

	public:
		unsigned int count() const { return _count; }

	protected:
		bool visit(DataModel::PublicObject *) override {
			++_count;
			return true;
		}
		
		void visit(DataModel::Object *) override {
			++_count;
		}

	private:
		unsigned int _count;
};


class ObjectWriter : public DataModel::DatabaseObjectWriter {
	// ----------------------------------------------------------------------
	//  Xstruction
	// ----------------------------------------------------------------------
	public:
		ObjectWriter(DataModel::DatabaseArchive& archive, bool addToDatabase,
		             unsigned int total, unsigned int totalProgress)
		: DataModel::DatabaseObjectWriter(archive, addToDatabase)
		, _total(total), _totalProgress(totalProgress)
		, _lastStep(0), _failure(0) {}


	// ----------------------------------------------------------------------
	//  Visitor interface
	// ----------------------------------------------------------------------
	protected:
		bool visit(DataModel::PublicObject* publicObject) override {
			bool result = DataModel::DatabaseObjectWriter::visit(publicObject);
			if ( !result )
				_failure += ObjectCounter(publicObject).count()-1;

			updateProgress();

			return result;
		}

		void visit(DataModel::Object* object) override {
			DataModel::DatabaseObjectWriter::visit(object);
			updateProgress();
		}

		void updateProgress() {
			unsigned int current = count() + _failure;
			unsigned int progress = current * _totalProgress / _total;
			if ( progress != _lastStep ) {
				_lastStep = progress;
				cout << "." << flush;
			}
		}

	private:
		unsigned int _total;
		unsigned int _totalProgress;
		unsigned int _lastStep;
		unsigned int _failure;
};



class DBTool : public Seiscomp::Client::Application {
	public:
		DBTool(int argc, char** argv) : Application(argc, argv) {
			setAutoApplyNotifierEnabled(false);
			setInterpretNotifierEnabled(false);
			setPrimaryMessagingGroup(Client::Protocol::LISTENER_GROUP);
			addMessagingSubscription("*");
		}


	protected:
		void createCommandLineDescription() override {
			commandline().addOption("Messaging", "mode,m", "listen mode [none, notifier, all]", &_listenMode, "notifier");
			commandline().addOption("Database", "output,o", "database connection to write to", &_databaseWriteConnection);

			commandline().addGroup("Import");
			commandline().addOption("Import", "input,i", "file to import. Provide multiple times to import multiple files", &_importFiles);
			commandline().addOption("Import", "remove,r", "remove objects found in import file");
		}


		bool validateParameters() override {
			if ( !Application::validateParameters() ) {
				return false;
			}

			_remove = commandline().hasOption("remove");

			if ( _listenMode != "none" ) {
				if ( _listenMode == "all" )
					_dbAllObjects = true;
				else if ( _listenMode == "notifier" )
					_dbAllObjects = false;
				else {
					cout << "Error: unknown listen mode '" << _listenMode << "'" << endl;
					return false;
				}
			}

			if ( commandline().hasOption("input") )
				setMessagingEnabled(false);

			return true;
		}


		bool initConfiguration() override {
			if ( !Application::initConfiguration() ) return false;

			string dbType, dbParams;
			try { _serviceRequestGroup = configGetString("connection.requestGroup"); } catch (...) {}
			try { _serviceProvideGroup = configGetString("connection.provideGroup"); } catch (...) {}
			try { dbType = configGetString("output.type"); } catch (...) {}
			try { dbParams = configGetString("output.parameters"); } catch (...) {}

			if ( !dbType.empty() && !dbParams.empty() )
				_databaseWriteConnection = dbType + "://" + dbParams;

			return true;
		}

		bool initSubscriptions() override {
			if ( _listenMode != "none" )
				return Application::initSubscriptions();

			return true;
		}

		bool initDatabase() override {
			int errorCode = -1;
			if ( _listenMode != "none" && !commandline().hasOption("input") ) {
				SEISCOMP_DEBUG("Checking database '%s'", _databaseWriteConnection.c_str());
	
				DatabaseInterfacePtr db = DatabaseInterface::Open(_databaseWriteConnection.c_str());
				if ( db == NULL ) {
					SEISCOMP_ERROR("Could not open output database '%s'", _databaseWriteConnection.c_str());
					Application::exit(errorCode);
					return false;
				}
				--errorCode;
		
				SEISCOMP_DEBUG("Database check...ok");
	
				setDatabase(db.get());
			}
			else if ( commandline().hasOption("input") )
				return Application::initDatabase();
			else {
				if ( connection() )
					connection()->subscribe(_serviceRequestGroup.c_str());
			}

			return true;
		}


		bool run() override {
			if ( commandline().hasOption("input") ) {
				for ( std::string filename: _importFiles ) {
					if ( !importDatabase(filename) ) {
						return false;
					}
				}
				return true;
			}

			return Application::run();
		}


		ServiceProvideMessage *createServiceResponse(ServiceRequestMessage* msg) {
			DatabaseRequestMessage *dbRequest = DatabaseRequestMessage::Cast(msg);
			if ( dbRequest == NULL ) {
				SEISCOMP_DEBUG("Could not handle message of type '%s' -> ignoring", msg->className());
				return NULL;
			}
		
			if ( !Core::isEmpty(dbRequest->service()) && _settings.database.type != dbRequest->service() )
				return NULL;
		
			if ( _settings.database.type.empty() || _settings.database.parameters.empty() )
				return NULL;
		
			return new DatabaseProvideMessage(_settings.database.type.c_str(), _settings.database.parameters.c_str());
		}


		void handleMessage(Message* msg) override {
			ServiceRequestMessage* serviceRequestMsg = ServiceRequestMessage::Cast(msg);
			if ( serviceRequestMsg ) {
				ServiceProvideMessagePtr serviceMsg = createServiceResponse(serviceRequestMsg);
				if ( serviceMsg ) {
					connection()->send(_serviceProvideGroup.c_str(), serviceMsg.get());
				}
		
				return;
			}

			DataModel::DatabaseObjectWriter writer(*query());
			for ( MessageIterator it = msg->iter(); *it != NULL; ++it ) {
				if ( _dbAllObjects ) {
					DataModel::Object* object = DataModel::Object::Cast(*it);
					if ( object != NULL ) {
						writer(object);
						continue;
					}
				}
		
				DataModel::Notifier* notifier = DataModel::Notifier::Cast(*it);
				if ( notifier && notifier->object() ) {
					DataModel::PublicObject *po = DataModel::PublicObject::Cast(notifier->object());
					if ( po )
						SEISCOMP_DEBUG("%s %s", notifier->operation().toString(), po->publicID().data());

					switch ( notifier->operation() ) {
						case DataModel::OP_ADD:
							writer(notifier->object(), notifier->parentID());
							//dbArchive->write(notifier->object(), notifier->parentID());
							break;
						case DataModel::OP_REMOVE:
							query()->remove(notifier->object(), notifier->parentID());
							break;
						case DataModel::OP_UPDATE:
							query()->update(notifier->object(), notifier->parentID());
							break;
						default:
							break;
					}
				}
			}
		}

		bool importDatabase(std::string filename) {
			XMLArchive ar;
			if ( filename == "-" )
				ar.open(std::cin.rdbuf());
			else if ( !ar.open(filename.c_str()) ) {
				cout << "Error: could not open input file '" << filename << "'" << endl;
				return false;
			}
		
			/*
			DatabaseInterfacePtr db = DatabaseInterface::Open(_databaseWriteConnection.c_str());
			if ( db == NULL ) {
				cout << "Error: could not connect to database '" << _databaseWriteConnection << "'" << endl;
				return false;
			}
			*/
		
			cout << "Parsing file '" << filename << "'..." << endl;
		
			Util::StopWatch timer;
			Core::BaseObjectPtr obj;
			ar >> obj;
			ar.close();

			if ( !obj ) {
				cout << "Error: no valid entry found in file '" << filename << "'" << endl;
				return false;
			}

			auto msg = Core::Message::Cast(obj);
			if ( msg ) {
				handleMessage(msg);
				return true;
			}

			DataModel::ObjectPtr doc = DataModel::Object::Cast(obj);
		
			if ( !doc ) {
				cout << "Error: no valid object found in file '" << filename << "'" << endl;
				return false;
			}
		
			ObjectWriter writer(*query(), !_remove, ObjectCounter(doc.get()).count(), 78);
		
			cout << "Time needed to parse XML: " << Core::Time(timer.elapsed()).toString("%T.%f") << endl;
			cout << "Document object type: " << doc->className() << endl;
			cout << "Total number of objects: " << ObjectCounter(doc.get()).count() << endl;
		
			cout << "Writing " << doc->className() << " into database" << endl;
			timer.restart();
		
			writer(doc.get());
			cout << endl;
		
			cout << "While writing " << writer.count() << " objects " << writer.errors() << " errors occured" << endl;
			cout << "Time needed to write " << writer.count() << " objects: " << Core::Time(timer.elapsed()).toString("%T.%f") << endl;

			if ( writer.errors() > 0 ) {
				_returnCode = 1;
			}
			return true;
		}


	private:
		std::vector<std::string> _importFiles;
		bool _remove;

		std::string _listenMode;
		bool _dbAllObjects;

		std::string _databaseWriteConnection;
		std::string _serviceRequestGroup;
		std::string _serviceProvideGroup;
};


int main(int argc, char** argv) {
	DBTool app(argc, argv);
	return app.exec();
}

