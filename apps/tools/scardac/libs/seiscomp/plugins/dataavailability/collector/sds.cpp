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

#define SEISCOMP_COMPONENT SDSDataAvailabilityCollector

#include "sds.h"

#include <seiscomp/plugins/dataavailability/utils.hpp>

#include <seiscomp/core/system.h>
#include <seiscomp/core/typedarray.h>
#include <seiscomp/datamodel/waveformstreamid.h>
#include <seiscomp/io/records/mseedrecord.h>
#include <seiscomp/logging/log.h>
#include <seiscomp/system/environment.h>

#include <utility>
#include <vector>


using namespace std;
namespace fs = boost::filesystem;

namespace Seiscomp {
namespace DataAvailability {

REGISTER_DATAAVAILABILITY_COLLECTOR(SDSCollector, "sds");

namespace {


} // ns anonymous
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
SDSCollector::RecordIterator::RecordIterator(string file,
                                    const DataModel::WaveformStreamID &wid)
: _file(std::move(file)), _sid(streamID(wid)),
  _input(&_stream, Array::DOUBLE, Record::DATA_ONLY) {
	if ( !_stream.setSource(_file) ) {
		throw CollectorException("could not open record file");
	}

	_stream.addStream(wid.networkCode(), wid.stationCode(), wid.locationCode(),
	                  wid.channelCode());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool SDSCollector::RecordIterator::valid() const {
	return _rec.get() != nullptr;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool SDSCollector::RecordIterator::next() {
	while ( !_abortRequested ) {
		_rec = _input.next();

		if ( !valid() ) {
			return false;
		}

		// SEISCOMP_DEBUG("%s: Received record: %i samples, %.1fHz, %s ~ %s",
		//                _sid.c_str(), _rec->sampleCount(),
		//                _rec->samplingFrequency(), _rec->startTime().iso().c_str(),
		//                _rec->endTime().iso().c_str());

		if ( _rec->streamID() != _sid ) {
			SEISCOMP_WARNING("%s: Received record with invalid stream id "
			                 "while reading file: %s",
			                 _sid.c_str(), _file.c_str());
			continue;
		}

		_endTime = _rec->endTime();

		auto *msRec = IO::MSeedRecord::Cast(_rec.get());
		if ( msRec ) {
			_quality = string(1, msRec->dataQuality());
		}
		else {
			_quality = "";
		}

		return true;
	}

	// only reached if abort was requested
	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const Core::Time& SDSCollector::RecordIterator::startTime() const {
	return _rec->startTime();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const Core::Time& SDSCollector::RecordIterator::endTime() const {
	return _endTime;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
double SDSCollector::RecordIterator::sampleRate() const {
	return _rec->samplingFrequency();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const std::string& SDSCollector::RecordIterator::quality() const {
	return _quality;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool SDSCollector::setSource(const char *source) {
	if ( !Collector::setSource(source) ) {
		return false;
	}

	string archive(Environment::Instance()->absolutePath(source));

	// build vector of archive years heavily used later on
	fs::directory_iterator end_itr;
	try {
		_basePath = SC_FS_PATH(archive);
		for ( fs::directory_iterator itr(archive); itr != end_itr; ++itr ) {
			if ( _abortRequested ) {
				return false;
			}

			fs::path dir = SC_FS_DE_PATH(itr);
			if ( !fs::is_directory(dir) ) {
				continue;
			}

			std::string name = SC_FS_FILE_NAME(dir);
			int year;
			if ( name.length() == 4 && Core::fromString(year, name) ) {
				_archiveYears.emplace_back(make_pair(year, dir));
			}
			else {
				SEISCOMP_WARNING("Invalid archive directory: %s",
				                 dir.string().c_str());
				continue;
			}
		}
	}
	catch ( ... ) {
		throw CollectorException("could not read archive years from source: " +
		                         archive);
	}

	// sort years
	sort(_archiveYears.begin(), _archiveYears.end(),
	     [](const ArchiveYearItem &a, const ArchiveYearItem &b) {
		return a.first < b.first;
	});

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void SDSCollector::setStartTime(Core::Time startTime) {
	Collector::setStartTime(startTime);
	_startYear = 0;
	_startDOY = 0;
	_startTime->get2(&(*_startYear), &(*_startDOY));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void SDSCollector::setEndTime(Core::Time endTime) {
	Collector::setEndTime(endTime);
	_endYear = 0;
	_endDOY = 0;
	_endTime->get2(&(*_endYear), &(*_endDOY));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void SDSCollector::reset() {
	Collector::reset();
	_archiveYears.clear();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void SDSCollector::collectWaveformIDs(WaveformIDs &wids) {
	// Year/NET/STA/CHA.D/NET.STA.LOC.CHA.D.YEAR.DAY

	wids.clear();

	for ( const auto &year : _archiveYears ) {
		if ( _abortRequested || (_endYear && year.first > *_endYear) ) {
			break;
		}

		if ( _startYear && year.first < *_startYear) {
			continue;
		}

		scanDirectory(wids, year.second);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void SDSCollector::collectChunks(DataChunks &chunks,
                                 const DataModel::WaveformStreamID &wid) {
	// Year/NET/STA/CHA.D/NET.STA.LOC.CHA.D.YEAR.DAY

	chunks.clear();
	SC_FS_DECLARE_PATH(widPath, wid.networkCode())
	widPath /= SC_FS_PATH(wid.stationCode()) /
	           SC_FS_PATH(wid.channelCode() + ".D");

	auto sid = streamID(wid);

	for ( const auto &yearItem : _archiveYears ) {
		if ( _abortRequested || (_endYear && yearItem.first > *_endYear) ) {
			break;
		}

		if ( _startYear && yearItem.first < *_startYear) {
			continue;
		}

		const auto &year = yearItem.second;

		string fileDir = (SC_FS_FILE_PATH(year) / widPath).string() +
		                 fs::path::preferred_separator;
		fs::directory_iterator end_it;
		try {
			for ( fs::directory_iterator it(year / widPath);
			      it != end_it && !_abortRequested; ++it ) {
				std::string file = SC_FS_FILE_NAME(SC_FS_DE_PATH(it));
				auto idDate = fileStreamID(file);

				if ( idDate.streamID == sid &&
				     checkTimeWindow(idDate.year, idDate.doy) ) {
					chunks.push_back(fileDir + file);
				}
			}
		}
		catch ( ... ) {}
	}

	if ( _abortRequested ) {
		return;
	}

	// sort files by name
	sort(chunks.begin(), chunks.end());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool SDSCollector::chunkTimeWindow(Core::TimeWindow &window,
                                   const std::string &chunk) {
	// NET.STA.LOC.CHA.D.YEAR.DAY
	vector<string> toks;
	int year;
	int doy;
	Core::split(toks, SC_FS_FILE_NAME(SC_FS_PATH(chunk)).c_str(), ".", false);
	if ( toks.size() == 7 &&
	     toks[5].length() == 4 && Core::fromString(year, toks[5]) &&
	     toks[6].length() == 3 && Core::fromString(doy, toks[6]) ) {

		Core::Time start;
		start.set2(year, doy-1, 0, 0, 0, 0);

		window.set(start, start + Core::TimeSpan(86400, 0));
		return true;
	}

	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Core::Time SDSCollector::chunkMTime(const std::string &chunk) {
	Core::Time t;
	fs::path canonicalPath;
	try {
		// resolve softlinks
		canonicalPath = fs::canonical(_basePath / SC_FS_PATH(chunk));
	}
	catch ( ... ) {
		SEISCOMP_WARNING("Could not resolve canonical path of file: %s",
		                 chunk.c_str());
		return t;
	}

	try {
		std::time_t mtime = boost::filesystem::last_write_time(canonicalPath);
		if ( mtime >= 0 ) {
			t = mtime;
		}
		else {
			SEISCOMP_WARNING("Could not read mtime of file: %s", chunk.c_str());
		}
	}
	catch ( ... ) {
		SEISCOMP_WARNING("Could not read mtime of file: %s", chunk.c_str());
		return t;
	}

	return t;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Collector::RecordIterator*
SDSCollector::begin(const std::string &chunk,
                    const DataModel::WaveformStreamID &wid) {
	return new RecordIterator((_basePath / SC_FS_PATH(chunk)).string(), wid);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool SDSCollector::threadSafe() const {
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void SDSCollector::scanDirectory(WaveformIDs &wids, const fs::path &dir,
                                 uint16_t depth) {

	// If depth is 0 we reached the directory level on which stream files are
	// expected
	if ( depth == 0 ) {
		scanFiles(wids, dir);
		return;
	}

	const fs::directory_iterator end_itr;
	try {
		--depth;
		for ( fs::directory_iterator itr(dir);
		      itr != end_itr && !_abortRequested; ++itr ) {
			fs::path path(SC_FS_DE_PATH(itr));
			if ( fs::is_directory(path) ) {
				scanDirectory(wids, path, depth);
			}
		}
	}
	catch ( ... ) {}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void SDSCollector::scanFiles(WaveformIDs &wids, const fs::path &dir) {
	fs::directory_iterator end_itr;
	try {
		for ( fs::directory_iterator itr(dir);
		      itr != end_itr && !_abortRequested; ++itr ) {

			fs::path path = SC_FS_DE_PATH(itr);
			if ( !SC_FS_IS_REGULAR_FILE(path) ) {
				continue;
			}

			std::string name = SC_FS_FILE_NAME(path);
			auto idDate = fileStreamID(name);
			if ( idDate.streamID.empty() ||
			     wids.find(idDate.streamID) != wids.end() ||
			     !checkTimeWindow(idDate.year, idDate.doy) ) {
				continue;
			}

			DataModel::WaveformStreamID wfid;
			if ( !wfID(wfid, idDate.streamID) ) {
				SEISCOMP_WARNING("Invalid file name: %s",
				                 SC_FS_FILE_PATH(path).c_str());
				continue;
			}
			wids[idDate.streamID] = wfid;
		}
	}
	catch ( ... ) {}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
SDSCollector::IDDate SDSCollector::fileStreamID(const std::string &filename) {
	// NET.STA.LOC.CHA.D.YEAR.DAY
	size_t pos = -1;
	for ( int i = 0; i < 4; ++i ) {
		pos = filename.find('.', pos+1);
		if ( pos == string::npos ) {
			return {};
		}
	}

	if ( filename.length() - pos != 11 ||
	     filename[pos+1] != 'D' ||
	     filename[pos+2] != '.' ||
	     filename[pos+7] != '.' ) {
		return {};
	}

	IDDate idDate;
	if ( !Core::fromString(idDate.year, filename.substr(pos+3, 4)) ||
	     !Core::fromString(idDate.doy, filename.substr(pos+8)) ) {
		return {};
	}

	--idDate.doy;
	idDate.streamID = filename.substr(0, pos);

	return idDate;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool SDSCollector::checkTimeWindow(int year, int doy) {
	return ( !_startYear || (
	           year > *_startYear || (year == *_startYear && doy >= *_startDOY))
	       ) &&
	       ( !_endTime || (
	           year < *_endYear || (year == *_endYear && doy <= *_endDOY))
	       );
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
} // ns DataAvailability
} // ns Seiscomp

