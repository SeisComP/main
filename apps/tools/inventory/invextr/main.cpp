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
#include <seiscomp/io/archive/xmlarchive.h>
#include <seiscomp/geo/boundingbox.h>
#include <seiscomp/core/strings.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>
#include <set>


using namespace std;
using namespace Seiscomp;


bool filter(const std::vector<string> &whitelist, const string &id) {
	if ( whitelist.empty() ) return false;

	for ( size_t i = 0; i < whitelist.size(); ++i ) {
		if ( Core::wildcmp(whitelist[i], id) )
			return false;
	}

	return true;
}


class InventoryExtractor : public Client::Application {
	public:
		InventoryExtractor(int argc, char **argv)
		: Client::Application(argc, argv) {
			setMessagingEnabled(false);
			setDatabaseEnabled(false, false);
			setLoggingToStdErr(true);
		}


		void createCommandLineDescription() override {
			Client::Application::createCommandLineDescription();
			commandline().addGroup("Extract");
			commandline().addOption("Extract", "begin",
			                        "Begin time to consider streams.", &_begin);
			commandline().addOption("Extract", "end",
			                        "End time to consider streams.", &_end);
			commandline().addOption("Extract", "chans",
			                        "A comma separated list of channel id's to "
			                        "extract which can contain wildcards. "
			                        "Default: *.*.*.* meaning all streams.",
			                        &_chanIDs);
			commandline().addOption("Extract", "nslc",
			                        "Stream list file to be used for extracting "
			                        "inventory. Wildcards can be used. --chans "
			                        "is ignored.", &_nslc);
			commandline().addOption("Extract", "region,r",
			                        "Filter streams by geographic region given as "
			                        "\"South,East,North,West\". Region is "
			                        "unused by default.", &_region);
			commandline().addOption("Extract", "rm",
			                        "Remove the given channels instead of "
			                        "extracting them.");
			commandline().addOption("Extract", "formatted,f",
			                        "Enable formatted XML output.");
		}


		bool validateParameters() override {
			if ( !Client::Application::validateParameters() ) {
				return false;
			}
			_remove = commandline().hasOption("rm");
			return true;
		}

		void printUsage() const override {
			cout << "Usage:"  << endl
			     << "  " << name() << " [OPTIONS] [input=stdin] [output=stdout]"
			     << endl << endl
			     << "Extract or remove streams from inventory." << endl;

			Client::Application::printUsage();

			cout << "Examples:" << endl;
			cout << "Extract inventory for entire GR network" << endl
			     << "  " << name() << " -f --chans GR.*.*.* inventory_all.xml > inventory_GR.xml"
			     << endl << endl;
			cout << "Extract inventory for all stations within the given region" << endl
			     << "  " << name() << " -f -r -10,0,50,15 inventory_all.xml > inventory_GR.xml"
			     << endl << endl;
			cout << "Remove inventory for entire GR network" << endl
			     << "  " << name() << " -f --rm --chans GR.*.*.* inventory_all.xml > inventory_others.xml"
			     << endl << endl;
		}


		bool run() override {
			OPT(Seiscomp::Core::Time) begin;
			if ( !_begin.empty() ) {
				begin = Seiscomp::Core::Time::FromString(_begin);
			}
			OPT(Seiscomp::Core::Time) end = Seiscomp::Core::Time::GMT();
			if ( !_end.empty() ) {
				end = Seiscomp::Core::Time::FromString(_end);
			}
			vector<string> chanIDs;
			if ( !_nslc.empty() ) {
				ifstream file(_nslc);
				if ( file.is_open() ) {
					string line;
					while ( getline(file, line) ) {
						line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
						if ( line.empty() ) {
							continue;
						}
						chanIDs.push_back(line);
					}
					file.close();
				}
				else {
					cerr << "Unable to open NSLC list file " << _nslc << endl;
					return false;
				}
			}
			else {
				Core::split(chanIDs, _chanIDs.c_str(), ",");
			}

			if ( !_region.empty() ) {
				vector<string> region;
				Core::split(region, _region.c_str(), ",");
				if ( region.size() != 4 ) {
					cerr << "Region: expected 4 values, got " << int(region.size()) << " " << _region
					     << endl;
					return false;
				}

				Core::fromString(_bBox.south, region[0]);
				Core::fromString(_bBox.west, region[1]);
				Core::fromString(_bBox.north, region[2]);
				Core::fromString(_bBox.east, region[3]);
				_bBox.normalize();
			}


			vector<string> opts = commandline().unrecognizedOptions();
			string input("-"), output("-");

			if ( opts.empty() ) {
				cerr << "Reading inventory from stdin. Use Ctrl + C to interrupt."
				     << endl;
			}
			else {
				input = opts[0];
			}

			if ( opts.size() > 1 ) {
				output = opts[1];
			}

			DataModel::InventoryPtr inv;

			IO::XMLArchive ar;
			if ( !ar.open(input.c_str()) ) {
				cerr << "Unable to open " << input << endl;
				return false;
			}

			ar >> inv;
			if ( !inv ) {
				cerr << "No inventory found in " << input << endl;
				return false;
			}

			ar.close();

			string id0;
			set<string> usedSensors, usedDataloggers;

			for ( size_t n = 0; n < inv->networkCount(); ) {
				DataModel::Network *net = inv->network(n);
				id0 = net->code();

				for ( size_t s = 0; s < net->stationCount(); ) {
					DataModel::Station *sta = net->station(s);
					string id1 = id0 + "." + sta->code();

					for ( size_t l = 0; l < sta->sensorLocationCount(); ) {
						DataModel::SensorLocation *loc = sta->sensorLocation(l);
						string id2 = id1 + "." + loc->code();

						// check coordinate of sensor location
						if ( !_bBox.isEmpty() ) {
							Geo::GeoCoordinate coord(loc->latitude(), loc->longitude());
							// ignore if
							// _remove IS active AND location IS within OR
							// _remove IS NOT active AND location is NOT within
							if ( _remove == _bBox.contains(coord) ) {
								sta->removeSensorLocation(l);
								continue;
							}
						}

						for ( size_t c = 0; c < loc->streamCount(); ) {
							DataModel::Stream *cha = loc->stream(c);
							string id3 = id2 + "." + cha->code();

							if ( _remove != filter(chanIDs, id3) ) {
								loc->removeStream(c);
								continue;
							}

							try {
								if ( begin >= cha->end() ) {
									loc->removeStream(c);
									continue;
								}
							} catch ( ... ) { }

							try {
								if ( end <= cha->start() ) {
									loc->removeStream(c);
									continue;
								}
							} catch ( ... ) { }

							// Keep track of used sensors and dataloggers
							if ( !cha->sensor().empty() ) {
								usedSensors.insert(cha->sensor());
							}

							if ( !cha->datalogger().empty() ) {
								usedDataloggers.insert(cha->datalogger());
							}

							++c;
						}

						if ( loc->streamCount() == 0 ) {
							sta->removeSensorLocation(l);
							continue;
						}

						try {
							if ( begin >= loc->end() ) {
								sta->removeSensorLocation(l);
								continue;
							}
						} catch ( ... ) { }

						try {
							if ( end <= loc->start() ) {
								sta->removeSensorLocation(l);
								continue;
							}
						} catch ( ... ) { }

						++l;
					}

					if ( sta->sensorLocationCount() == 0 ) {
						net->removeStation(s);
						continue;
					}

					try {
						if ( begin >= sta->end() ) {
							net->removeStation(s);
							continue;
						}
					} catch ( ... ) { }

					try {
						if ( end <= sta->start() ) {
							net->removeStation(s);
							continue;
						}
					} catch ( ... ) { }


					++s;
				}

				if ( net->stationCount() == 0 ) {
					inv->removeNetwork(n);
					continue;
				}

				try {
					if ( begin >= net->end() ) {
						inv->removeNetwork(n);
						continue;
					}
				} catch ( ... ) { }

				try {
					if ( end <= net->start() ) {
						inv->removeNetwork(n);
						continue;
					}
				} catch ( ... ) { }

				++n;
			}

			set<string> usedResponses;

			// Remove unused sensors
			for ( size_t s = 0; s < inv->sensorCount(); ) {
				if ( usedSensors.find(inv->sensor(s)->publicID()) == usedSensors.end() ) {
					inv->removeSensor(s);
				}
				else {
					usedResponses.insert(inv->sensor(s)->response());
					++s;
				}
			}

			// Remove unused dataloggers
			for ( size_t d = 0; d < inv->dataloggerCount(); ) {
				DataModel::Datalogger *dl = inv->datalogger(d);
				if ( usedDataloggers.find(dl->publicID()) == usedDataloggers.end() ) {
					inv->removeDatalogger(d);
				}
				else {
					for ( size_t i = 0; i < dl->decimationCount(); ++i ) {
						DataModel::Decimation *deci = dl->decimation(i);
						try {
							vector<string> filters;
							Core::split(filters, deci->analogueFilterChain().content().c_str(), " ");

							for ( size_t j = 0; j < filters.size(); ++j ) {
								if ( filters[j].empty() ) continue;
								usedResponses.insert(filters[j]);
							}
						}
						catch ( ... ) {}

						try {
							vector<string> filters;
							Core::split(filters, deci->digitalFilterChain().content().c_str(), " ");

							for ( size_t j = 0; j < filters.size(); ++j ) {
								if ( filters[j].empty() ) continue;
								usedResponses.insert(filters[j]);
							}
						}
						catch ( ... ) {}
					}

					++d;
				}
			}


			// Remove unused responses
			for ( size_t i = 0; i < inv->responsePAZCount(); ) {
				if ( usedResponses.find(inv->responsePAZ(i)->publicID()) == usedResponses.end() ) {
					inv->removeResponsePAZ(i);
				}
				else
					++i;
			}

			for ( size_t i = 0; i < inv->responsePolynomialCount(); ) {
				if ( usedResponses.find(inv->responsePolynomial(i)->publicID()) == usedResponses.end() ) {
					inv->removeResponsePolynomial(i);
				}
				else {
					++i;
				}
			}

			for ( size_t i = 0; i < inv->responseFAPCount(); ) {
				if ( usedResponses.find(inv->responseFAP(i)->publicID()) == usedResponses.end() ) {
					inv->removeResponseFAP(i);
				}
				else {
					++i;
				}
			}

			for ( size_t i = 0; i < inv->responseFIRCount(); ) {
				if ( usedResponses.find(inv->responseFIR(i)->publicID()) == usedResponses.end() ) {
					inv->removeResponseFIR(i);
				}
				else {
					++i;
				}
			}

			for ( size_t i = 0; i < inv->responseIIRCount(); ) {
				if ( usedResponses.find(inv->responseIIR(i)->publicID()) == usedResponses.end() ) {
					inv->removeResponseIIR(i);
				}
				else {
					++i;
				}
			}

			if ( !ar.create(output.c_str()) ) {
				cerr << "Unable to create output " << output << endl;
				return false;
			}

			ar.setFormattedOutput(commandline().hasOption("formatted"));
			ar << inv;
			ar.close();

			return true;
		}

	private:
		string              _nslc;
		string              _begin;
		string              _end;
		string              _chanIDs{"*.*.*.*"};
		bool                _remove{false};
		string              _region;
		Geo::GeoBoundingBox _bBox;
};


int main(int argc, char **argv) {
	return InventoryExtractor(argc, argv)();
}
