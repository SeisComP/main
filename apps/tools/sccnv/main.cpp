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

#define SEISCOMP_COMPONENT ExchangeTest

#include <seiscomp/logging/log.h>
#include <seiscomp/client/application.h>
#include <seiscomp/io/importer.h>
#include <seiscomp/io/exporter.h>
#include <seiscomp/io/archive/xmlarchive.h>

#include <iostream>


using namespace Seiscomp;
using namespace Seiscomp::Client;
using namespace Seiscomp::Core;
using namespace Seiscomp::IO;


class ConvertApp : public Application {
	public:
		ConvertApp(int argc, char **argv) : Application(argc, argv) {
			setMessagingEnabled(false);
			setDatabaseEnabled(false, false);
			setLoggingToStdErr(true);
			_indentation = 2;
		}

	protected:
		void createCommandLineDescription() {
			commandline().addGroup("Formats");
			commandline().addOption("Formats", "format-list",
			                        "list all supported formats");
			commandline().addGroup("Input");
			commandline().addOption("Input", "input,i",
			                        "input stream [format:][file], default: trunk:-",
			                        &_inputStream, false);
			commandline().addGroup("Output");
			commandline().addOption("Output", "output,o",
			                        "output stream [format:][file], default trunk:-",
			                        &_outputStream, false);
			commandline().addOption("Output", "formatted,f", "use formatted output");
			commandline().addOption("Output", "indent", "formatted line indent",
			                        &_indentation);
		}

		void printUsage() const {
			std::cout << "Usage:" << std::endl << "  sccnv [options]"
			          << std::endl << std::endl
			          << "Convert content between different formats."
			          << std::endl;

			Client::Application::printUsage();

			std::cout << "Examples:" << std::endl;
			std::cout << "Print the list of supported formats"
			          << std::endl
			          << "  sccnv --format-list"
			          << std::endl << std::endl
			          << "Convert an event parameter file in SCML format to QuakeML, store the content in a file"
			          << std::endl
			          << "  sccnv -i seiscomp.xml -o qml1.2:quakeml.xml"
			          << std::endl << std::endl;
		}

		bool run() {
			if ( commandline().hasOption("format-list") ) {
				ImporterFactory::ServiceNames *importFormats = ImporterFactory::Services();
				ExporterFactory::ServiceNames *exportFormats = ExporterFactory::Services();

				if ( importFormats ) {
					std::cout << "Input formats: ";
					for ( ImporterFactory::ServiceNames::iterator it = importFormats->begin();
					      it != importFormats->end(); ++it ) {
						if ( it != importFormats->begin() )
							std::cout << ", ";
						std::cout << *it;
					}
					std::cout << std::endl;
					delete importFormats;
				}

				if ( exportFormats ) {
					std::cout << "Output formats: ";
					for ( ExporterFactory::ServiceNames::iterator it = exportFormats->begin();
					      it != exportFormats->end(); ++it ) {
						if ( it != exportFormats->begin() )
							std::cout << ", ";
						std::cout << *it;
					}
					std::cout << std::endl;
					delete exportFormats;
				}

				return true;
			}


			if ( _inputStream.empty() && _outputStream.empty() ) {
				SEISCOMP_ERROR("No input and output stream given, nothing to do");
				return false;
			}

			size_t inputSplitter = _inputStream.find(':');
			size_t outputSplitter = _outputStream.find(':');

			std::string inputFormat = inputSplitter != std::string::npos?_inputStream.substr(0, inputSplitter):"";
			std::string outputFormat = outputSplitter != std::string::npos?_outputStream.substr(0, outputSplitter):"";

			std::string inputFile = inputSplitter != std::string::npos?_inputStream.substr(inputSplitter + 1):_inputStream;
			std::string outputFile = outputSplitter != std::string::npos?_outputStream.substr(outputSplitter + 1):_outputStream;

			/*
			if ( inputFormat == outputFormat ) {
				SEISCOMP_ERROR("Same input and output format, nothing to do");
				return false;
			}
			*/

			if ( inputFormat.empty() ) inputFormat = "trunk";
			if ( outputFormat.empty() ) outputFormat = "trunk";

			if ( inputFile.empty() ) inputFile = "-";
			if ( outputFile.empty() ) outputFile = "-";

			BaseObjectPtr obj;

			ImporterPtr imp = ImporterFactory::Create(inputFormat.c_str());
			if ( !imp ) {
				SEISCOMP_ERROR("Unknown input format %s", inputFormat.c_str());
				return false;
			}

			obj = imp->read(inputFile.c_str());
			if ( !imp->withoutErrors() )
				SEISCOMP_WARNING("Got errors during reading");

			if ( !obj ) {
				SEISCOMP_ERROR("Input empty, nothing to do");
				return false;
			}


			ExporterPtr exp = ExporterFactory::Create(outputFormat.c_str());
			if ( !exp ) {
				SEISCOMP_ERROR("Unknown output format %s", outputFormat.c_str());
				return false;
			}

			exp->setFormattedOutput(commandline().hasOption("formatted"));
			exp->setIndent(_indentation);
			if ( !exp->write(outputFile.c_str(), obj.get()) ) {
				SEISCOMP_ERROR("Error while writing to %s", outputFile.c_str());
				return false;
			}

			return true;
		}

	private:
		std::string _inputStream;
		std::string _outputStream;
		int _indentation;
};


int main(int argc, char **argv) {
	int retCode = EXIT_SUCCESS;

	// Create an own block to make sure the application object
	// is destroyed when printing the overall objectcount
	{
		ConvertApp app(argc, argv);
		retCode = app.exec();
	}

	return retCode;
}
