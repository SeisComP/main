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

#define SEISCOMP_COMPONENT Dispatcher

#include <seiscomp/logging/log.h>
#include <seiscomp/client/application.h>
#include <seiscomp/io/archive/xmlarchive.h>
#include <seiscomp/messaging/connection.h>
#include <seiscomp/utils/timer.h>

#include <seiscomp/datamodel/databasearchive.h>
#include <seiscomp/datamodel/diff.h>
#include <seiscomp/datamodel/eventparameters_package.h>


using namespace std;
using namespace Seiscomp;
using namespace Seiscomp::DataModel;


typedef map<string, string> RoutingTable;


class BaseObjectDispatcher : protected Visitor {
	// ----------------------------------------------------------------------
	//  X'struction
	// ----------------------------------------------------------------------
	public:
		BaseObjectDispatcher(Visitor::TraversalMode tm,
		                     Client::Connection *connection, bool test)
		: Visitor(tm)
		, _connection(connection)
		, _errors(0)
		, _count(0)
		, _test(test)
		, _createNotifier(false) {}


	// ----------------------------------------------------------------------
	//  Public interface1
	// ----------------------------------------------------------------------
	public:
		enum ObjectTypes{
			Event = 1
		};

		//! Does the actual writing
		virtual bool operator()(Object *object) {
			_errors = 0;
			_count = 0;
			_loggedObjects.clear();

			if ( _createNotifier ) {
				_outputNotifier = new NotifierMessage;
			}

			object->accept(this);

			return _errors == 0;
		}

		void setCreateNotifierMsg(bool createNotifier) {
			_createNotifier = createNotifier;
		}

		NotifierMessagePtr getNotifierMsg() {
			return _outputNotifier;
		}

		void setRoutingTable(const RoutingTable &table) {
			_routingTable.clear();
			_routingTable = table;
		}

		void setSetIgnoreObject(int types) {
			_ignoreTypes = types;
		}

		//! Returns the number of handled objects
		int count() const { return _count; }

		//! Returns the number of errors while writing
		int errors() const { return _errors; }


	// ----------------------------------------------------------------------
	//  Protected interface
	// ----------------------------------------------------------------------
	protected:
		string indent(Object *object) {
			string tmp;
			while ( object->parent() != nullptr ) {
				tmp += "  ";
				object = object->parent();
			}
			return tmp;
		}

		void logObject(Object *object, Operation op, const std::string &group,
		               const std::string &additionalIndent = "") {
			PublicObject *po = PublicObject::Cast(object);
			if ( po != nullptr )
				_loggedObjects.insert(po->publicID());

			PublicObject *parent = object->parent();
			if ( parent != nullptr ) {
				if ( _loggedObjects.find(parent->publicID()) == _loggedObjects.end() )
					logObject(parent, OP_UNDEFINED, "");
			}

			std::stringstream ss;
			if ( op != OP_UNDEFINED )
				ss << char(toupper(op.toString()[0]));
			else
				ss << ' ';

			ss << "  " << additionalIndent << indent(object)
			     << object->className() << "(";

			if ( po != nullptr )
				ss << "'" << po->publicID() << "'";
			else {
				const Core::MetaObject *meta = object->meta();
				bool first = true;
				for ( size_t i= 0; i < meta->propertyCount(); ++i ) {
					const Core::MetaProperty *prop = meta->property(i);
					if ( prop->isIndex() ) {
						if ( !first ) ss << ",";
						if ( prop->type() == "string" )
							ss << "'" << prop->readString(object) << "'";
						else
							ss << prop->readString(object);
						first = false;
					}
				}
			}

			ss << ")";
			if ( !group.empty() ) {
				ss << " : " << group;
			}
			SEISCOMP_INFO("%s", ss.str().c_str());
		}


	// ----------------------------------------------------------------------
	//  Protected members
	// ----------------------------------------------------------------------
	protected:
		Client::Connection        *_connection;
		int                        _errors;
		int                        _count;
		RoutingTable               _routingTable;
		bool                       _test;
		std::set<std::string>      _loggedObjects;
		bool                       _createNotifier;
		int                        _ignoreTypes{};
		NotifierMessagePtr         _outputNotifier;
};
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<





// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
class ObjectDispatcher : public BaseObjectDispatcher {
	// ----------------------------------------------------------------------
	//  X'struction
	// ----------------------------------------------------------------------
	public:
		ObjectDispatcher(Client::Connection *connection,
		                 Operation op, bool test)
		: BaseObjectDispatcher(op != OP_REMOVE?Visitor::TM_TOPDOWN:Visitor::TM_BOTTOMUP,
		                       connection, test)
		, _operation(op) {}


	// ----------------------------------------------------------------------
	//  Public interface
	// ----------------------------------------------------------------------
	public:
		bool operator()(Object *object) {
			_parentID = "";
			return BaseObjectDispatcher::operator()(object);
		}

		void setOperation(Operation op) {
			_operation = op;
		}

		Operation operation() const {
			return _operation;
		}


	// ----------------------------------------------------------------------
	//  Visitor interface
	// ----------------------------------------------------------------------
	protected:
		bool visit(PublicObject* publicObject) {
			return write(publicObject);
		}

		void visit(Object* object) {
			write(object);
		}


	// ----------------------------------------------------------------------
	//  Implementation
	// ----------------------------------------------------------------------
	private:
		bool write(Object *object) {
			if ( SCCoreApp->isExitRequested() ) return false;

			PublicObject *parent = object->parent();

			if ( !parent ) {
				SEISCOMP_ERROR("No parent found for object %s", object->className());
				return false;
			}

			RoutingTable::iterator targetIt = _routingTable.find(object->className());
			PublicObject *p = parent;
			while ( (targetIt == _routingTable.end()) && (p != nullptr) ) {
				targetIt = _routingTable.find(p->className());
				p = p->parent();
			}

			if ( targetIt == _routingTable.end() ) {
				SEISCOMP_ERROR("! No routing for %s", object->className());
				return false;
			}

			logObject(object, _operation, targetIt->second);

			_parentID = parent->publicID();

			++_count;

			if ( _test ) return true;

			NotifierPtr notif = new Notifier(_parentID, _operation, object);

			if ( _createNotifier ) {
				_outputNotifier->attach(notif.get());
				return true;
			}

			NotifierMessage notifierMessage;
			notifierMessage.attach(notif.get());

			unsigned int counter = 0;
			while ( counter <= 4 ) {
				if ( _connection->send(targetIt->second, &notifierMessage) )
					return true;

				SEISCOMP_ERROR("Could not send object %s to %s@%s", 
				               object->className(), targetIt->second.c_str(),
				               _connection->source().c_str());
				if ( _connection->isConnected() ) break;
				++counter;
				sleep(1);
			}

			++_errors;
			return false;
		}


	// ----------------------------------------------------------------------
	// Private data members
	// ----------------------------------------------------------------------
	private:
		string    _parentID;
		Operation _operation;
};
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<





// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
class ObjectMerger : public BaseObjectDispatcher {
	// ----------------------------------------------------------------------
	//  X'struction
	// ----------------------------------------------------------------------
	public:
		ObjectMerger(Client::Connection *connection,
		             DatabaseReader *db, bool test, bool allowRemove)
		: BaseObjectDispatcher(Visitor::TM_TOPDOWN, connection, test)
		, _db(db)
		, _msgCount(0)
		, _allowRemove(allowRemove)
		{}


	// ----------------------------------------------------------------------
	//  Public interface
	// ----------------------------------------------------------------------
	public:
		bool operator()(Object *object) {
			_msgCount = 0;

			bool ret = BaseObjectDispatcher::operator()(object);

			flush();
			return ret;
		}


	// ----------------------------------------------------------------------
	//  Visitor interface
	// ----------------------------------------------------------------------
	protected:
		bool visit(PublicObject *po) {
			// Each PublicObject in a single message
			flush();

			PublicObject *parent = po->parent();

			if ( !parent ) {
				SEISCOMP_ERROR("! No parent found for object %s", po->className());
				return false;
			}

			if ( SCCoreApp->isExitRequested() ) {
				return false;
			}

			if ( _ignoreTypes & BaseObjectDispatcher::Event ) {
				EventPtr event;
				try {
					event = Event::Cast(po);
				}
				catch ( Core::ValueException & ) {}

				if ( event ) {
					SEISCOMP_DEBUG("Ignoring event with ID %s", event->publicID());
					return false;
				}
			}

			RoutingTable::iterator targetIt = _routingTable.find(po->className());
			PublicObject *p = parent;
			while ( (targetIt == _routingTable.end()) && (p != nullptr) ) {
				targetIt = _routingTable.find(p->className());
				p = p->parent();
			}

			if ( targetIt == _routingTable.end() ) {
				SEISCOMP_ERROR("! No routing for %s", po->className());
				return false;
			}

			_targetGroup = targetIt->second;

			PublicObjectPtr stored = _db->loadObject(po->typeInfo(), po->publicID());

			if ( !stored ) {
				write(parent, po, OP_ADD);
				return true;
			}

			string storedParent = _db->parentPublicID(stored.get());
			if ( storedParent != parent->publicID() ) {
				// Instead of losing information due to a re-parent
				// we just create a new publicID and so a copy of the underlying
				// object
				PublicObject::GenerateId(po);
				write(parent, po, OP_ADD);
				return true;
			}

			std::vector<NotifierPtr> diffs;
			Diff2 diff;
			diff.diff(stored.get(), po, parent->publicID(), diffs);

			// All equal
			if ( diffs.empty() ) {
				logObject(po, OP_UNDEFINED, "exists already");
				return false;
			}

			_inputIndent = indent(po);

			string tmpTG(_targetGroup);

			for ( size_t i = 0; i < diffs.size(); ++i ) {
				Notifier *n = diffs[i].get();

				if ( !_allowRemove && (n->operation() == OP_REMOVE) )
					// Block removes if requested
					continue;

				po = PublicObject::Cast(n->object());
				if ( po != nullptr ) {
					flush();
					targetIt = _routingTable.find(po->className());
					if ( targetIt != _routingTable.end() && targetIt->second != _targetGroup )
						_targetGroup = targetIt->second;
					else
						_targetGroup = tmpTG;
				}
				write(n->object()->parent(), n->object(), n->operation());
			}

			_inputIndent.clear();

			_targetGroup = tmpTG;

			return false;
		}

		// Do nothing: objects are handled during diffing the public object
		// tree.
		void visit(Object *obj) {
			PublicObject *parent = obj->parent();

			if ( !parent ) {
				SEISCOMP_ERROR("! No parent found for object %s", obj->className());
				return;
			}

			write(parent, obj, OP_ADD);
		}


	// ----------------------------------------------------------------------
	//  Implementation
	// ----------------------------------------------------------------------
	private:
		bool write(PublicObject *parent, Object *object, Operation op) {
			++_count;

			logObject(object, op, _targetGroup);

			NotifierPtr notif = new Notifier(parent->publicID(), op, object);

			if ( _createNotifier ) {
				_outputNotifier->attach(notif.get());
				return true;
			}

			if ( !_msg ) _msg = new NotifierMessage;

			_msg->attach(notif.get());

			return false;
		}

		void flush() {
			if ( !_msg || _createNotifier ) { // redundant check but better safe...
				return;
			}
			if ( _test ) {
				SEISCOMP_DEBUG("Would send %d notifiers to %s group",
				               _msg->size(), _targetGroup.c_str());
				++_msgCount;
				_msg = nullptr;
				return;
			}

			SEISCOMP_DEBUG("Send %d notifiers to %s group",
			               _msg->size(), _targetGroup.c_str());

			while ( !SCCoreApp->isExitRequested() ) {
				if ( _connection->send(_targetGroup.c_str(), _msg.get()) ) {
					_msg = nullptr;
					++_msgCount;
					return;
				}

				SEISCOMP_ERROR("Could not send message to %s@%s",
				               _targetGroup.c_str(), _connection->source().c_str());

				sleep(1);
			}

			_msg = nullptr;
			++_errors;
		}

		void logObject(Object *object, Operation op, const std::string &group) {
			if ( op != OP_REMOVE )
				BaseObjectDispatcher::logObject(object, op, group);
			else
				BaseObjectDispatcher::logObject(object, op, group, _inputIndent);
		}


	// ----------------------------------------------------------------------
	// Private data members
	// ----------------------------------------------------------------------
	private:
		DatabaseReader     *_db;
		NotifierMessagePtr  _msg;
		int                 _msgCount;
		string              _targetGroup;
		string              _inputIndent;
		bool                _allowRemove;
};
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<





// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
class ObjectCounter : protected Visitor {
	public:
		ObjectCounter(Object* object) : Visitor(), _count(0) {
			object->accept(this);
		}

	public:
		unsigned int count() const { return _count; }

	protected:
		bool visit(PublicObject*) {
			++_count;
			return true;
		}

		virtual void visit(Object*) {
			++_count;
		}

	private:
		unsigned int _count;
};
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
class DispatchTool : public Seiscomp::Client::Application {
	public:
		DispatchTool(int argc, char** argv)
		: Application(argc, argv)
		, _notifierOperation("merge")
		, _operation(OP_UNDEFINED) {

			setAutoApplyNotifierEnabled(false);
			setInterpretNotifierEnabled(false);
			setMessagingEnabled(true);
			setDatabaseEnabled(true, true);
			setLoggingToStdErr(true);
			setPrimaryMessagingGroup(Client::Protocol::LISTENER_GROUP);
			addMessagingSubscription("");

			_routingTable.insert(make_pair(Pick::ClassName(), "PICK"));
			_routingTable.insert(make_pair(Amplitude::ClassName(), "AMPLITUDE"));
			_routingTable.insert(make_pair(Origin::ClassName(), "LOCATION"));
			_routingTable.insert(make_pair(StationMagnitude::ClassName(), "MAGNITUDE"));
			_routingTable.insert(make_pair(Magnitude::ClassName(), "MAGNITUDE"));
			_routingTable.insert(make_pair(FocalMechanism::ClassName(), "FOCMECH"));
			_routingTable.insert(make_pair(Event::ClassName(), "EVENT"));
		}


	protected:
		void createCommandLineDescription() {
			commandline().addGroup("Dispatch");
			commandline().addOption("Dispatch", "no-events,e", "Do not send any "
			                        "event object. They will be ignored.");
			commandline().addOption("Dispatch", "create-notifier", "Do not send any object. "
			                        "All notifiers will be written to standard output in XML format.");
			commandline().addOption("Dispatch", "input,i",
			                        "File to dispatch to messaging",
			                        &_inputFile, false);
			commandline().addOption("Dispatch", "operation,O",
			                        "Notifier operation: add, update, remove, "
			                        "merge or merge-without-remove.",
			                        &_notifierOperation, true);
			commandline().addOption("Dispatch", "print-objects",
			                        "Print names of routable objects.");
			commandline().addOption("Dispatch", "print-routingtable",
			                        "Print routing table.");
			commandline().addOption("Dispatch", "routingtable",
			                        "Specify routing table as list of "
			                        "object:group pairs.",
			                        &_routingTableStr, false);
			commandline().addOption("Dispatch", "test", "Do not send any object.");
		}


		bool validateParameters() {
			if ( !Application::validateParameters() ) {
				return false;
			}

			if ( commandline().hasOption("routingtable") ) {
				vector<string> tableEntries;
				Core::split(tableEntries, _routingTableStr.c_str(), ",", false);

				vector<string>::iterator it = tableEntries.begin();
				_routingTable.clear();
				for ( ; it != tableEntries.end(); it++ ) {
					vector<string> entry;
					Core::split(entry, (*it).c_str(), ":", false);
					if ( entry.size() == 2 )
						_routingTable.insert(make_pair(entry[0], entry[1]));
				}
			}

			if ( _notifierOperation != "merge" && _notifierOperation != "merge-without-remove" ) {
				setDatabaseEnabled(false, false);
			}

			if ( commandline().hasOption("operation") ) {
				if ( _notifierOperation != "merge" && _notifierOperation != "merge-without-remove" ) {
					if ( !_operation.fromString(_notifierOperation) ) {
						SEISCOMP_ERROR("Notifier operation %s is not valid. Operations are "
						               "add, update, remove or merge", _notifierOperation.c_str());
						return false;
					}

					if ( _operation != OP_ADD && _operation != OP_REMOVE && _operation != OP_UPDATE ) {
						SEISCOMP_ERROR("Notifier operation %s is not valid. Operations are "
						               "add, update, remove or merge", _notifierOperation.c_str());
						return false;
					}
				}
			}
			else if ( commandline().hasOption("print-objects") ) {
				for ( auto &kv : _routingTable ) {
					cout << kv.first << endl;
				}
				setMessagingEnabled(false);
				setDatabaseEnabled(false ,false);
				return true;
			}
			else if ( commandline().hasOption("print-routingtable") ) {
				for ( auto &kv : _routingTable ) {
					cout << kv.first << ":" << kv.second << endl;
				}
				setMessagingEnabled(false);
				setDatabaseEnabled(false ,false);
				return true;
			}

			if ( !commandline().hasOption("input") ) {
				SEISCOMP_ERROR("No input given");
				return false;
			}

			if ( commandline().hasOption("test") ||
			     commandline().hasOption("create-notifier") ) {
				setMessagingEnabled(false);
			}

			return true;
		}


		virtual bool initSubscriptions() {
			return true;
		}


		bool run() {
			if ( commandline().hasOption("print-objects") ) {
				for ( auto &kv : _routingTable ) {
					cout << kv.first << endl;
				}
				return true;
			}

			if ( commandline().hasOption("print-routingtable") ) {
				for ( auto &kv : _routingTable ) {
					cout << kv.first << ":" << kv.second << endl;
				}
				return true;
			}

			if ( commandline().hasOption("input") ) {
				return processInput();
			}

			return true;
		}


		bool processInput() {
			PublicObject::SetRegistrationEnabled(false);

			IO::XMLArchive ar;
			if ( !ar.open(_inputFile.c_str()) ) {
				SEISCOMP_ERROR("Error: could not open input file '%s'", _inputFile.c_str());
				return false;
			}

			SEISCOMP_INFO("Parsing file '%s'...", _inputFile.c_str());

			Util::StopWatch timer;
			ObjectPtr doc;
			ar >> doc;
			ar.close();

			PublicObject::SetRegistrationEnabled(true);

			if ( !doc ) {
				SEISCOMP_ERROR("Error: no valid object found in file '%s'", _inputFile.c_str());
				return false;
			}

			BaseObjectDispatcher *dispatcher = nullptr;
			if ( _notifierOperation == "merge" )
				dispatcher = new ObjectMerger(connection(), query(), commandline().hasOption("test"), true);
			else if ( _notifierOperation == "merge-without-remove" )
				dispatcher = new ObjectMerger(connection(), query(), commandline().hasOption("test"), false);
			else
				dispatcher = new ObjectDispatcher(connection(), _operation, commandline().hasOption("test"));

			dispatcher->setRoutingTable(_routingTable);

			if ( commandline().hasOption("no-events") ) {
				dispatcher->setSetIgnoreObject(BaseObjectDispatcher::Event);
			}

			if ( commandline().hasOption("create-notifier") ) {
				dispatcher->setCreateNotifierMsg(true);
				SEISCOMP_INFO("XML output enabled, not messages will be sent");
			}

			unsigned int totalCount = ObjectCounter(doc.get()).count();

			SEISCOMP_INFO("Time needed to parse XML: %s", Core::Time(timer.elapsed()).toString("%T.%f"));
			SEISCOMP_INFO("Document object type: %s", doc->className());
			SEISCOMP_INFO("Total number of objects: %d", totalCount);

			if ( connection() ) {
				SEISCOMP_INFO("Dispatching %s to %s",
				              doc->className(), connection()->source().c_str());
			}
			timer.restart();

			(*dispatcher)(doc.get());

			if ( commandline().hasOption("create-notifier") ) {
				NotifierMessagePtr msg = dispatcher->getNotifierMsg();
				IO::XMLArchive ar;
				ar.create("-");
				ar.setFormattedOutput(true);
				ar << msg;
				ar.close();
			}

			SEISCOMP_INFO("While dispatching %d/%d objects %d errors occured", 
			              dispatcher->count(), totalCount, dispatcher->errors());
			SEISCOMP_INFO("Time needed to dispatch %d objects: %s",
			              dispatcher->count(), Core::Time(timer.elapsed()).toString("%T.%f"));

			delete dispatcher;

			return true;
		}

	// ----------------------------------------------------------------------
	// Private data member
	// ----------------------------------------------------------------------
	private:
		string               _inputFile;
		string               _notifierOperation;
		string               _routingTableStr;
		RoutingTable         _routingTable;
		Operation            _operation;

};
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int main(int argc, char** argv) {
	DispatchTool app(argc, argv);
	return app.exec();
}
