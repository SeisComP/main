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



bool deletePath(DatabaseInterface *db, const vector<string> &path,
                const string &tableOverride, DatabaseInterface::OID oid) {
	const string &table = tableOverride.empty() ? path.back() : tableOverride;
	stringstream ss;

	if ( tableOverride.empty() && path.size() == 2 ) {
		ss << "DELETE FROM " << table
		   << " WHERE " << path[1] << "._parent_oid=" << oid;
	}
	else {
		if ( db->backend() != DatabaseInterface::MySQL ) {
			ss << "DELETE FROM " << table << " WHERE _oid IN (" << endl;
			ss << "  SELECT " << path.back() << "._oid" << endl
			    << "  FROM ";
			for ( size_t i = 1; i < path.size(); ++i ) {
				if ( i > 1 ) {
					ss << ", ";
				}
				ss << path[i];
			}
			ss << endl;

			ss << "  WHERE ";

			for ( size_t i = 1; i < path.size(); ++i ) {
				if ( i > 1 ) {
					ss << "    AND ";
				}
				ss << path[i] << "._parent_oid=";
				if ( i > 1 ) {
					ss << path[i-1] << "._oid";
				}
				else {
					ss << oid;
				}
				ss << endl;
			}

			ss << ")";
		}
		else {
			// Optimized MySQL version
			auto tables = path.size();
			ss << "DELETE " << table << " FROM ";
			if ( !tableOverride.empty() ) {
				ss << table << ", ";
			}
			for ( size_t i = 1; i < tables; ++i ) {
				if ( i > 1 ) {
					ss << ", ";
				}
				ss << path[i];
			}
			ss << " WHERE ";

			if ( !tableOverride.empty() ) {
				ss << table << "._oid=" << path.back() << "._oid AND ";
			}

			for ( size_t i = 1; i < path.size(); ++i ) {
				if ( i > 1 ) {
					ss << " AND ";
				}
				ss << path[i] << "._parent_oid=";
				if ( i > 1 ) {
					ss << path[i-1] << "._oid";
				}
				else {
					ss << oid;
				}
			}
		}
	}

	return db->execute(ss.str().c_str());
}


bool dumpPath(DatabaseInterface *db, vector<string> &path,
              const string &type, DatabaseInterface::OID oid) {
	auto f = Core::ClassFactory::FindByClassName(type.c_str());
	if ( !f ) {
		SEISCOMP_ERROR("Class %s not registered", type.c_str());
		return false;
	}

	auto meta = f->meta();
	if ( !meta ) {
		SEISCOMP_ERROR("Class %s has no meta record", type.c_str());
		return false;
	}

	path.push_back(type);

	auto cnt = meta->propertyCount();
	for ( decltype(meta->propertyCount()) i = 0; i < cnt; ++i ) {
		auto p = meta->property(i);
		if ( p->isArray() && p->isClass() ) {
			if ( !dumpPath(db, path, p->type(), oid) ) {
				return false;
			}
		}
	}

	bool isPublicObject = meta->rtti()->isTypeOf(DataModel::PublicObject::TypeInfo());

	if ( !deletePath(db, path, "Object", oid) ) {
		return false;
	}

	if ( isPublicObject ) {
		if ( !deletePath(db, path, "PublicObject", oid) ) {
			return false;
		}
	}

	if ( !deletePath(db, path, string(), oid) ) {
		return false;
	}

	path.pop_back();
	return true;
}

bool deleteTree(DatabaseInterface *db,
                const ClassFactory *factory,
                DatabaseInterface::OID oid) {
	auto *meta = factory->meta();

	vector<string> typePath;
	typePath.push_back(factory->className());

	auto cnt = meta->propertyCount();
	for ( decltype(meta->propertyCount()) i = 0; i < cnt; ++i ) {
		auto p = meta->property(i);
		if ( p->isArray() && p->isClass() ) {
			if ( !dumpPath(db, typePath, p->type(), oid) ) {
				return false;
			}
		}
	}

	return true;
}

bool deleteTree(DatabaseInterface *db, const ClassFactory *factory, const string &publicID) {
	if ( !db || !factory ) {
		return false;
	}

	DatabaseInterface::OID oid = DatabaseInterface::INVALID_OID;
	stringstream ss;
	string escapedPublicID;

	bool status = true;
	if ( db->escape(escapedPublicID, publicID) ) {
		ss << "SELECT _oid FROM PublicObject WHERE "
		   << db->convertColumnName("publicID") << "='" << escapedPublicID << "'";

		if ( db->beginQuery(ss.str().c_str()) ) {
			if ( db->fetchRow() ) {
				if ( !Core::fromString(oid, static_cast<const char*>(db->getRowField(0))) ) {
					SEISCOMP_ERROR("Invalid oid read from db: %s", static_cast<const char*>(db->getRowField(0)));
					status = false;
				}
			}
			else {
				SEISCOMP_ERROR("Object with id %s not found", publicID);
				status = false;
			}

			db->endQuery();

			if ( status ) {
				status = deleteTree(db, factory, oid);
			}
		}
	}

	return status;
}


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
			setLoadConfigModuleEnabled(false);
			setLoadInventoryEnabled(false);
			addMessagingSubscription("*");
		}


	protected:
		void createCommandLineDescription() override {
			commandline().addOption("Messaging", "mode,m", "Listen mode [none, notifier, all]", &_listenMode, "notifier");
			commandline().addOption("Database", "output,o", "The database connection to write to.", &_databaseWriteConnection);

			commandline().addGroup("Import");
			commandline().addOption("Import", "input,i", "File to import. Provide multiple times to import multiple files.", &_importFiles);
			commandline().addOption("Import", "remove,r", "Remove objects found in import file.");

			commandline().addGroup("Operation");
			commandline().addOption("Operation", "wipe,x", "PublicObjects for which all child objects will be wiped out. A PublicObject is defined as {type}[:{publicID}], e.g. Origin:123. "
			                        "If the colon and publicID is omitted then the publicID is equal to the type, e.g. Inventory.", &_clearObjects);
		}


		bool validateParameters() override {
			if ( !Application::validateParameters() ) {
				return false;
			}

			_remove = commandline().hasOption("remove");

			if ( _listenMode != "none" ) {
				if ( _listenMode == "all" ) {
					_dbAllObjects = true;
				}
				else if ( _listenMode == "notifier" ) {
					_dbAllObjects = false;
				}
				else {
					cout << "Error: unknown listen mode '" << _listenMode << "'" << endl;
					return false;
				}
			}

			if ( commandline().hasOption("input") || !_clearObjects.empty() ) {
				setMessagingEnabled(false);
			}

			return true;
		}


		bool initConfiguration() override {
			if ( !Application::initConfiguration() ) return false;

			string dbType, dbParams;
			try { _serviceRequestGroup = configGetString("connection.requestGroup"); } catch (...) {}
			try { _serviceProvideGroup = configGetString("connection.provideGroup"); } catch (...) {}
			try { dbType = configGetString("output.type"); } catch (...) {}
			try { dbParams = configGetString("output.parameters"); } catch (...) {}

			if ( !dbType.empty() && !dbParams.empty() ) {
				_databaseWriteConnection = dbType + "://" + dbParams;
			}

			return true;
		}

		bool initSubscriptions() override {
			if ( _listenMode != "none" ) {
				return Application::initSubscriptions();
			}

			return true;
		}

		bool initDatabase() override {
			int errorCode = -1;
			if ( (_listenMode != "none") && !commandline().hasOption("input") && _clearObjects.empty() ) {
				SEISCOMP_DEBUG("Checking database '%s'", _databaseWriteConnection.c_str());

				DatabaseInterfacePtr db = DatabaseInterface::Open(_databaseWriteConnection.c_str());
				if ( !db ) {
					SEISCOMP_ERROR("Could not open output database '%s'", _databaseWriteConnection.c_str());
					Application::exit(errorCode);
					return false;
				}

				--errorCode;

				SEISCOMP_DEBUG("Database check...ok");

				setDatabase(db.get());
			}
			else if ( commandline().hasOption("input") || !_clearObjects.empty() ) {
				return Application::initDatabase();
			}
			else {
				if ( connection() ) {
					connection()->subscribe(_serviceRequestGroup.c_str());
				}
			}

			return true;
		}


		bool run() override {
			if ( _clearObjects.empty() && !commandline().hasOption("input") ) {
				// Online mode
				return Application::run();
			}

			for ( auto &objectID : _clearObjects ) {
				string type, publicID;

				// Find colon which is not escaped with '\'.
				bool isEscaped = false;
				size_t p = string::npos;
				for ( size_t i = 0; i < objectID.size(); ++i ) {
					if ( isEscaped ) {
						type += objectID[i];
						isEscaped = false;
					}
					else if ( objectID[i] == '\\' ) {
						isEscaped = true;
					}
					else if ( objectID[i] == ':' ) {
						p = i;
						break;
					}
					else {
						type += objectID[i];
					}
				}

				if ( p != string::npos ) {
					publicID = objectID.substr(p + 1);
				}
				else {
					publicID = type;
				}

				SEISCOMP_INFO("Wiping childs of %s '%s'", type, publicID);

				auto classInfo = ClassFactory::FindByClassName(type);
				if ( !classInfo ) {
					SEISCOMP_ERROR("Unknown object type: %s", type);
					return false;
				}

				if ( !classInfo->meta() ) {
					SEISCOMP_ERROR("No runtime type information for %s", type);
					return false;
				}

				if ( !deleteTree(query()->driver(), classInfo, publicID) ) {
					SEISCOMP_ERROR("Failed to wipe %s %s", type, publicID);
					return false;
				}
			}

			if ( commandline().hasOption("input") ) {
				DataModel::PublicObject::SetIdGeneration(true);
				for ( auto &filename : _importFiles ) {
					if ( !importDatabase(filename) ) {
						return false;
					}
				}
			}

			return true;
		}


		ServiceProvideMessage *createServiceResponse(ServiceRequestMessage* msg) {
			DatabaseRequestMessage *dbRequest = DatabaseRequestMessage::Cast(msg);
			if ( !dbRequest ) {
				SEISCOMP_DEBUG("Could not handle message of type '%s' -> ignoring", msg->className());
				return nullptr;
			}

			if ( !Core::isEmpty(dbRequest->service()) && _settings.database.type != dbRequest->service() )
				return nullptr;

			if ( _settings.database.type.empty() || _settings.database.parameters.empty() )
				return nullptr;

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
			for ( MessageIterator it = msg->iter(); *it; ++it ) {
				if ( _dbAllObjects ) {
					DataModel::Object* object = DataModel::Object::Cast(*it);
					if ( object ) {
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
			if ( filename == "-" ) {
				ar.open(std::cin.rdbuf());
			}
			else if ( !ar.open(filename.c_str()) ) {
				cerr << "Error: could not open input file '" << filename << "'" << endl;
				return false;
			}

			cerr << "Parsing file '" << filename << "'..." << endl;

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
				cerr << "Error: no valid object found in file '" << filename << "'" << endl;
				return false;
			}

			ObjectWriter writer(*query(), !_remove, ObjectCounter(doc.get()).count(), 78);

			cout << "Time needed to parse XML: " << timer.elapsed() << endl;
			cout << "Document object type: " << doc->className() << endl;
			cout << "Total number of objects: " << ObjectCounter(doc.get()).count() << endl;

			cout << "Writing " << doc->className() << " into database" << endl;
			timer.restart();

			writer(doc.get());
			cout << endl;

			cout << "While writing " << writer.count() << " objects " << writer.errors() << " errors occured" << endl;
			cout << "Time needed to write " << writer.count() << " objects: " << timer.elapsed() << endl;

			if ( writer.errors() > 0 ) {
				_returnCode = 1;
			}

			return true;
		}


	private:
		std::vector<std::string> _importFiles;
		std::vector<std::string> _clearObjects;
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

