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

#include <seiscomp/client/application.h>
#include <seiscomp/datamodel/config.h>
#include <seiscomp/datamodel/dataavailability.h>
#include <seiscomp/datamodel/diff.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/inventory.h>
#include <seiscomp/datamodel/object.h>
#include <seiscomp/datamodel/qualitycontrol.h>
#include <seiscomp/datamodel/routing.h>
#include <seiscomp/datamodel/journaling.h>
#include <seiscomp/io/archive/xmlarchive.h>

#include <iostream>


class XMLMerge : public Seiscomp::Client::Application {

	public:
		XMLMerge(int argc, char **argv) : Seiscomp::Client::Application(argc, argv) {
			setMessagingEnabled(false);
			setDatabaseEnabled(false, false);
			setDaemonEnabled(false);
		}

		bool init() {
			if ( !Seiscomp::Client::Application::init() )
				return false;

			_files = commandline().unrecognizedOptions();
			if ( _files.empty() ) {
				std::cerr << "No input files given" << std::endl;
				printUsage();
				return false;
			}

			return true;
		}

		void printUsage() const {
			std::cout << std::endl << "Usage:" << std::endl
			          << "  scxmlmerge [options] inputFiles"
			          << std::endl << std::endl;
			std::cout << "Merge the content of multiple XML files. "
			             "Different root elements like EventParameters and "
			             "Inventory may be combined."
			          << std::endl;

			Seiscomp::Client::Application::printUsage();

			std::cout << "Examples:" << std::endl;
			std::cout << "Merge all SeisComP elements the from 2 XML files into "
			             "a single XML file." << std::endl
			          << "  scxmlmerge file1.xml file2.xml > file.xml"
			          << std::endl << std::endl;
			std::cout << "Merge the EventParameters and Config elements from 2 "
			             "XML files into a single XML file" << std::endl
			          << "  scxmlmerge -E -C file1.xml file2.xml > file.xml"
			          << std::endl << std::endl;
		}

		void createCommandLineDescription() {
			commandline().addGroup("Dump");
			commandline().addOption("Dump", "availability,Y", "Include DataAvailability.");
			commandline().addOption("Dump", "config,C", "Include Config.");
			commandline().addOption("Dump", "event,E", "Include EventParameters.");
			commandline().addOption("Dump", "inventory,I", "Include Inventory.");
			commandline().addOption("Dump", "journaling,J", "Include Journaling.");
			commandline().addOption("Dump", "quality,Q", "Include QualityControl.");
			commandline().addOption("Dump", "routing,R", "Include Routing.");
			commandline().addGroup("Options");
			commandline().addOption("Options", "ignore-bad-files",
			                        "Tolerate empty or corrupted input files "
			                        "and continue without interruption.");
		}

		bool run() {
			Seiscomp::DataModel::PublicObject::SetRegistrationEnabled(false);

			std::vector<Seiscomp::DataModel::ObjectPtr> storage;

			// set up root objects to collect
			bool collectDA      = commandline().hasOption("availability");
			bool collectCfg     = commandline().hasOption("config");
			bool collectEP      = commandline().hasOption("event");
			bool collectInv     = commandline().hasOption("inventory");
			bool collectJ       = commandline().hasOption("journaling");
			bool collectQC      = commandline().hasOption("quality");
			bool collectRouting = commandline().hasOption("routing");

			bool ignoreBadFiles       = commandline().hasOption("ignore-bad-files");

			bool collectAll = !collectEP && !collectInv && !collectCfg &&
			                  !collectRouting && !collectQC && !collectDA &&
			                  !collectJ;

			if ( collectAll || collectEP )
				registerRootObject(Seiscomp::DataModel::EventParameters::ClassName());
			if ( collectAll || collectInv )
				registerRootObject(Seiscomp::DataModel::Inventory::ClassName());
			if ( collectAll || collectCfg )
				registerRootObject(Seiscomp::DataModel::Config::ClassName());
			if ( collectAll || collectRouting )
				registerRootObject(Seiscomp::DataModel::Routing::ClassName());
			if ( collectAll || collectQC )
				registerRootObject(Seiscomp::DataModel::QualityControl::ClassName());
			if ( collectAll || collectDA )
				registerRootObject(Seiscomp::DataModel::DataAvailability::ClassName());
			if ( collectAll || collectJ )
				registerRootObject(Seiscomp::DataModel::Journaling::ClassName());

			std::map<std::string, Objects>::iterator it;

			for ( size_t i = 0; i < _files.size(); ++i ) {
				Seiscomp::IO::XMLArchive ar;
				if ( !ar.open(_files[i].c_str()) ) {
					std::cerr << "Failed to read content from file: "
					          << _files[i] << std::endl;
					// ignore files which could not be read
					if ( ignoreBadFiles ) {
						continue;
					}

					return false;
				}

				std::cerr << "+ " << _files[i] << std::endl;
				Seiscomp::DataModel::ObjectPtr obj;
				bool first = true;
				while ( true ) {
					Seiscomp::Core::Generic::ObjectIterator
					        <Seiscomp::DataModel::ObjectPtr> objIt(obj, first);
					first = false;
					ar >> objIt;
					if ( !obj ) break;

					std::cerr << "    " << obj->className();
					it = _objectBins.find(obj->className());
					if ( it == _objectBins.end() ) {
						std::cerr << " (ignored)";
					}
					else {
						storage.push_back(obj.get());
						it->second.push_back(obj.get());
					}
					std::cerr << std::endl;
				}

				ar.close();
			}

			if ( _objectBins.empty() ) {
				std::cerr << "No output has been generated" << std::endl;
				return false;
			}

			Seiscomp::DataModel::DiffMerge merger;
			merger.setLoggingLevel(4);

			Seiscomp::IO::XMLArchive ar;
			ar.create("-");
			ar.setFormattedOutput(true);

			for ( it = _objectBins.begin(); it != _objectBins.end(); ++it ) {
				const std::string &name = it->first;
				Objects &objs = it->second;
				if ( objs.size() == 1 ) {
					std::cerr << "writing " << name << " object" << std::endl;
					ar << objs.front();
				}
				else if ( objs.size() > 1 ) {
					std::cerr << "merging " << objs.size()
					          << " " << name << " objects" << std::endl;
					Seiscomp::DataModel::ObjectPtr obj =
					        dynamic_cast<Seiscomp::DataModel::Object*>(
					            Seiscomp::Core::ClassFactory::Create(it->first));
					merger.merge(obj.get(), objs);
					merger.showLog();
					ar << obj;
				}
			}

			ar.close();

			return true;
		}

	private:
		void registerRootObject(const std::string &name) {
			_objectBins[name] = std::vector<Seiscomp::DataModel::Object*>();
		}

	private:
		typedef std::vector<Seiscomp::DataModel::Object*> Objects;

		std::vector<std::string>            _files;
		std::map<std::string, Objects>      _objectBins;
};


int main(int argc, char **argv) {
	XMLMerge app(argc, argv);
	return app();
}

