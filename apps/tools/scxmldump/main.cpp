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


#define SEISCOMP_COMPONENT SCXMLDump

#include <seiscomp/logging/log.h>
#include <seiscomp/client/application.h>
#include <seiscomp/io/archive/xmlarchive.h>
#include <seiscomp/datamodel/inventory.h>
#include <seiscomp/datamodel/config.h>
#include <seiscomp/datamodel/journaling.h>
#include <seiscomp/datamodel/journalentry.h>
#include <seiscomp/datamodel/routing.h>
#include <seiscomp/datamodel/dataavailability.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/event.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/magnitude.h>
#include <seiscomp/datamodel/stationmagnitude.h>
#include <seiscomp/datamodel/amplitude.h>
#include <seiscomp/datamodel/focalmechanism.h>
#include <seiscomp/datamodel/momenttensor.h>

#include <set>


using namespace std;
using namespace Seiscomp;
using namespace Seiscomp::Core;
using namespace Seiscomp::IO;
using namespace Seiscomp::DataModel;


static void removeAllArrivals(Seiscomp::DataModel::Origin *origin) {

	while ( origin->arrivalCount() > 0 ) {
		origin->removeArrival(0);
	}
}


static void removeAllNetworkMagnitudes(Seiscomp::DataModel::Origin *origin) {

	while ( origin->magnitudeCount() > 0 ) {
		origin->removeMagnitude(0);
	}
}


static void removeAllStationMagnitudes(Seiscomp::DataModel::Origin *origin) {

	while ( origin->stationMagnitudeCount() > 0 ) {
		origin->removeStationMagnitude(0);
	}

	for ( size_t i = 0; i < origin->magnitudeCount(); ++i ) {
		Magnitude* netMag = origin->magnitude(i);
		while ( netMag->stationMagnitudeContributionCount() > 0 ) {
			netMag->removeStationMagnitudeContribution(0);
		}
	}
}


static void removeAllStationContributions(Seiscomp::DataModel::MomentTensor *mt) {

	while ( mt->momentTensorStationContributionCount() > 0 ) {
		mt->removeMomentTensorStationContribution(0);
	}
}


bool readIDParam(vector<string> &store, string &previousStdinParam,
                 const string &value, const string &param) {
	if ( value.empty() ) {
		return true;
	}

	// Read from stdin
	if ( value == "-" ) {
		if ( !previousStdinParam.empty() ) {
			SEISCOMP_ERROR("Conflicting parameter value in '%s' and '%s'. "
			               "Only one ID paramater may be specified via stdin.",
			               previousStdinParam.c_str(), param.c_str());
			return false;
		}
		previousStdinParam = param;

		string line;
		while (std::getline(cin, line) ) {
			store.push_back(line);
		}

		// Assert at least one item
		if ( store.empty() ) {
			SEISCOMP_ERROR("No IDs found for parameter '%s' on stdin",
			               param.c_str());
			return false;
		}
	}
	// Split comma-separated list
	else {
		Core::split(store, value, ",");
	}

	// Sort and unique
	sort(store.begin(), store.end() );
	store.erase(unique(store.begin(), store.end()), store.end());
	return true;
}


void readTree(DatabaseArchive *ar, PublicObject *obj) {
	auto meta = obj->meta();
	if ( !meta ) {
		return;
	}

	auto properyCount = meta->propertyCount();
	for ( size_t i = 0; i < properyCount; ++i ) {
		auto prop = meta->property(i);
		if ( !prop->isArray() || !prop->isClass() ) {
			continue;
		}

		auto factory = ClassFactory::FindByClassName(prop->type());
		if ( !factory ) {
			continue;
		}

		auto it = ar->getObjects(obj->publicID(), *factory->typeInfo());
		for ( ; it.get(); ++it ) {
			prop->arrayAddObject(obj, it.get());
		}
		it.close();

		if ( factory->typeInfo()->isTypeOf(PublicObject::TypeInfo()) ) {
			// Load childs
			auto cnt = prop->arrayElementCount(obj);
			for ( size_t c = 0; c < cnt; ++c ) {
				readTree(ar, static_cast<PublicObject*>(prop->arrayObject(obj, static_cast<int>(c))));
			}
		}
	}
}


bool classIsRootType(const RTTI *typeInfo) {
	auto classes = ClassFactory::RegisteredClasses();
	for ( auto &[rtti, name] : classes ) {
		auto meta = MetaObject::Find(name);
		if ( !meta ) {
			continue;
		}

		auto properyCount = meta->propertyCount();
		for ( size_t i = 0; i < properyCount; ++i ) {
			auto prop = meta->property(i);
			if ( !prop->isArray() || !prop->isClass() ) {
				continue;
			}

			auto factory = ClassFactory::FindByClassName(prop->type());
			if ( !factory ) {
				continue;
			}

			if ( factory->typeInfo() == typeInfo ) {
				return false;
			}
		}
	}

	return true;
}



class XMLDump : public Seiscomp::Client::Application {
	public:
		XMLDump(int argc, char** argv) : Application(argc, argv) {
			setPrimaryMessagingGroup(Seiscomp::Client::Protocol::LISTENER_GROUP);
			addMessagingSubscription("EVENT");
			setMessagingEnabled(true);
			setDatabaseEnabled(true, false);
			setAutoApplyNotifierEnabled(false);
			setInterpretNotifierEnabled(true);

			addMessagingSubscription("EVENT");
		}

		~XMLDump() override {
			flushArchive();
		}

	protected:
		void createCommandLineDescription() override {
			commandline().addGroup("Dump");
			commandline().addOption("Dump", "config,C", "Dump the config (bindings).");
			commandline().addOption("Dump", "inventory,I", "Dump the inventory.");
			commandline().addOption("Dump", "without-station-groups",
			                        "Remove station groups from inventory.");
			commandline().addOption("Dump", "stations",
			                        "If inventory is dumped, filter the "
			                        "stations to dump. Wildcards are supported."
			                        "Format of each item: net[.{sta|*}]. Use '-' "
			                        "to read the IDs as individual lines from stdin.",
			                        &_stationIDParam);
			commandline().addOption("Dump", "journal,J", "Dump the journal.");
			commandline().addOption("Dump", "routing,R", "Dump routing.");
			commandline().addOption("Dump", "availability,Y",
			                        "Dump data availability information.");
			commandline().addOption("Dump", "with-segments",
			                        "Dump individual data availability segments.");
			commandline().addOption("Dump", "listen",
			                        "Listen to the message server for incoming events.");
			commandline().addOption("Dump", "pick",
			                        "ID(s) of pick(s) to dump. Use '-' to read "
			                        "the IDs as individual lines from stdin.",
			                        &_pickIDParam, false);
			commandline().addOption("Dump", "origin,O",
			                        "ID(s) of origin(s) to dump. Use '-' to read "
			                        "the IDs as individual lines from stdin.",
			                        &_originIDParam, false);
			commandline().addOption("Dump", "event,E",
			                        "ID(s) of event(s) to export. Use '-' to read "
			                        "the IDs as individual lines from stdin.",
			                        &_eventIDParam, false);
			commandline().addOption("Dump", "with-picks,P", "Dump associated picks.");
			commandline().addOption("Dump", "with-amplitudes,A",
			                        "Add amplitudes associated to dumped objects.");
			commandline().addOption("Dump", "with-magnitudes,M",
			                        "Add station magnitudes associated to dumped objects.");
			commandline().addOption("Dump", "with-focal-mechanisms,F",
			                        "Add focal mechanisms associated to dumped objects.");
			commandline().addOption("Dump", "ignore-arrivals,a",
			                        "Do not dump arrivals of origins.");
			commandline().addOption("Dump", "ignore-magnitudes",
			                        "Do not dump magnitudes of origins.");
			commandline().addOption("Dump", "preferred-only,p",
			                        "When dumping events, only the preferred "
			                        "origin and the preferred magnitude will be dumped.");
			commandline().addOption("Dump", "all-magnitudes,m",
			                        "If only the preferred origin is dumped, "
			                        "all magnitudes for this origin will be dumped.");
			commandline().addOption("Dump", "public-id",
			                        "Dump object(s) by its(their) publicID.", &_publicIDParam);
			commandline().addOption("Dump", "with-childs", "Whether to export child objects for "
			                        "PublicObjects or not. Valid in combination with --public-id.");
			commandline().addGroup("Output");
			commandline().addOption("Output", "formatted,f", "Use formatted XML output.");
			commandline().addOption("Output", "output,o",
			                        "Name of output file. If not given or '-', output "
			                        "is sent to stdout.",
			                        &_outputFile, false);
			commandline().addOption("Output", "prepend-datasize",
			                        "Prepend a line with the length of the XML string.");
		}

		void printUsage() const override {
			std::cout << "Usage:" << std::endl << "  scxmldump [options]"
			          << std::endl << std::endl
			          << "Dump objects from a database or messaging to XML."
			          << std::endl;

			Client::Application::printUsage();

			std::cout << "Examples:" << std::endl;
			std::cout << "Dump event parameters for one event in database into a XML file"
			          << std::endl
			          << "  scxmldump -d mysql://sysop:sysop@localhost/seiscomp -E gempa2022abcd -PAMFf -o gempa2022abcd.xml"
			          << std::endl << std::endl;
			std::cout << "Dump all event parameters received from local messaging into XML"
			          << std::endl
			          << "  scxmldump -H localhost/production --listen"
			          << std::endl << std::endl;
		}

		bool validateParameters() override {
			if ( !Seiscomp::Client::Application::validateParameters() ) {
				return false;
			}

			if ( _outputFile.empty() ) {
				_outputFile = "-";
			}

			if ( !commandline().hasOption("listen") ) {
				if ( !commandline().hasOption("public-id")
				  && !commandline().hasOption("event")
				  && !commandline().hasOption("origin")
				  && !commandline().hasOption("pick")
				  && !commandline().hasOption("inventory")
				  && !commandline().hasOption("config")
				  && !commandline().hasOption("journal")
				  && !commandline().hasOption("routing")
				  && !commandline().hasOption("availability") ) {
					cerr << "Require inventory, config, journal, routing or "
					        "availability flag, or IDs of origin(s), event(s) or pick(s)" << endl;
					return false;
				}

				setMessagingEnabled(false);
				setLoggingToStdErr(true);
			}

			_withPicks             = commandline().hasOption("with-picks");
			_withAmplitudes        = commandline().hasOption("with-amplitudes");
			_preferredOnly         = commandline().hasOption("preferred-only");
			_withStationMagnitudes = commandline().hasOption("with-magnitudes");
			_allMagnitudes         = commandline().hasOption("all-magnitudes");
			_ignoreArrivals        = commandline().hasOption("ignore-arrivals");
			_withFocalMechanisms   = commandline().hasOption("with-focal-mechanisms");
			_withChilds            = commandline().hasOption("with-childs");

			string previousStdinParam;
			if ( !readIDParam(_publicIDs, previousStdinParam, _publicIDParam, "public-id") ||
			     !readIDParam(_stationIDs, previousStdinParam, _stationIDParam, "stations") ||
			     !readIDParam(_eventIDs, previousStdinParam, _eventIDParam, "event") ||
			     !readIDParam(_originIDs, previousStdinParam, _originIDParam, "origin") ||
			     !readIDParam(_pickIDs, previousStdinParam, _pickIDParam, "pick") ) {
				return false;
			}

			return true;
		}

		bool write(PublicObject *po) {
			if ( !_archive ) {
				_archive = new XMLArchive();
				_archive->setFormattedOutput(commandline().hasOption("formatted"));

				if ( commandline().hasOption("prepend-datasize") ) {
					if ( !_archive->create(&_archiveBuf) ) {
						SEISCOMP_ERROR("Could not create output file '%s'", _outputFile.c_str());
						delete _archive;
						_archive = nullptr;
						return false;
					}
				}
				else {
					if ( !_archive->create(_outputFile.c_str()) ) {
						SEISCOMP_ERROR("Could not create output file '%s'", _outputFile.c_str());
						delete _archive;
						_archive = nullptr;
						return false;
					}
				}
			}


			if ( po ) {
				*_archive << po;
			}

			return true;
		}

		bool flushArchive() {
			if ( !_archive ) {
				return false;
			}

			_archive->close();
			delete _archive;
			_archive = nullptr;

			if ( !commandline().hasOption("prepend-datasize") ) {
				return true;
			}

			string content = _archiveBuf.str();
			_archiveBuf.str("");

			if ( _outputFile == "-" ) {
				cout << content.size() << endl << content << flush;
				SEISCOMP_INFO("Flushing %zu bytes", content.size());
				return true;
			}

			ofstream file(_outputFile.c_str(), ios::out | ios::trunc);

			if ( !file.is_open() ) {
				SEISCOMP_ERROR("Could not create file: %s", _outputFile.c_str());
				return false;
			}

			file << content.size() << endl << content;
			file.close();

			return true;
		}


		bool dumpInventory() {
			SEISCOMP_INFO("Dumping Inventory");
			typedef string NetworkID;
			typedef pair<NetworkID,string> StationID;
			typedef set<NetworkID> NetworkFilter;
			typedef set<StationID> StationFilter;
			NetworkFilter networkFilter;
			StationFilter stationFilter;
			set<string> usedSensors;
			set<string> usedDataloggers;
			set<string> usedDevices;
			set<string> usedResponses;

			InventoryPtr inv = query()->loadInventory();
			if ( !inv ) {
				SEISCOMP_ERROR("Inventory has not been found");
				return false;
			}

			if ( !_stationIDs.empty() ) {
				for ( const auto &stationID : _stationIDs ) {
					size_t pos = stationID.find('.');

					if ( pos == string::npos ) {
						stationFilter.insert(StationID(stationID, "*"));
						networkFilter.insert(stationID);
					}
					else {
						stationFilter.insert(
						            StationID(
						                stationID.substr(0,pos),
						                stationID.substr(pos+1)
						                )
						            );
						networkFilter.insert(stationID.substr(0,pos));
					}
				}

				// Remove unwanted networks
				for ( size_t n = 0; n < inv->networkCount(); ) {
					Network *net = inv->network(n);

					bool passed;
					NetworkFilter::iterator nit;
					for ( nit = networkFilter.begin(), passed = false;
					      nit != networkFilter.end(); ++ nit ) {
						if ( Core::wildcmp(*nit, net->code()) ) {
							passed = true;
							break;
						}
					}

					if ( !passed ) {
						inv->removeNetwork(n);
						continue;
					}

					++n;

					// Remove unwanted stations
					for ( size_t s = 0; s < net->stationCount(); ) {
						Station *sta = net->station(s);

						StationFilter::iterator sit;
						for ( sit = stationFilter.begin(), passed = false;
						      sit != stationFilter.end(); ++ sit ) {
							if ( Core::wildcmp(sit->first, net->code()) &&
							     Core::wildcmp(sit->second, sta->code()) ) {
								passed = true;
								break;
							}
						}

						// Should this station be filtered
						if ( !passed ) {
							net->removeStation(s);
							continue;
						}

						++s;
					}
				}

				// Collect used sensors and dataloggers
				for ( size_t n = 0; n < inv->networkCount(); ++n ) {
					Network *net = inv->network(n);

					for ( size_t s = 0; s < net->stationCount(); ++s ) {
						Station *sta = net->station(s);

						// Collect all used sensors and dataloggers
						for ( size_t l = 0; l < sta->sensorLocationCount(); ++l ) {
							SensorLocation *loc = sta->sensorLocation(l);
							for ( size_t c = 0; c < loc->streamCount(); ++c ) {
								Stream *cha = loc->stream(c);
								usedSensors.insert(cha->sensor());
								usedDataloggers.insert(cha->datalogger());
							}

							for ( size_t a = 0; a < loc->auxStreamCount(); ++a ) {
								AuxStream *aux = loc->auxStream(a);
								usedDevices.insert(aux->device());
							}
						}
					}
				}

				// Removed unused dataloggers
				for ( size_t i = 0; i < inv->dataloggerCount(); ) {
					Datalogger *dl = inv->datalogger(i);
					if ( usedDataloggers.find(dl->publicID()) == usedDataloggers.end() ) {
						inv->removeDatalogger(i);
						continue;
					}

					++i;

					for ( size_t j = 0; j < dl->decimationCount(); ++j ) {
						Decimation *deci = dl->decimation(j);
						try {
							const string &c = deci->analogueFilterChain().content();
							if ( !c.empty() ) {
								vector<string> ids;
								Core::fromString(ids, c);
								usedResponses.insert(ids.begin(), ids.end());
							}
						}
						catch ( ... ) {}

						try {
							const string &c = deci->digitalFilterChain().content();
							if ( !c.empty() ) {
								vector<string> ids;
								Core::fromString(ids, c);
								usedResponses.insert(ids.begin(), ids.end());
							}
						}
						catch ( ... ) {}
					}
				}

				for ( size_t i = 0; i < inv->sensorCount(); ) {
					Sensor *sensor = inv->sensor(i);
					if ( usedSensors.find(sensor->publicID()) == usedSensors.end() ) {
						inv->removeSensor(i);
						continue;
					}

					++i;

					usedResponses.insert(sensor->response());
				}

				for ( size_t i = 0; i < inv->auxDeviceCount(); ) {
					auto *device = inv->auxDevice(i);
					if ( usedDevices.find(device->publicID()) == usedDevices.end() ) {
						inv->removeAuxDevice(i);
						continue;
					}

					++i;
				}

				// Go through all available responses and remove unused ones
				for ( size_t i = 0; i < inv->responseFIRCount(); ) {
					auto *resp = inv->responseFIR(i);
					// Response not used -> remove it
					if ( usedResponses.find(resp->publicID()) == usedResponses.end() ) {
						inv->removeResponseFIR(i);
						continue;
					}

					++i;
				}

				for ( size_t i = 0; i < inv->responsePAZCount(); ) {
					auto *resp = inv->responsePAZ(i);
					// Response not used -> remove it
					if ( usedResponses.find(resp->publicID()) == usedResponses.end() ) {
						inv->removeResponsePAZ(i);
						continue;
					}

					++i;
				}

				for ( size_t i = 0; i < inv->responsePolynomialCount(); ) {
					auto *resp = inv->responsePolynomial(i);
					// Response not used -> remove it
					if ( usedResponses.find(resp->publicID()) == usedResponses.end() ) {
						inv->removeResponsePolynomial(i);
						continue;
					}

					++i;
				}

				for ( size_t i = 0; i < inv->responseFAPCount(); ) {
					auto *resp = inv->responseFAP(i);
					// Response not used -> remove it
					if ( usedResponses.find(resp->publicID()) == usedResponses.end() ) {
						inv->removeResponseFAP(i);
						continue;
					}

					++i;
				}

				for ( size_t i = 0; i < inv->responseIIRCount(); ) {
					auto *resp = inv->responseIIR(i);
					// Response not used -> remove it
					if ( usedResponses.find(resp->publicID()) == usedResponses.end() ) {
						inv->removeResponseIIR(i);
						continue;
					}

					++i;
				}
			}

			if ( commandline().hasOption("without-station-groups") ) {
				while ( inv->stationGroupCount() > 0 ) {
					inv->removeStationGroup(0);
				}
			}

			return write(inv.get());
		}


		bool dumpConfig() {
			SEISCOMP_INFO("Dumping Config");
			ConfigPtr cfg = query()->loadConfig();
			if ( !cfg ) {
				SEISCOMP_ERROR("Config has not been found");
				return false;
			}

			return write(cfg.get());
		}


		bool dumpRouting() {
			SEISCOMP_INFO("Dumping Routing");
			RoutingPtr routing = query()->loadRouting();
			if ( !routing ) {
				SEISCOMP_ERROR("Routing has not been found");
				return false;
			}

			return write(routing.get());
		}


		bool dumpDataAvailability() {
			SEISCOMP_INFO("Dumping DataAvailability");
			DataAvailabilityPtr avail = new DataAvailability();
			query()->loadDataExtents(avail.get());
			if ( avail->dataExtentCount() == 0 ) {
				SEISCOMP_ERROR("No data availability extents found");
				return false;
			}

			if ( commandline().hasOption("with-segments") ) {
				for ( size_t i = 0; i < avail->dataExtentCount(); ++i ) {
					query()->load(avail->dataExtent(i));
				}
			}
			else {
				for ( size_t i = 0; i < avail->dataExtentCount(); ++i ) {
					query()->loadDataAttributeExtents(avail->dataExtent(i));
				}
			}

			return write(avail.get());
		}

		bool dumpPublicIDs() {
			if ( _publicIDs.empty() ) {
				return true;
			}

			auto classes = ClassFactory::RegisteredClasses();
			SEISCOMP_DEBUG("Dumping %d publicID%s",
			               _publicIDs.size(), _publicIDs.size() > 1 ? "s":"");
			for ( auto &publicID : _publicIDs ) {
				bool found = false;
				for ( auto &[rtti, name] : classes ) {
					if ( (rtti == &PublicObject::TypeInfo())
					  || !rtti->isTypeOf(PublicObject::TypeInfo()) ) {
						continue;
					}

					if ( classIsRootType(rtti) ) {
						// Root types do not have a database table representation
						continue;
					}

					PublicObjectPtr po = query()->getObject(*rtti, publicID);
					if ( po ) {
						if ( _withChilds ) {
							readTree(query(), po.get());
						}
						write(po.get());
						found = true;
						break;
					}
				}

				if ( found ) {
					SEISCOMP_DEBUG("+ %s", publicID);
				}
				else {
					SEISCOMP_DEBUG("- %s [not found]", publicID);
				}
			}

			return true;
		}

		bool dumpEPAndJournal() {
			// collection of publicIDs for which we might want the
			// journal to be dumped (if journal output is requested).
			EventParametersPtr ep;
			if ( !_eventIDs.empty() ) {
				if ( !ep ) {
					SEISCOMP_INFO("Dumping EventParameters");
					ep = new EventParameters;
				}

				for ( const auto &publicID : _eventIDs ) {
					EventPtr event = Event::Cast(PublicObjectPtr(
						query()->getObject(Event::TypeInfo(), publicID)));
					if ( event ) {
						addEvent(ep.get(), event.get());
					}
					else {
						SEISCOMP_ERROR("Event with ID '%s' has not been found", publicID.c_str());
					}
				}
			}

			if ( ! _originIDs.empty() ) {
				if ( ! ep ) {
					SEISCOMP_INFO("Dumping EventParameters");
					ep = new EventParameters;
				}

				for ( const auto &publicID : _originIDs ) {
					OriginPtr origin = Origin::Cast(PublicObjectPtr(
						query()->getObject(Origin::TypeInfo(), publicID)));
					if ( origin ) {
						addOrigin(ep.get(), origin.get());
					}
					else {
						SEISCOMP_ERROR("Origin with ID '%s' has not been found", publicID.c_str());
					}
				}
			}

			if ( !_pickIDs.empty() ) {
				if ( !ep ) {
					SEISCOMP_INFO("Dumping EventParameters");
					ep = new EventParameters;
				}

				for ( const auto &publicID : _pickIDs ) {
					if ( _pickIDSet.find(publicID) != _pickIDSet.end() ) {
						SEISCOMP_INFO("Pick '%s' already exported", publicID.c_str());
					}

					PickPtr pick = Pick::Cast(PublicObjectPtr(
					    query()->getObject(Pick::TypeInfo(), publicID)));
					if ( pick ) {
						SEISCOMP_INFO("Dumping Pick '%s'", pick->publicID().c_str());
						ep->add(pick.get());
					}
					else {
						SEISCOMP_ERROR("Pick with ID '%s' has not been found", publicID.c_str());
					}
				}
			}

			if ( ep && !write(ep.get()) ) {
				return false;
			}

			JournalingPtr jnl;
			if ( commandline().hasOption("journal") ) {

				vector<string> journalingPublicIDs;
				journalingPublicIDs.insert(journalingPublicIDs.end(),
				                           _eventIDs.cbegin(), _eventIDs.cend());
				journalingPublicIDs.insert(journalingPublicIDs.end(),
				                           _originIDs.cbegin(), _originIDs.cend());
				journalingPublicIDs.insert(journalingPublicIDs.end(),
				                           _pickIDs.cbegin(), _pickIDs.cend());

				if ( journalingPublicIDs.empty() ) {
					// retrieve entire journal
					jnl = query()->loadJournaling();
					if ( !jnl ) {
						SEISCOMP_ERROR("Journal has not been found");
						return false;
					}
				}
				else {
					// create a journal specific to the publicIDs
					jnl = new Journaling();
					for ( const auto &publicID : journalingPublicIDs ) {
						DatabaseIterator it = query()->getJournal(publicID);
						while ( it.get() ) {
							jnl->add(JournalEntry::Cast(*it));
							++it;
						}
					}
				}

				if ( !write(jnl.get()) ) {
					return false;
				}
			}

			return true;
		}


		bool run() override {
			if ( !query() ) {
				SEISCOMP_ERROR("No database connection");
				return false;
			}

			if ( ( commandline().hasOption("inventory") && !dumpInventory() ) ||
			     ( commandline().hasOption("config") && !dumpConfig() ) ||
			     ( commandline().hasOption("routing") && !dumpRouting() ) ||
			     ( commandline().hasOption("availability") && !dumpDataAvailability() ) ||
			     !dumpEPAndJournal() || !dumpPublicIDs() ) {
				return false;
			}

			flushArchive();

			return !commandline().hasOption("listen") || Application::run();
		}


		void addObject(const string &parentID,
		               Seiscomp::DataModel::Object *object) override {
			updateObject(parentID, object);
		}


		void updateObject(const string &parentID,
		                  Seiscomp::DataModel::Object *object) override {
			auto *e = Event::Cast(object);
			if ( !e ) {
				return;
			}

			EventParametersPtr ep = new EventParameters;
			addEvent(ep.get(), e);
			write(ep.get()) && flushArchive();
		}


		void addOrigin(EventParameters *ep, Origin *origin) {
			SEISCOMP_INFO("Dumping Origin '%s'", origin->publicID().c_str());
			ep->add(origin);

			query()->load(origin);

			if ( commandline().hasOption("ignore-magnitudes") ) {
				removeAllNetworkMagnitudes(origin);
			}

			if ( !_withStationMagnitudes ) {
				removeAllStationMagnitudes(origin);
			}

			if ( _ignoreArrivals ) {
				removeAllArrivals(origin);
			}

			if ( _withPicks ) {
				for ( size_t a = 0; a < origin->arrivalCount(); ++a ) {
					const string &pickID = origin->arrival(a)->pickID();
					if ( _pickIDSet.find(pickID) != _pickIDSet.end() ) {
						continue;
					}

					PickPtr pick = Pick::Cast(PublicObjectPtr(
						query()->getObject(Pick::TypeInfo(), pickID)));
					if ( !pick ) {
						SEISCOMP_WARNING("Pick with id '%s' not found", pickID.c_str());
						continue;
					}

					query()->load(pick.get());

					if ( !pick->eventParameters() ) {
						ep->add(pick.get());
					}
					_pickIDSet.insert(pickID);
				}
			}

			if ( _withAmplitudes && _withStationMagnitudes ) {
				// Extract amplitudes corresponding to station magnitudes
				for ( size_t m = 0; m < origin->magnitudeCount(); ++m ) {
					Magnitude* netMag = origin->magnitude(m);
					for ( size_t s = 0; s < netMag->stationMagnitudeContributionCount(); ++s ) {
						const string &stationMagnitudeID =
							netMag->stationMagnitudeContribution(s)->stationMagnitudeID();
						StationMagnitude* staMag = StationMagnitude::Find(stationMagnitudeID);
						if ( !staMag ) {
							SEISCOMP_WARNING("StationMagnitude with id '%s' not found",
									 stationMagnitudeID.c_str());
							continue;
						}

						const string &amplitudeID = staMag->amplitudeID();
						if ( amplitudeID.empty() ) {
							SEISCOMP_DEBUG("StationMagnitude with id '%s' has no amplitude ID",
							               staMag->publicID().c_str());
							continue;
						}

						if (_amplitudeIDSet.find(amplitudeID) != _amplitudeIDSet.end()) {
							continue;
						}

						AmplitudePtr amplitude = Amplitude::Cast(PublicObjectPtr(
							query()->getObject(Amplitude::TypeInfo(), amplitudeID)));
						if ( !amplitude ) {
							SEISCOMP_WARNING("Amplitude with id '%s' not found",
									 amplitudeID.c_str());
							continue;
						}

						if ( !amplitude->eventParameters() ) {
							ep->add(amplitude.get());
						}

						_amplitudeIDSet.insert(amplitudeID);
					}
				}
			}

			if ( _withAmplitudes && !_withStationMagnitudes ) {
				// Extract all amplitudes for all picks
				for ( size_t p = 0; p < ep->pickCount(); ++p ) {
					Pick *pick = ep->pick(p);
					DatabaseIterator it = query()->getAmplitudesForPick(pick->publicID());
					for ( ; *it; ++it ) {
						auto *amplitude = Amplitude::Cast(*it);
						if (_amplitudeIDSet.find(amplitude->publicID()) != _amplitudeIDSet.end()) {
							continue;
						}

						if ( amplitude ) {
							ep->add(amplitude);
						}

						_amplitudeIDSet.insert(amplitude->publicID());
					}
				}
			}
		}


		void addEvent(EventParameters *ep, Event *event) {
			SEISCOMP_INFO("Dumping Event '%s'", event->publicID().c_str());
			ep->add(event);

			if ( !_preferredOnly ) {
				query()->load(event);
			}
			else {
				query()->loadComments(event);
				query()->loadEventDescriptions(event);
				event->add(
					OriginReferencePtr(
						new OriginReference(event->preferredOriginID())
					).get()
				);

				if ( _withFocalMechanisms
				  && !event->preferredFocalMechanismID().empty() ) {
					event->add(
						FocalMechanismReferencePtr(
							new FocalMechanismReference(
								event->preferredFocalMechanismID()
							)
						).get()
					);
				}
			}

			bool foundPreferredMag = false;

			// No need to search for it
			if ( event->preferredMagnitudeID().empty() ) {
				foundPreferredMag = true;
			}


			// loop over origins referenced by event
			for ( size_t i = 0; i < event->originReferenceCount(); ++i ) {
				const string originID = event->originReference(i)->originID();
				OriginPtr origin = Origin::Cast(PublicObjectPtr(
					query()->getObject(Origin::TypeInfo(), originID)));
				if ( !origin ) {
					SEISCOMP_WARNING("Origin with id '%s' not found", originID.c_str());
					continue;
				}

				query()->load(origin.get());

				if ( _preferredOnly && !_allMagnitudes ) {
					MagnitudePtr netMag;
					while ( origin->magnitudeCount() > 0 ) {
						if ( origin->magnitude(0)->publicID() == event->preferredMagnitudeID() ) {
							netMag = origin->magnitude(0);
						}
						origin->removeMagnitude(0);
					}

					if ( netMag ) {
						foundPreferredMag = true;
						origin->add(netMag.get());

						// remove station magnitudes of types which are not preferred
						for ( size_t i = 0; i < origin->stationMagnitudeCount(); ) {
							auto *staMag = origin->stationMagnitude(i);
							if ( staMag->type() != netMag->type() ) {
								origin->removeStationMagnitude(i);
							}
							else {
								++i;
							}
						}
					}
				}
				else if ( !foundPreferredMag ) {
					for ( size_t m = 0; m < origin->magnitudeCount(); ++m ) {
						if ( origin->magnitude(m)->publicID() == event->preferredMagnitudeID() ) {
							foundPreferredMag = true;
							break;
						}
					}
				}

				if ( !_withStationMagnitudes ) {
					removeAllStationMagnitudes(origin.get());
				}

				if ( _ignoreArrivals ) {
					removeAllArrivals(origin.get());
				}

				ep->add(origin.get());

				if ( _withPicks ) {
					for ( size_t a = 0; a < origin->arrivalCount(); ++a ) {
						const string &pickID =
							origin->arrival(a)->pickID();
						if ( _pickIDSet.find(pickID) != _pickIDSet.end() ) {
							continue;
						}

						PickPtr pick = Pick::Cast(PublicObjectPtr(
							query()->getObject(Pick::TypeInfo(), pickID)));
						if ( !pick ) {
							SEISCOMP_WARNING("Pick with id '%s' not found",
									 pickID.c_str());
							continue;
						}

						query()->load(pick.get());

						if ( !pick->eventParameters() ) {
							ep->add(pick.get());
						}
						_pickIDSet.insert(pickID);
					}
				}

				if ( _withAmplitudes && _withStationMagnitudes ) {
					for ( size_t m = 0; m < origin->magnitudeCount(); ++m ) {
						Magnitude* netMag = origin->magnitude(m);
						for ( size_t s = 0; s < netMag->stationMagnitudeContributionCount(); ++s ) {
							const string &staMagID
								= netMag->stationMagnitudeContribution(s)->stationMagnitudeID();
							StationMagnitude* staMag = StationMagnitude::Find(staMagID);
							if ( !staMag ) {
								SEISCOMP_WARNING("StationMagnitude with id '%s' not found",
								                 staMagID.c_str());
								continue;
							}

							const string &amplitudeID = staMag->amplitudeID();
							if ( amplitudeID.empty() ) {
								SEISCOMP_DEBUG("StationMagnitude with id '%s' has no amplitude ID",
								               staMag->publicID().c_str());
								continue;
							}
							if ( _amplitudeIDSet.find(amplitudeID) != _amplitudeIDSet.end() ) {
								continue;
							}

							AmplitudePtr amplitude = Amplitude::Cast(PublicObjectPtr(
								query()->getObject(Amplitude::TypeInfo(), amplitudeID)));

							if ( !amplitude ) {
								SEISCOMP_WARNING("Amplitude with id '%s' not found",
										 amplitudeID.c_str());
								continue;
							}

							if ( !amplitude->eventParameters() ) {
								ep->add(amplitude.get());
							}

							_amplitudeIDSet.insert(amplitudeID);
						}
					}
				}
			}
			// end of loop over origins referenced by event


			if ( _withAmplitudes && !_withStationMagnitudes ) {
				// Extract all amplitudes for all picks
				for ( size_t p = 0; p < ep->pickCount(); ++p ) {
					Pick *pick = ep->pick(p);
					DatabaseIterator it = query()->getAmplitudesForPick(pick->publicID());
					for ( ; *it; ++it ) {
						auto *amplitude = Amplitude::Cast(*it);
						if (_amplitudeIDSet.find(amplitude->publicID()) != _amplitudeIDSet.end()) {
							continue;
						}

						if ( amplitude ) {
							ep->add(amplitude);
						}

						_amplitudeIDSet.insert(amplitude->publicID());
					}
				}
			}

			if ( !_withFocalMechanisms ) {
				while ( event->focalMechanismReferenceCount() > 0 ) {
					event->removeFocalMechanismReference(0);
				}
			}

			for ( size_t i = 0; i < event->focalMechanismReferenceCount(); ++i ) {
				const string &fmID =
					event->focalMechanismReference(i)->focalMechanismID();
				FocalMechanismPtr fm = FocalMechanism::Cast(PublicObjectPtr(
					query()->getObject(FocalMechanism::TypeInfo(), fmID)));
				if ( !fm ) {
					SEISCOMP_WARNING("FocalMechanism with id '%s' not found", fmID.c_str());
					continue;
				}

				query()->load(fm.get());
				for ( size_t m = 0; m < fm->momentTensorCount(); ++m ) {
					auto *mt = fm->momentTensor(m);
					if ( _ignoreArrivals ) {// TODO: review!
						removeAllStationContributions(mt);
					}
				}

				if ( !fm->triggeringOriginID().empty()
				  && fm->triggeringOriginID() != event->preferredOriginID() ) {

					OriginPtr triggeringOrigin = ep->findOrigin(fm->triggeringOriginID());
					if ( !triggeringOrigin ) {
						triggeringOrigin = Origin::Cast(PublicObjectPtr(
							query()->getObject(Origin::TypeInfo(), fm->triggeringOriginID())));
						if ( !triggeringOrigin ) {
							SEISCOMP_WARNING("Triggering origin with id '%s' not found",
									 fm->triggeringOriginID().c_str());
						}
						else {
							query()->load(triggeringOrigin.get());

							if ( !_withStationMagnitudes ) {
								removeAllStationMagnitudes(triggeringOrigin.get());
							}

							if ( _ignoreArrivals ) {
								removeAllArrivals(triggeringOrigin.get());
							}

							if ( _preferredOnly && !_allMagnitudes ) {
								MagnitudePtr netMag;
								while ( triggeringOrigin->magnitudeCount() > 0 ) {
									if ( triggeringOrigin->magnitude(0)->publicID() == event->preferredMagnitudeID() ) {
										netMag = triggeringOrigin->magnitude(0);
									}

									triggeringOrigin->removeMagnitude(0);
								}

								if ( netMag ) {
									triggeringOrigin->add(netMag.get());
								}
							}

							ep->add(triggeringOrigin.get());
						}
					}
				}

				ep->add(fm.get());

				for ( size_t m = 0; m < fm->momentTensorCount(); ++m ) {
					MomentTensor *mt = fm->momentTensor(m);
					if ( mt->derivedOriginID().empty() ) {
						continue;
					}

					OriginPtr derivedOrigin = ep->findOrigin(mt->derivedOriginID());
					if ( derivedOrigin ) {
						continue;
					}

					derivedOrigin = Origin::Cast(PublicObjectPtr(
						query()->getObject(Origin::TypeInfo(), mt->derivedOriginID())));
					if ( !derivedOrigin ) {
						SEISCOMP_WARNING("Derived MT origin with id '%s' not found",
								 mt->derivedOriginID().c_str());
						continue;
					}

					query()->load(derivedOrigin.get());
					ep->add(derivedOrigin.get());

					if ( !foundPreferredMag ) {
						for ( size_t m = 0; m < derivedOrigin->magnitudeCount(); ++m ) {
							if ( derivedOrigin->magnitude(m)->publicID() == event->preferredMagnitudeID() ) {
								foundPreferredMag = true;
								break;
							}
						}
					}
				}
			}

			// Find the preferred magnitude
			if ( !foundPreferredMag ) {
				OriginPtr origin = query()->getOriginByMagnitude(event->preferredMagnitudeID());
				if ( origin ) {
					query()->load(origin.get());

					if ( !_withStationMagnitudes ) {
						removeAllStationMagnitudes(origin.get());
					}

					if ( _ignoreArrivals ) {
						removeAllArrivals(origin.get());
					}

					if ( _preferredOnly && !_allMagnitudes ) {
						MagnitudePtr netMag;
						while ( origin->magnitudeCount() > 0 ) {
							if ( origin->magnitude(0)->publicID() == event->preferredMagnitudeID() ) {
								netMag = origin->magnitude(0);
							}

							origin->removeMagnitude(0);
						}

						if ( netMag ) {
							origin->add(netMag.get());
						}
					}

					ep->add(origin.get());
				}
			}
		}


	private:
		bool _preferredOnly;
		bool _allMagnitudes;
		bool _ignoreArrivals;
		bool _withPicks;
		bool _withAmplitudes;
		bool _withStationMagnitudes;
		bool _withFocalMechanisms;
		bool _withChilds;

		string _outputFile;
		string _publicIDParam;
		string _stationIDParam;
		string _eventIDParam;
		string _originIDParam;
		string _pickIDParam;

		vector<string> _publicIDs;
		vector<string> _stationIDs;
		vector<string> _eventIDs;
		vector<string> _originIDs;
		vector<string> _pickIDs;

		set<string> _pickIDSet;
		set<string> _amplitudeIDSet;

		XMLArchive *_archive{nullptr};
		stringbuf _archiveBuf;
};


int main(int argc, char** argv) {
	XMLDump app(argc, argv);
	return app.exec();
}
