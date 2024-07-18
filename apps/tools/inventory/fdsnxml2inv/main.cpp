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

#define SEISCOMP_COMPONENT STAXML

#include "convert2fdsnxml.h"
#include "convert2sc.h"

#include <fdsnxml/xml.h>
#include <fdsnxml/fdsnstationxml.h>

#include <seiscomp/client/application.h>
#include <seiscomp/client/inventory.h>
#include <seiscomp/datamodel/inventory.h>
#include <seiscomp/io/archive/xmlarchive.h>
#include <seiscomp/utils/files.h>
#include <seiscomp/logging/log.h>

#include <iostream>
#include <cstring>
#include <cstdlib>


using namespace std;
using namespace Seiscomp;


namespace {

inline bool leap(int y) {
	return (y % 400 == 0 || (y % 4 == 0 && y % 100 != 0));
}


inline int doy(const Core::Time &t) {
	static const int DOY[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
	int year, month, day;
	t.get(&year, &month, &day);
	return (DOY[month-1] + (leap(year) && month > 2)) + day;
}

}


class SyncStationXML : public Client::Application {
	public:
		SyncStationXML(int argc, char **argv) : Client::Application(argc, argv) {
			setMessagingEnabled(false);
			setDatabaseEnabled(false, false);
			setLoggingToStdErr(true);
			setDaemonEnabled(false);

			_convertBack = false;
			_activeConverter = NULL;
			_strictNsCheck = true;
		}


		bool initConfiguration() {
			if ( !Client::Application::initConfiguration() ) return false;
			setLoggingToStdErr(true);
			return true;
		}


		void createCommandLineDescription() {
			Client::Application::createCommandLineDescription();

			commandline().addGroup("Convert");

			commandline().addOption("Convert", "relaxed-ns-check",
			                        "Enable relaxed XML namespace checks. This "
			                        "will accept also tags within a different "
			                        "namespace than defined in the supported "
			                        "schema.");
			commandline().addOption("Convert", "to-staxml",
			                        "Convert from SeisComPML to StationXML "
			                        "expecting SeisComPML as input.");
			commandline().addOption("Convert", "formatted,f",
			                        "Enable formatted XML output.");
			commandline().addOption("Convert", "log-stages",
			                        "Add more output to stderr for all channel "
			                        "response stages when converting from "
			                        "StationXML.");
			commandline().addOption("Convert", "only-instruments",
			                        "Convert only instruments and ignore networks.");
		}

		bool validateParameters() {
			if ( !Client::Application::validateParameters() ) return false;

			if ( commandline().hasOption("relaxed-ns-check") )
				_strictNsCheck = false;

			if ( commandline().hasOption("to-staxml") )
				_convertBack = true;

			if ( commandline().hasOption("only-instruments") ) {
				if ( _convertBack ) {
					cout << "--only-instruments is not supported in combination with --to-staxml" << endl;
					return false;
				}
				_onlyInstruments = true;
			}

			const vector<string> &args = commandline().unrecognizedOptions();
			if ( args.size() < 1 ) {
				cout << "Must provide exactly one input file" << endl;
				printUsage();
				return false;
			}
			if ( args.size() > 2 ) {
				cout << "You must provide exactly one input and you may provide "
				        "one output file" << endl;
				printUsage();
				return false;
			}


			_inputFile = args[0];

			if ( args.size() > 1 )
				_outputFile = args[1];
			else
				_outputFile = "-";

			return true;
		}

		void printUsage() const {
			cout << "Usage:"  << endl << "  fdsnxml2inv [options] input [output=stdout]"
			     << endl << endl
			     << "Convert station inventory between FDSN StationXML format "
			        "and SeisComP XML." << endl;

			Seiscomp::Client::Application::printUsage();

			cout << "Examples:" << endl;
			cout << "Convert FDSN StationXML to SCML" << endl
			     << "  fdsnxml2inv fdsn-station.xml inventory.xml" << endl;
			cout  << endl << "Convert SCML to FDSN StationXML" << endl
			     << "  fdsnxml2inv --to-staxml inventory.xml fdsn-station.xml" << endl;
		}


		void exit(int returnCode) {
			Client::Application::exit(returnCode);

			if ( _activeConverter )
				_activeConverter->interrupt();
		}

		bool convertToSC() {
			DataModel::InventoryPtr inv;

			inv = Client::Inventory::Instance()->inventory();
			if ( !inv ) {
				cerr << "No inventory read from inventory db" << endl;
				cerr << "Create empty one" << endl;
				inv = new DataModel::Inventory;
			}

			Convert2SC cnv(inv.get());
			cnv.setLogStages(commandline().hasOption("log-stages"));

			_activeConverter = &cnv;

			if ( _exitRequested ) {
				return false;
			}

			FDSNXML::Importer imp;
			imp.setStrictNamespaceCheck(_strictNsCheck);

			cerr << "Processing " << _inputFile << endl;
			cerr << " - parsing StationXML" << endl;

			Core::BaseObjectPtr obj = imp.read(_inputFile);
			if ( !obj ) {
				cerr << " - empty or invalid XML: skipping" << endl;
				return false;
			}

			FDSNXML::FDSNStationXMLPtr msg = FDSNXML::FDSNStationXML::Cast(obj);
			if ( !msg ) {
				cerr << " - no FDSNStationXML found: skipping" << endl;
				return false;
			}

			cerr << " - converting into SeisComP-XML" << endl;

			cnv.push(msg.get());

			// Clean up the inventory after pushing all messages
			cnv.cleanUp();

			if ( _exitRequested ) {
				cerr << "Exit requested" << endl;
				return false;
			}

			if ( _onlyInstruments ) {
				while ( inv->networkCount() > 0 ) {
					inv->removeNetwork(0);
				}
			}

			cerr << "Finished processing" << endl;

			IO::XMLArchive ar;

			if ( !_outputFile.empty() ) {
				cerr << "Writing inventory to " << _outputFile << endl;
				if ( !ar.create(_outputFile.c_str()) ) {
					cerr << " - failed to create file" << endl;
					return false;
				}
			}
			else {
				cerr << "Writing inventory to stdout" << endl;
				if ( !ar.create("-") ) {
					cerr << " - failed to open output" << endl;
					return false;
				}
			}

			ar.setFormattedOutput(commandline().hasOption("formatted"));
			ar << inv;
			ar.close();

			return true;
		}


		bool convertToStaXML() {
			DataModel::InventoryPtr inv;

			IO::XMLArchive ar;
			if ( !ar.open(_inputFile.c_str()) ) {
				cerr << "Cannot open " << _inputFile << endl;
				return false;
			}

			ar >> inv;

			if ( !inv ) {
				cerr << "No inventory read and thus nothing to convert" << endl;
				return false;
			}

			FDSNXML::FDSNStationXML msg;
			msg.setSender(agencyID());
			msg.setCreated(Core::Time::GMT());
			msg.setSource("SeisComP");
			Convert2FDSNStaXML cnv(&msg);
			_activeConverter = &cnv;

			cerr << "Processing inventory" << endl;
			cerr << " - converting to StationXML" << endl;
			cnv.push(inv.get());
			cerr << "Finished processing" << endl;

			if ( _exitRequested ) return false;

			if ( _outputFile.empty() ) {
				cerr << "Writing stations to stdout" << endl;
				_outputFile = "-";
			}
			else
				cerr << "Writing stations to " << _outputFile << endl;

			FDSNXML::Exporter out;
			out.setFormattedOutput(commandline().hasOption("formatted"));
			out.setIndent(1);
			return out.write(_outputFile, &msg);
		}


		bool run() {
			if ( _convertBack )
				return convertToStaXML();
			else
				return convertToSC();
		}


	private:
		Converter *_activeConverter;
		bool       _convertBack;
		string     _inputFile;
		string     _outputFile;
		bool       _strictNsCheck;
		bool       _onlyInstruments{false};
};


int main(int argc, char **argv) {
	SyncStationXML app(argc, argv);
	return app();
}
