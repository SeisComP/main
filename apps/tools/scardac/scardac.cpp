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


#define SEISCOMP_COMPONENT SCARDAC

#include "scardac.h"

#include <seiscomp/plugins/dataavailability/utils.hpp>

#include <seiscomp/datamodel/inventory.h>
#include <seiscomp/datamodel/network.h>
#include <seiscomp/datamodel/sensorlocation.h>
#include <seiscomp/datamodel/station.h>
#include <seiscomp/datamodel/stream.h>

#include <seiscomp/io/database.h>

#include <seiscomp/logging/log.h>

#include <ctime>
#include <functional>
#include <vector>

#define _T(name) _db->driver()->convertColumnName(name)

using namespace std;
using namespace Seiscomp::DataModel;

namespace Seiscomp {
namespace DataAvailability {

enum { MAX_THREADS = 1000 };

namespace {

inline
bool compareSegmentStart(const DataSegmentPtr &a, const DataSegmentPtr &b) {
	return a->start() < b->start();
}

bool readWFIDFile(Collector::WaveformIDs &wids, const string &fileName) {
	SEISCOMP_INFO("Reading waveform stream IDs from %s",
	              fileName.c_str());
	ifstream file(fileName);
	if ( !file.is_open() ) {
		SEISCOMP_ERROR("Unable to open waveform stream ID file %s",
		               fileName.c_str());
		return false;
	}

	string line;
	int lineNum = 0;
	WaveformStreamID wid;
	while ( getline(file, line) ) {
		++lineNum;
		line.erase(std::remove_if(line.begin(), line.end(), ::isspace),
		           line.end());
		if ( line.empty() || line[0] == '#' ) {
			continue;
		}

		if ( !wfID(wid, line) ) {
			SEISCOMP_ERROR("Invalid line #%i in waveform stream ID "
			               "file %s",
			               lineNum, fileName.c_str());
			return false;
		}

		if ( wids.find(line) != wids.end() ) {
			SEISCOMP_DEBUG("Ignoring duplicated line #i in waveform "
			               "stream ID file", lineNum);
			continue;
		}

		wids[line] = wid;
	}

	file.close();

	return true;
}

inline
bool parseDate(OPT(Core::Time) &time, const string &timeStr, const char *desc) {
	double daysAgo;
	if ( Core::fromString(daysAgo, timeStr) ) {
		time = Core::Time::UTC() - Core::TimeSpan(daysAgo * 86400.0);
	}
	else {
		time = Core::Time();
		if ( !Core::fromString(*time, timeStr) ) {
			SEISCOMP_ERROR("Invalid %s time value, expected time span in days "
			               "(float) before now or date "
			               "(YYYY-mm-dd['T'HH:MM:SS])", desc);
			return false;
		}
	}

	return true;
}

} // ns anonymous
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Worker::Worker(const SCARDAC *app, int id, Collector *collector)
: _app(app), _id(id), _collector(collector) {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Worker::processExtent(DataExtent *extent, bool foundInDB) {
	if ( !extent ) {
		return;
	}

	// reset processExtent variables
	_extent = extent;
	_extentOID = IO::DatabaseInterface::INVALID_OID;
	_sid = streamID(extent->waveformID());
	_segmentOverflow = false;
	_segCount = 0;
	_segmentsStore.clear();
	_segmentsRemove.clear();

	SEISCOMP_INFO("[%i] %s: Start processing", _id, _sid.c_str());
	Collector::DataChunks chunks;
	_collector->collectChunks(chunks, _extent->waveformID());

	if ( chunks.empty() ) {
		SEISCOMP_INFO("[%i] %s: Found no data chunks", _id, _sid.c_str());
	}
	else {
		SEISCOMP_INFO("[%i] %s: Found %zu data chunks", _id, _sid.c_str(),
		              chunks.size());
		SEISCOMP_DEBUG("[%i] %s: First: %s", _id, _sid.c_str(),
		               chunks.front().c_str());
		SEISCOMP_DEBUG("[%i] %s: Last : %s", _id, _sid.c_str(),
		               chunks.back().c_str());
	}

	// database segment iterator, limited by scan window (if any)
	DatabaseIterator db_seg_it;

	// check if extent exists
	if ( foundInDB ) {
		// query existing segments
		if ( !dbConnect() ) {
			SEISCOMP_ERROR("[%i] %s: Could not query existing attribute "
			               "extents and data segments", _id, _sid.c_str());
			return;
		}

		// load existing data attribute extents
		_db->loadDataAttributeExtents(_extent);

		// query existing segments within requested time window, count segments
		// outside time window
		_extentOID = _db->getCachedId(_extent);
		if ( _extentOID != IO::DatabaseInterface::INVALID_OID ) {
			db_seg_it = dbSegments(_segCount);
		}
		SEISCOMP_DEBUG("[%i] %s: Found existing extent\n"
		               "  start            : %s\n"
		               "  end              : %s\n"
		               "  updated          : %s\n"
		               "  last scan        : %s\n"
		               "  attribute extents: %zu\n"
		               "  segment overflow : %i",
		               _id, _sid.c_str(), extent->start().iso(),
		               extent->end().iso(), extent->updated().iso(),
		               extent->lastScan().iso(),
		               extent->dataAttributeExtentCount(),
		               extent->segmentOverflow());
	}
	else if ( writeExtent(OP_ADD) ) {
		_extentOID = _db->getCachedId(_extent);
	}
	else {
		SEISCOMP_ERROR("[%i] %s: Could not create new extent",
		               _id, _sid.c_str());
		return;
	}

	if ( _extentOID == IO::DatabaseInterface::INVALID_OID ) {
		SEISCOMP_ERROR("[%i] %s: Could not read extent _oid", _id, _sid.c_str());
		return;
	}

	auto now = Core::Time::UTC();
	auto mtime = now;
	Segments segments;
	Core::TimeWindow chunkWindow;
	Core::TimeWindow nextChunkWindow;
	OPT(Core::Time) chunkSeqEndTime;
	OPT(Core::Time) prevChunkEndTime;
	DataSegmentPtr dbSeg = nullptr;
	DataSegmentPtr prevChunkSeg = nullptr;
	string readTrigger;

	if ( _app->_startTime ) {
		prevChunkEndTime = *_app->_startTime;
	}

	// iterate over all stream chunks
	for ( auto chunk = chunks.cbegin(), nextChunk = chunks.cbegin();
	      chunk != chunks.cend(); ++chunk ) {
		if ( _app->_exitRequested || _segmentOverflow ) {
			break;
		}

		nextChunkWindow = {};
		readTrigger = !foundInDB ? "new extent" :
		              _app->_deepScan ? "deep-scan" : "";
		nextChunk = next(chunk);

		if ( nextChunk != chunks.end() ) {
			if ( !_collector->chunkTimeWindow(nextChunkWindow, *nextChunk) ) {
				nextChunkWindow = {};
			}
		}

		// update previous chunk end time
		if ( chunkWindow ) {
			prevChunkEndTime = chunkWindow.endTime();
		}

		// request time window of current chunk
		if ( !_collector->chunkTimeWindow(chunkWindow, *chunk) ) {
			SEISCOMP_WARNING("[%i] %s: Invalid chunk time window, skipping: %s",
			                 _id, _sid.c_str(), chunk->c_str());
			chunkWindow = {};
			continue;
		}

		// request modification time of current chunk
		mtime = _collector->chunkMTime(*chunk);
		if ( !mtime ) {
			mtime = now;
		}

		if ( readTrigger.empty() && (
		         !_app->_mtimeEnd || mtime <= _app->_mtimeEnd) ) {
			if ( _app->_mtimeStart && mtime >= _app->_mtimeStart ) {
				readTrigger = "mtime > modified since";
			}
			else if ( mtime > _extent->lastScan() ) {
				readTrigger = "mtime > last scan";
			}
		}

		// first chunk and no start time given or chunk gap
		// - remove all DB segments starting before current chunk
		// - force scan if segments overlapping chunk start time were found
		if ( !prevChunkEndTime || chunkWindow.startTime() > prevChunkEndTime) {
			if ( prevChunkEndTime) {
				SEISCOMP_DEBUG("[%i] %s: Detected chunk gap: %s ~ %s",
				               _id, _sid.c_str(), prevChunkEndTime->iso().c_str(),
				               chunkWindow.startTime().iso().c_str());
				// trigger read if previous DB segment was overlapping
				// chunkWindow startTime
				if ( readTrigger.empty() && dbSeg &&
				     dbSeg->end() > chunkWindow.startTime() ) {
					readTrigger = "new chunk gap";
				}
			}

			for ( ; !_app->_exitRequested && *db_seg_it; ++db_seg_it ) {
				dbSeg = DataSegment::Cast(*db_seg_it);
				if ( dbSeg->start() >= chunkWindow.startTime() ) {
					break;
				}

				if ( dbSeg->end() > chunkWindow.startTime() &&
				     readTrigger.empty()) {
					readTrigger = "db segment overlapping chunk start time";
				}
				SEISCOMP_DEBUG("[%i] %s: Remove DB segment [%s~%s] "
				               "(overlapping chunk start time)", _id,
				               _sid.c_str(), dbSeg->start().iso(),
				               dbSeg->end().iso());
				_segmentsRemove.push_back(dbSeg);
			}
		}
		else if ( readTrigger.empty() && nextChunkWindow &&
		          chunkWindow.endTime() != nextChunkWindow.startTime() ) {
			readTrigger = "chunk gap ahead";
		}

		// chunk unchanged and scan not forced otherwise
		if ( readTrigger.empty() ) {
			SEISCOMP_DEBUG("[%i] %s: Skipping chunk (mtime of %s %s): %s",
			               _id, _sid.c_str(), mtime.iso().c_str(),
			               _app->_mtimeEnd && mtime > _app->_mtimeEnd ?
			                   "> mtime window" :
			               _app->_mtimeStart ? "< mtime window" : "< last scan",
			               chunk->c_str());

			// previous chunk was read and last segment was not committed yet
			if ( prevChunkSeg ) {
				diffSegment(db_seg_it, prevChunkSeg.get(), true);
				prevChunkSeg = nullptr;
			}

			// advance database segment iterator to current chunk's end time
			for ( ; !_app->_exitRequested && *db_seg_it; ++db_seg_it ) {
				dbSeg = DataSegment::Cast(*db_seg_it);

				if ( dbSeg->end() >= chunkWindow.endTime() ) {
					break;
				}

				++_segCount;
			}

			continue;
		}

		// read segments from chunk
		SEISCOMP_DEBUG("[%i] %s: Reading chunk (%s): %s",
		               _id, _sid.c_str(), readTrigger.c_str(),
		               chunk->c_str());
		if ( !readChunkSegments(segments, *chunk, prevChunkSeg, mtime,
		                        chunkWindow) ) {
			// TODO: Truncate DB segment to window start time
			continue;
		}

		// process chunk segments
		for ( auto it = segments.cbegin(), last = --segments.cend();
		      it != segments.cend() && !_app->_exitRequested; ++it ) {

			// check segment overflow
			if ( _app->_maxSegments && !_segmentOverflow &&
			     _segCount > _app->_maxSegments ) {
				_segmentOverflow = true;
				SEISCOMP_WARNING("[%i] %s: Segment overflow detected, new "
				                 "segments will no longer be added to the "
				                 "database", _id, _sid.c_str());
				break;
			}

			auto seg = *it;

			if ( _app->_endTime && seg->start() >= _app->_endTime ) {
				SEISCOMP_DEBUG("[%i] %s: Abort scan, chunk segment [%s~%s] "
				               "behind end time", _id, _sid.c_str(),
				               seg->start().iso(), seg->end().iso());
				break;
			}

			if ( _app->_startTime && seg->end() < _app->_startTime ) {
				SEISCOMP_DEBUG("[%i] %s: Skipping chunk segment [%s~%s] "
				               "before start time", _id, _sid.c_str(),
				               seg->start().iso(), seg->end().iso());
				continue;
			}

			// first segment of current chunk and previous chunk not read:
			// check if chunk segment can be joined with DB segment
			if ( !prevChunkSeg && *db_seg_it ) {
				//double jitter = _app->_jitter / seg->sampleRate();
				auto segStartJitter = seg->start();// + Core::TimeSpan(jitter);
				dbSeg = DataSegment::Cast(*db_seg_it);
				if ( dbSeg->end() < segStartJitter ) {
					++db_seg_it;
					++_segCount;
				}
				else if ( dbSeg->start() < segStartJitter &&
				          dbSeg->sampleRate() == seg->sampleRate() &&
				          dbSeg->quality()    == seg->quality() ) {
					SEISCOMP_DEBUG("[%i] %s: Join DB segment [%s~%s] with "
					               "first chunk segment [%s~%s]", _id,
					               _sid.c_str(),
					               dbSeg->start().iso(), dbSeg->end().iso(),
					               seg->start().iso(), seg->end().iso());
					seg->setStart(dbSeg->start());
					if ( dbSeg->end() > seg->end() ) {
						seg->setEnd(dbSeg->end());
					}
					if ( dbSeg->updated() > seg->updated() ) {
						seg->setUpdated(dbSeg->updated());
					}

					_segmentsRemove.push_back(dbSeg);
					++db_seg_it;
				}
			}
			prevChunkSeg = nullptr;

			// skip last segment if it might be extended by records of the next
			// data chunk
			if ( it == last && nextChunkWindow ) {
				// commit last segment if it ends before next chunk window
				double jitter = _app->_jitter / seg->sampleRate();
				if ( (nextChunkWindow.startTime() - seg->end()).length() < jitter ) {
					prevChunkSeg = seg;
					break;
				}
			}

			dbSeg = DataSegment::Cast(*db_seg_it);

			// diff database segments up to current chunk segment
			diffSegment(db_seg_it, it->get());
		}
	}

	// remove trailing database segments
	for ( ; !_app->_exitRequested && *db_seg_it; ++db_seg_it ) {
		dbSeg = DataSegment::Cast(*db_seg_it);

		SEISCOMP_DEBUG("[%i] %s: Remove DB segment [%s~%s] (trailing last "
		               "chunk)", _id, _sid.c_str(), dbSeg->start().iso(),
		               dbSeg->end().iso());
		_segmentsRemove.push_back(dbSeg);
	}

	db_seg_it.close();

	if ( syncSegments() ) {
		// update extent's last scan time
		_extent->setLastScan(now);
		syncExtent();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Worker::dbConnect() {
	while ( !_app->_exitRequested &&
	        ( !_db || !_db->driver()->isConnected() ) ) {
		SEISCOMP_DEBUG("[%i] Initializing database connection", _id);
		IO::DatabaseInterfacePtr dbInterface =
		        IO::DatabaseInterface::Open(_app->databaseURI().c_str());
		if ( dbInterface ) {
			_db = new DatabaseReader(dbInterface.get());
		}
		else {
			SEISCOMP_DEBUG("[%i] Trying to reconnect in 5s", _id);
			for ( int i = 0; i < 5 && !_app->_exitRequested; ++i ) {
				sleep(1);
			}
		}
	}

	if ( _db && _db->driver()->isConnected() ) {
		return true;
	}

	SEISCOMP_ERROR("[%i] Could not initialize database connection", _id);
	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
DatabaseIterator Worker::dbSegments(size_t &segmentsOutside) {
	std::ostringstream oss;
	segmentsOutside = 0;
	auto *db = _db->driver();

	// if the data collection is restricted by time, then query the number of
	// segments lying outside the requested time window
	if ( _app->_startTime || _app->_endTime ) {
		oss << "SELECT COUNT(*) "
		       "FROM DataSegment "
		       "WHERE _parent_oid = " << _extentOID;
		if ( _app->_startTime ) {
			oss << " AND " << _T("end") << " <= '" <<
			       db->timeToString(*_app->_startTime) << "'";
		}
		if ( _app->_endTime ) {
			oss << " AND " << _T("start") << " > '" <<
			      db->timeToString(*_app->_endTime) << "'";
		}

		if ( !db->beginQuery(oss.str().c_str()) ||
		     !db->fetchRow() ||
		     !Seiscomp::Core::fromString(segmentsOutside,
		                                 static_cast<const char*>(
		                                         db->getRowField(0))) ) {
			SEISCOMP_WARNING("[%i] %s: Could not query number of segments "
			                 "outside time window", _id, _sid.c_str());
		}
		db->endQuery();

		oss.str("");
	}

	oss << "SELECT * "
	       "FROM DataSegment "
	       "WHERE _parent_oid = " << _extentOID;
	if ( _app->_startTime ) {
		oss << " AND " << _T("end") << " > '" <<
		       _db->driver()->timeToString(*_app->_startTime) << "'";
	}
	if ( _app->_endTime ) {
		oss << " AND " << _T("start") << " <= '" <<
		       _db->driver()->timeToString(*_app->_endTime) << "'";
	}
	oss << " ORDER BY " << _T("start") << " ASC, "
	                    << _T("start_ms") << " ASC";

	return _db->getObjectIterator(oss.str(), DataSegment::TypeInfo());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Worker::readChunkSegments(Segments &segments, const std::string &chunk,
                               DataModel::DataSegmentPtr chunkSeg,
                               const Core::Time &mtime,
                               const Core::TimeWindow &window) {
	segments.clear();

	Collector::RecordIteratorPtr rec;
	try {
		rec = _collector->begin(chunk, _extent->waveformID());
	} catch ( CollectorException &e) {
		SEISCOMP_WARNING("[%i] %s: %s: %s",
		                 _id, _sid.c_str(), chunk.c_str(), e.what());
		return false;
	}

	DataModel::DataSegmentPtr segment = chunkSeg;
	double jitter = segment ? _app->_jitter / segment->sampleRate() : 0;

	uint32_t records         = 0;
	uint32_t gaps            = 0;
	uint32_t overlaps        = 0;
	uint32_t outOfOrder      = 0;
	uint32_t rateChanges     = 0;
	uint32_t qualityChanges  = 0;
	double availability      = 0; // in seconds

	while ( rec->next() && !_app->_exitRequested ) {
		// assert valid sampling rate
		if ( rec->sampleRate() <= 0 ) {
			++records;
			SEISCOMP_WARNING("[%i] %s: Invalid sampling rate in record #%i",
			                 _id, _sid.c_str(), records);
			continue;
		}

		// check if record can be merged with current segment
		auto merge = false;
		if ( segment ) {
			// gap
			if ( (rec->startTime() - segment->end()).length() > jitter ) {
				++gaps;
				SEISCOMP_DEBUG("[%i] %s: Detected gap: %s ~ %s (%.1fs)",
				               _id, _sid.c_str(),
				               segment->end().iso().c_str(),
				               rec->startTime().iso().c_str(),
				               (rec->startTime() - segment->end()).length());
			}
			// overlap
			else if ( (segment->end() - rec->startTime()).length() > jitter ) {
				++overlaps;
				SEISCOMP_DEBUG("[%i] %s: Detected overlap: %s ~ %s (%.1fs)",
				               _id, _sid.c_str(), rec->startTime().iso().c_str(),
				               segment->end().iso().c_str(),
				               (segment->end() - rec->startTime()).length());
			}
			else {
				merge = true;
			}

			// sampling rate change
			if ( segment->sampleRate() != rec->sampleRate() ) {
				++rateChanges;
				SEISCOMP_DEBUG("[%i] %s: Detected change of sampling rate at "
				               "%s: %.1f -> %.1f", _id, _sid.c_str(),
				               rec->startTime().iso().c_str(),
				               segment->sampleRate(), rec->sampleRate());
				merge = false;
			}

			// quality change
			if ( segment->quality() != rec->quality() ) {
				++qualityChanges;
				SEISCOMP_DEBUG("[%i] %s: Detected change of quality at %s "
				               "%s -> %s", _id, _sid.c_str(),
				               rec->startTime().iso().c_str(),
				               segment->quality().c_str(),
				               rec->quality().c_str());
				merge = false;
			}
		}

		if ( merge ) {
			// first record of current chunk is merged with segment of previous
			// chunk
			if ( records == 0 && mtime > segment->updated() ) {
				segment->setUpdated(mtime);
			}
			segment->setEnd(rec->endTime());
		}
		else {
			auto ooo = false;
			if ( segment ) {
				segments.emplace_back(segment.get());
				if ( rec->startTime() < segment->start() ) {
					ooo = true;
					++outOfOrder;
				}
			}
			segment = new DataSegment();
			segment->setStart(rec->startTime());
			segment->setEnd(rec->endTime());
			segment->setUpdated(mtime);
			segment->setSampleRate(rec->sampleRate());
			segment->setQuality(rec->quality());
			segment->setOutOfOrder(ooo);
			jitter = _app->_jitter / rec->sampleRate();
		}

		++records;
		availability += (rec->endTime() - rec->startTime()).length();
//		SEISCOMP_DEBUG("%s - %s (%.3fs)", it->startTime().iso().c_str(),
//		               it->endTime().iso().c_str(),
//		               (it->endTime() - it->startTime()).length());
	}

	if ( !records ) {
		SEISCOMP_WARNING("[%i] %s: Found no data in chunk: %s ", _id,
		                 _sid.c_str(), chunk.c_str());
		return false;
	}

	segments.emplace_back(segment.get());

	// sort segment vector according start time if out of order data
	// was detected
	if ( outOfOrder > 0 ) {
		sort(segments.begin(), segments.end(), compareSegmentStart);
	}

	// check segments for duplicated start time, keep segment with largest
	// time window
	uint32_t dropped = 0;
	auto seg = segments.begin();
	auto last = seg++;
	while ( seg != segments.end() ) {
		if ( (*seg)->start() != (*last)->start() ) {
			++seg;
			++last;
			continue;
		}

		SEISCOMP_DEBUG("[%i] %s: Dropping segment with duplicated start "
		               "time: %s", _id, _sid.c_str(),
		               (*seg)->start().iso().c_str());
		segments.erase((*seg)->end() > (*last)->end() ? last : seg );
		++dropped;
	}

	SEISCOMP_DEBUG("[%i] %s: %s, Results:\n"
	               "  time window          : %s ~ %s (%.1fs)\n"
	               "  modification time    : %s\n"
	               "  segments             : %zu\n"
	               "  gaps                 : %i\n"
	               "  overlaps             : %i\n"
	               "  out of order segments: %i\n"
	               "  dropped segments     : %i\n"
	               "  sampling rate changes: %i\n"
	               "  quality changes      : %i\n"
	               "  records              : %i\n"
	               "  availability         : %.2f%% (%.1fs)",
	               _id, _sid.c_str(), chunk.c_str(),
	               window.startTime().iso().c_str(),
	               window.endTime().iso().c_str(), window.length(),
	               mtime.iso().c_str(), segments.size(), gaps, overlaps,
	               outOfOrder, dropped, rateChanges, qualityChanges, records,
	               availability/window.length()*100.0, availability);

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Worker::diffSegment(DatabaseIterator &db_seg_it, DataSegment *chunkSeg,
                         bool extent) {
	for ( ; !_app->_exitRequested && *db_seg_it; ++db_seg_it ) {
		DataSegmentPtr dbSeg = DataSegment::Cast(*db_seg_it);

		// new segment
		if ( dbSeg->start() > chunkSeg->start() ) {
			++_segCount;

			// check if chunk segment can be merged into database segment
			if ( extent &&
			     dbSeg->sampleRate() == chunkSeg->sampleRate() &&
			     dbSeg->quality() == chunkSeg->quality() ) {
				double jitter = _app->_jitter / chunkSeg->sampleRate();
				if ( abs((dbSeg->start() - chunkSeg->end()).length()) < jitter ) {
					SEISCOMP_DEBUG("[%i] %s: Join chunk segment [%s~%s] with "
					               "DB segment [%s~%s]", _id, _sid.c_str(),
					               chunkSeg->start().iso(),
					               chunkSeg->end().iso(), dbSeg->start().iso(),
					               dbSeg->end().iso());
					chunkSeg->setEnd(dbSeg->end());
					if ( dbSeg->updated() > chunkSeg->updated() ) {
						chunkSeg->setUpdated(chunkSeg->updated());
					}
					if ( dbSeg->outOfOrder() ) {
						chunkSeg->setOutOfOrder(true);
					}

					_segmentsRemove.push_back(dbSeg);
					_segmentsStore.emplace_back(chunkSeg);
					++_segCount;
					++db_seg_it;
					return;
				}
			}

			SEISCOMP_DEBUG("[%i] %s: Add chunk segment [%s~%s] (ahead of DB "
			               "segment)", _id, _sid.c_str(),
			               chunkSeg->start().iso(), chunkSeg->end().iso());
			_segmentsStore.emplace_back(chunkSeg);
			return;
		}

		// remove database segment
		if ( dbSeg->start() < chunkSeg->start() ) {
			SEISCOMP_DEBUG("[%i] %s: Remove DB segment [%s~%s] (ahead of "
			               "chunk segment)", _id, _sid.c_str(),
			               dbSeg->start().iso(), dbSeg->end().iso());
			_segmentsRemove.push_back(dbSeg);
			continue;
		}

		// same start time: update if modified
		if ( dbSeg->sampleRate() != chunkSeg->sampleRate() ||
		     dbSeg->quality() != chunkSeg->quality() ||
		     dbSeg->outOfOrder() != chunkSeg->outOfOrder() ||
		     dbSeg->updated() != chunkSeg->updated() ||
		     dbSeg->end() != chunkSeg->end() ) {
			SEISCOMP_DEBUG("[%i] %s: Replace DB segment [%s~%s] with "
			               "chunk segment [%s~%s] (same start time)",
			               _id, _sid.c_str(),
			               dbSeg->start().iso(), dbSeg->end().iso(),
			               chunkSeg->start().iso(), chunkSeg->end().iso());
			if ( extent ) {
				if ( dbSeg->end() > chunkSeg->end() ) {
					chunkSeg->setEnd(dbSeg->end());
				}
				if ( dbSeg->updated() > chunkSeg->updated() ) {
					chunkSeg->setUpdated(dbSeg->updated());
				}
				if ( dbSeg->outOfOrder() ) {
					chunkSeg->setOutOfOrder(true);
				}
			}
			chunkSeg->setParent(_extent);
			_segmentsStore.emplace_back(chunkSeg);
		}
		++_segCount;
		++db_seg_it;
		return;
	}

	if ( !*db_seg_it ) {
		SEISCOMP_DEBUG("[%i] %s: Add chunk segment [%s~%s] (no DB segment left)",
		               _id, _sid.c_str(), chunkSeg->start().iso(),
		               chunkSeg->end().iso());
		++_segCount;
		_segmentsStore.emplace_back(chunkSeg);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Worker::writeExtent(Operation op) {
	if ( op == OP_UNDEFINED || !dbConnect() ) {
		return false;
	}

	if ( ( op == OP_ADD    && !_db->insert(_extent)  ) ||
	     ( op == OP_UPDATE && !_db->update(_extent) ) ||
	     ( op == OP_REMOVE && !_db->remove(_extent) ) ) {
		SEISCOMP_ERROR("[%i] %s: Could not %s extent: %s", _id, _sid.c_str(),
		               op.toString(), _extent->publicID().c_str());
		return false;
	}

	SEISCOMP_DEBUG("[%i] %s: %s extent: %s", _id, _sid.c_str(),
	               string(op == OP_ADD   ?"Added":
	                                      op == OP_UPDATE?"Updated":"Removed").c_str(),
	               _extent->publicID().c_str());
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Worker::syncSegments() {
	SEISCOMP_INFO("[%i] %s: Synchronizing segments with database (store/"
	              "remove): %zu/%zu", _id, _sid.c_str(), _segmentsStore.size(),
	              _segmentsRemove.size());

	if ( _app->_exitRequested || !dbConnect() || !_extent ) {
		return false;
	}

	// remove
	for ( auto &segment : _segmentsRemove ) {
		if ( _app->_exitRequested ) {
			return false;
		}

		segment->setParent(_extent);
		if ( _db->remove(segment.get()) ) {
			SEISCOMP_DEBUG("[%i] %s: Removed segment [%s~%s]",
			               _id, _sid.c_str(), segment->start().iso(),
			               segment->end().iso());
		}
		else {
			SEISCOMP_ERROR("[%i] %s: Failed to remove segment [%s~%s]",
			               _id, _sid.c_str(), segment->start().iso(),
			               segment->end().iso());
		}
	}

	// add/update
	for ( auto &segment : _segmentsStore ) {
		if ( _app->_exitRequested ) {
			return false;
		}

		if ( segment->parent() ) {
			if ( _db->update(segment.get()) ) {
				SEISCOMP_DEBUG("[%i] %s: Updated segment [%s~%s]",
				               _id, _sid.c_str(), segment->start().iso(),
				               segment->end().iso());
			}
			else {
				SEISCOMP_ERROR("[%i] %s: Failed to update segment [%s~%s]",
				               _id, _sid.c_str(), segment->start().iso(),
				               segment->end().iso());
			}
		}
		else {
			segment->setParent(_extent);
			if ( _db->insert(segment.get()) ) {
				SEISCOMP_DEBUG("[%i] %s: Added segment [%s~%s]",
				               _id, _sid.c_str(), segment->start().iso(),
				               segment->end().iso());
			}
			else {
				SEISCOMP_ERROR("[%i] %s: Failed to add segment [%s~%s]",
				               _id, _sid.c_str(), segment->start().iso(),
				               segment->end().iso());
			}
		}
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Worker::syncExtent() {
	if ( _app->_exitRequested || !dbConnect() ) {
		SEISCOMP_ERROR("[%i] %s:Could not synchronize extent and attribute "
		               "extents with segments", _id, _sid.c_str());
		return;
	}

	// temporary extent
	DataExtent tmpExt("tmp_" + _sid);
	tmpExt.setLastScan(_extent->lastScan());
	tmpExt.setWaveformID(_extent->waveformID());
	tmpExt.setSegmentOverflow(_segmentOverflow);

	// request attribute extents based on segments found in database,
	// all time values lack millisecond precision because there is no efficient
	// aggregate function which combines two columns
	stringstream oss;
	oss << "SELECT MIN(" << _T("start") << ") AS " << _T("start") << ", "
	              "MAX(" << _T("end") << ") AS " << _T("end") << ", " <<
	              _T("sampleRate") << ", " <<
	              _T("quality") << ", " <<
	              "MAX(" << _T("updated") << ") AS " << _T("updated") << ", "
	              "COUNT(*) AS " << _T("segmentCount") << " "
	       "FROM DataSegment "
	       "WHERE _parent_oid = " << _extentOID << " "
	       "GROUP BY " << _T("sampleRate") << ", " << _T("quality");

	for ( auto it = _db->getObjectIterator(
	          oss.str(), DataAttributeExtent::TypeInfo()); *it; ++it ) {
		tmpExt.add(DataAttributeExtent::Cast(*it));
	}

	size_t attSegCount = 0;
	for ( size_t i = 0; i < tmpExt.dataAttributeExtentCount(); ++i ) {
		auto *attExt = tmpExt.dataAttributeExtent(i);

		readAttExtMillis(attExt);

		if ( !i || attExt->start() < tmpExt.start() ) {
			tmpExt.setStart(attExt->start());
		}
		if ( !i || attExt->end() > tmpExt.end() ) {
			tmpExt.setEnd(attExt->end());
		}
		if ( !i || attExt->updated() > tmpExt.updated() ) {
			tmpExt.setUpdated(attExt->updated());
		}

		attSegCount += attExt->segmentCount();
	}

	if ( _segCount != attSegCount ) {
		SEISCOMP_WARNING("[%i] %s: Attribute extent segment counter differs "
		                 "from segments counted in current scan [%zu != %zu]",
		                 _id, _sid.c_str(), attSegCount, _segCount);
	}

	// remove or update existing attribute extents
	for ( size_t i = 0; i < _extent->dataAttributeExtentCount(); ) {
		auto *attExt = _extent->dataAttributeExtent(i);
		auto *tmpAttExt = tmpExt.dataAttributeExtent(attExt->index());

		// remove if no corresponding attribute extent was found
		if ( !tmpAttExt ) {
			SEISCOMP_DEBUG("[%i] %s: Removing attribute extent with index "
			               "%f,%s", _id, _sid.c_str(), attExt->sampleRate(),
			               attExt->quality().c_str());
			_db->remove(attExt);
			_extent->removeDataAttributeExtent(i);
			continue;
		}

		// update
		if ( *attExt != *tmpAttExt ) {
			*attExt = *tmpAttExt;
			SEISCOMP_DEBUG("[%i] %s: Updating attribute extent with index "
			               "%f,%s", _id, _sid.c_str(), attExt->sampleRate(),
			               attExt->quality().c_str());
			_db->update(attExt);
		}

		++i;
	}

	// add new attribute extents
	for ( size_t i = 0; i < tmpExt.dataAttributeExtentCount(); ++i ) {
		auto *tmpAttExt = tmpExt.dataAttributeExtent(i);
		auto *attExt = _extent->dataAttributeExtent(tmpAttExt->index());
		if ( !attExt ) {
			attExt = new DataAttributeExtent(*tmpAttExt);
			SEISCOMP_DEBUG("[%i] %s: Adding attribute extent with index %f,%s",
			                _id, _sid.c_str(), attExt->sampleRate(),
			               attExt->quality().c_str());
			_extent->add(attExt);
			_db->insert(attExt);
		}
	}

	// update extent if at least one attribute extent was found
	if ( tmpExt.dataAttributeExtentCount() ) {
		auto modStr = string(*_extent == tmpExt ? "unmodified" : "modified");
		*_extent = tmpExt;

		if ( writeExtent(OP_UPDATE) ) {
			SEISCOMP_INFO("[%i] %s: Extent %s: %s ~ %s", _id, _sid.c_str(),
			              modStr.c_str(),
			              _extent->start().iso().c_str(),
			              _extent->end().iso().c_str());
		}
	}
	else {
		// no segments found, remove entire extent
		if ( writeExtent(OP_REMOVE) ) {
			SEISCOMP_INFO("[%i] %s: Extent removed", _id, _sid.c_str());
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Worker::readAttExtMillis(DataAttributeExtent *attExt) {
	auto *driver = _db->driver();

	// query start time milliseconds
	auto time = attExt->start();
	stringstream oss;
	oss << "SELECT " << _T("start_ms") << " "
	       "FROM DataSegment "
	       "WHERE _parent_oid = " << _extentOID << " AND "
	    << _T("start") << " = '"
	    << driver->timeToString(time) << "' "
	       "ORDER BY " << _T("start_ms") << " ASC LIMIT 1";

	if ( !driver->beginQuery(oss.str().c_str()) ) {
		SEISCOMP_ERROR("Query for attribute extent start milliseconds failed: "
		               "%s", oss.str().c_str());
		return;
	}

	if ( driver->fetchRow() && driver->getRowFieldCount() == 1 ) {
		time.setUSecs(atoi(static_cast<const char*>(driver->getRowField(0))));
		attExt->setStart(time);
	}

	driver->endQuery();

	// query end time milliseconds
	time = attExt->end();
	oss.str("");
	oss.clear();
	oss << "SELECT " << _T("end_ms") << " "
	       "FROM DataSegment "
	       "WHERE _parent_oid = " << _extentOID << " AND "
	    << _T("end") << " = '"
	    << driver->timeToString(time) << "' "
	       "ORDER BY " << _T("end_ms") << " DESC LIMIT 1";

	if ( !driver->beginQuery(oss.str().c_str()) ) {
		SEISCOMP_ERROR("Query for attribute extent end milliseconds failed: %s",
		               oss.str().c_str());
		return;
	}

	if ( driver->fetchRow() && driver->getRowFieldCount() == 1 ) {
		time.setUSecs(atoi(static_cast<const char*>(driver->getRowField(0))));
		attExt->setEnd(time);
	}

	driver->endQuery();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
SCARDAC::SCARDAC(int argc, char **argv)
: Seiscomp::Client::Application(argc, argv) {
	setMessagingEnabled(true);
	setDatabaseEnabled(true, true);

	_workQueue.resize(MAX_THREADS);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void SCARDAC::printUsage() const {
	cout << "Usage:"  << endl << "  scardac [options]" << endl << endl
	     << "Scan waveform data archive and collect availability information "
	        "from records."
	     << endl;

	Seiscomp::Client::Application::printUsage();

	cout << "Examples:" << endl
	     << "Scan the configured archive, write information to the default "
	        "SeisComP database" << endl
	     << "  scardac -d localhost -a $SEISCOMP_ROOT/var/lib/archive"
	     << endl << endl;
	cout << "Scan a specific archive at /archive, write information to the "
	        "default SeisComP database" << endl
	     << "  scardac -d localhost -a /archive"
	     << endl << endl;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void SCARDAC::createCommandLineDescription() {
	commandline().addGroup("Collector");
	commandline().addOption("Collector", "archive,a",
	                        "Type and location of the waveform archive.",
	                        &_archive);
	commandline().addOption("Collector", "threads",
	                        "Number of threads scanning the archive in "
	                        "parallel.",
	                        &_threads);
	commandline().addOption("Collector", "jitter",
	                        "Acceptable derivation of end time and start time "
	                        "of successive records in multiples of sample "
	                        "rate.",
	                        &_jitter);
	commandline().addOption("Collector", "nslc",
	                        "Line-based text file of form NET.STA.LOC.CHA "
	                        "defining available waveform IDs. Depending on the "
	                        "archive type, size and storage media used this "
	                        "file may offer a significant performance "
	                        "improvement compared to collecting the available "
	                        "streams on each startup. Filters defined by "
	                        "'--include' or '--exclude' still apply.",
	                        &_wfidFile);
	commandline().addOption("Collector", "start",
	                        "Start of data availability check given as date "
	                        "string or as number of days before now.",
	                        &_start);
	commandline().addOption("Collector", "end",
	                        "End of data availability check given as date "
	                        "string or as number of days before now.",
	                        &_end);
	commandline().addOption("Collector", "include",
	                        "Stream IDs to process. If empty all stream IDs "
	                        "are accepted unless an exclude filter is defined. "
	                        "This option may be repeated and supports the "
	                        "wildcards '*' and '?'.",
	                        &_include);
	commandline().addOption("Collector", "exclude",
	                        "Stream IDs to exclude from processing. Excludes "
	                        "take precedence over includes. This option may be "
	                        "repeated and supports the wildcards '*' and '?'.",
	                        &_exclude);
	commandline().addOption("Collector", "deep-scan",
	                        "Process all data chunks independent of their "
	                        "modification time.");
	commandline().addOption("Collector", "modified-since",
	                        "Only read chunks modified after specific date "
	                        "given as date string or as number of days before "
	                        "now. Unused in deep-scan mode. Defaults to last "
	                        "scan time of extent.",
	                        &_modifiedSince);
	commandline().addOption("Collector", "modified-until",
	                        "Only read chunks modified before specific date "
	                        "given as date string or as number of days before "
	                        "now. Unused in deep-scan mode.",
	                        &_modifiedUntil);
	commandline().addOption("Collector", "generate-test-data",
	                        "For each stream in inventory generate test data. "
	                        "Format: days,gaps,gapseconds,overlaps,"
	                        "overlapseconds",
	                        &_testData);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool SCARDAC::initConfiguration() {
	if ( !Client::Application::initConfiguration() ) {
		return false;
	}

	try {
		_archive = SCCoreApp->configGetString("archive");
	}
	catch (...) {}

	try {
		_threads = SCCoreApp->configGetInt("threads");
	}
	catch (...) {}

	try {
		_jitter = SCCoreApp->configGetDouble("jitter");
	}
	catch (...) {}

	try {
		_wfidFile = SCCoreApp->configGetString("nslcFile");
	}
	catch (...) {}

	try {
		int maxSegments = SCCoreApp->configGetInt("maxSegments");
		if ( maxSegments < 0 ) {
			SEISCOMP_ERROR("Negative values not allowed for parameter "
			               "maxSegments");
			return false;
		}
		_maxSegments = static_cast<size_t>(maxSegments);
	}
	catch (...) {}

	try {
		_start = SCCoreApp->configGetString("filter.time.start");
	}
	catch (...) {}

	try {
		_end = SCCoreApp->configGetString("filter.time.end");
	}
	catch (...) {}

	if ( _include.empty() ) {
		try {
			_include = SCCoreApp->configGetStrings("filter.nslc.include");
		}
		catch (...) {
			try {
				_include = SCCoreApp->configGetStrings("filter.wfid.include");
			}
			catch (...) {}
		}
	}

	if ( _exclude.empty() ) {
		try {
			_exclude = SCCoreApp->configGetStrings("filter.nslc.exclude");
		}
		catch (...) {
			try {
				_exclude = SCCoreApp->configGetStrings("filter.wfid.exclude");
			}
			catch (...) {}
		}
	}

	try {
		_deepScan = SCCoreApp->commandline().hasOption("deep-scan") ||
		            SCCoreApp->configGetBool("mtime.ignore");
	}
	catch (...) {}

	try {
		_modifiedSince = SCCoreApp->configGetString("mtime.start");
	}
	catch (...) {}

	try {
		_modifiedUntil = SCCoreApp->configGetString("mtime.end");
	}
	catch (...) {}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool SCARDAC::validateParameters() {
	if ( !Client::Application::validateParameters() ) {
		return false;
	}

	// database connection configured, no need to fetch parameters
	if ( !databaseURI().empty() ) {
		setMessagingEnabled(false);
		setDatabaseEnabled(true, false);
	}

	// thread count
	if ( _threads < 1 || _threads > MAX_THREADS ) {
		SEISCOMP_ERROR("Invalid number of threads, allowed range: [1,%i]",
		               static_cast<int>(MAX_THREADS));
		return false;
	}

	// jitter samples
	if ( _jitter < 0 ) {
		SEISCOMP_ERROR("Invalid jitter value, minimum value: 0");
		return false;
	}

	// start time
	if ( !_start.empty() && !parseDate(_startTime, _start, "start") ) {
		return false;
	}

	// end time
	if ( !_end.empty() && !parseDate(_endTime, _end, "start") ) {
		return false;
	}

	// wfid filter
	_wfidFirewall.allow = { std::make_move_iterator(_include.begin()),
	                        std::make_move_iterator(_include.end()) };
	_wfidFirewall.deny = { std::make_move_iterator(_exclude.begin()),
	                       std::make_move_iterator(_exclude.end()) };

	// mtime window start
	if ( !_modifiedSince.empty() ) {
		if ( _deepScan ) {
			SEISCOMP_WARNING("Modified-since option ignored in deep-scan mode");
		}
		else if ( !parseDate(_mtimeStart, _modifiedSince, "modified-since") ) {
			return false;
		}
	}

	// mtime window end
	if ( !_modifiedUntil.empty() ) {
		if ( _deepScan ) {
			SEISCOMP_WARNING("Modified-until option ignored in deep-scan mode");
		}
		else if ( !parseDate(_mtimeEnd, _modifiedUntil, "modified-until") ) {
			return false;
		}
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool SCARDAC::run() {
	if ( !_testData.empty() ) {
		return generateTestData();
	}

	// close messaging connection as it is only needed to read the database URI
	if ( _connection && _connection->isConnected() ) {
		_connection->close();
	}

	if ( _threads > 1 && !strncmp("sqlite", databaseType().c_str(), 6) ) {
		SEISCOMP_ERROR("Thread count set to %i, but database type '%s' "
		               "supports only one simultaneous connection",
		               _threads, databaseType().c_str());
		return false;
	}

	// print configuration
	string cfgMtime;
	if ( _deepScan ) {
		cfgMtime = "         : ignored";
	}
	else {
		stringstream oss;
		oss << endl
		    << "    start       : "
		    << (_mtimeStart ? _mtimeStart->iso() : "last scan of extent")
		    << endl
		    << "    end         : " << (_mtimeEnd ? _mtimeEnd->iso() : "-");
		cfgMtime = oss.str();
	}
	string cfgInclude = "-";
	if ( !_wfidFirewall.allow.empty() ) {
		stringstream oss;
		for ( const auto &wfid : _wfidFirewall.allow ) {
			oss << endl << "      " << wfid;
		}
		cfgInclude = oss.str();
	}
	string cfgExclude = "-";
	if ( !_wfidFirewall.deny.empty() ) {
		stringstream oss;
		for ( const auto &wfid : _wfidFirewall.deny ) {
			oss << endl << "      " << wfid;
		}
		cfgExclude = oss.str();
	}

	SEISCOMP_INFO(R"(Configuration:
  database      : %s
  archive       : %s
  threads       : %i
  jitter        : %f
  max segments  : %zu
  nslc list     : %s
  data filter
    start time  : %s
    end time    : %s
    nslc include: %s
    nslc exclude: %s
  mtime%s)",
	              _settings.database.URI.c_str(), _archive.c_str(), _threads,
	              _jitter, _maxSegments,
	              (_wfidFile.empty()
	                       ?string("obtained by archive scan")
	                       :_wfidFile).c_str(),
	              (_startTime?_startTime->iso():string("-")).c_str(),
	              (_endTime?_endTime->iso():string("-")).c_str(),
	              cfgInclude.c_str(), cfgExclude.c_str(), cfgMtime.c_str());

	// create collector
	_collector = Collector::Open(_archive.c_str());
	if ( !_collector ) {
		SEISCOMP_ERROR("Could not create data availability collector from "
		               "URL: %s", _archive.c_str());
		return false;
	}

	// update collector's time window
	setTimeWindow(_collector.get());

	// disable public object cache
	PublicObject::SetRegistrationEnabled(false);
	Notifier::Disable();

	_dataAvailability = new ::DataAvailability();
	// query all extents stored in database so far and add them to extent map
	int count = query()->loadDataExtents(_dataAvailability.get());
	SEISCOMP_INFO("Loaded %i extents (streams) from database", count);

	Collector::WaveformIDs wids;
	if ( _wfidFile.empty() ) {
		SEISCOMP_INFO("Scanning archive for waveform stream IDs");
		_collector->collectWaveformIDs(wids);
	}
	else if ( !readWFIDFile(wids, _wfidFile) ) {
		return false;
	}

	// filter collected waveform IDs
	bool filterByID = !_wfidFirewall.allow.empty() ||
	                  !_wfidFirewall.deny.empty();
	if ( filterByID ) {
		_wfidFirewall.setCachingEnabled(true);
		for ( auto it = wids.cbegin(); it != wids.cend(); ) {
			if ( _wfidFirewall.isAllowed(it->first) ) {
				++it;
			}
			else {
				SEISCOMP_DEBUG("Stream ID filtered: %s",
				               it->first.c_str());
				it = wids.erase(it);
			}
		}
	}

	// add existing extents to extent map
	bool filterByTime = _startTime || _endTime;
	ExtentMap extentMap;
	for ( size_t i = 0; i < _dataAvailability->dataExtentCount(); ++i) {
		auto *extent = _dataAvailability->dataExtent(i);
		auto sid = streamID(extent->waveformID());

		if ( filterByID && !_wfidFirewall.isAllowed(sid) ) {
			SEISCOMP_DEBUG("Skipping existing extent %s: stream ID does not "
			               "pass filter", sid.c_str());
			continue;
		}

		if ( filterByTime && wids.find(sid) == wids.end() ) {
			if ( _startTime && _startTime >= extent->end() ) {
				SEISCOMP_DEBUG("Skipping existing extent %s: end time ahead of "
				               "scan window and no new data found", sid.c_str());
				continue;
			}
			if ( _endTime && _endTime < extent->start() ) {
				SEISCOMP_DEBUG("Skipping existing extent %s: start time behind "
				               "scan window and no new data found", sid.c_str());
				continue;
			}
		}
		extentMap[sid] = extent;
	}

	// stop here if archive is empty and no extents have been found in database
	if ( wids.empty() && extentMap.empty() ) {
		SEISCOMP_INFO("%srchive is empty and no extents need to be removed",
		              filterByID || filterByTime ? "Filtered a" : "A");
		return true;
	}

	// Create N worker threads. If the collector is marked as not thread safe
	// a new collector instance needs to be created starting with the 2nd
	// worker instance.
	SEISCOMP_INFO("Creating %i worker threads", _threads);
	WorkerList workers;
	for ( int i = 1; i <= _threads; ++i ) {
		workers.push_back(new thread(bind(&SCARDAC::processExtents, this, i)));
	}

	// add extents to work queue, push may block if queue size is exceeded
	for ( auto &it : extentMap ) {
		_workQueue.push(WorkQueueItem(it.second, true));
	}

	// search for new streams and create new extents
	size_t oldSize = extentMap.size();
	SEISCOMP_INFO("Processing new streams");
	for ( const auto &wid : wids ) {
		if ( extentMap.find(wid.first) != extentMap.end() ) {
			continue;
		}

		auto *extent = DataExtent::Create();
		extent->setWaveformID(wid.second);
		_dataAvailability->add(extent);
		extentMap[wid.first] = extent;
		_workQueue.push(WorkQueueItem(extent, false));
	}
	SEISCOMP_INFO("Found %zu new streams in archive",
	              extentMap.size() - oldSize);

	// a nullptr object is used to signal end of queue
	_workQueue.push(WorkQueueItem());
	SEISCOMP_INFO("Last stream pushed, waiting for worker to terminate");

	// wait for all workers to terminate
	for ( auto &worker : workers ) {
		worker->join();
		delete worker;
	}
	workers.clear();

	_workQueue.reset();

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void SCARDAC::done() {
	_testData.clear();

	if ( _collector ) {
		_collector->reset();
	}

	if ( _dataAvailability ) {
		_dataAvailability.reset();
	}

	Client::Application::done();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void SCARDAC::setTimeWindow(Collector *collector) {
	if ( _startTime ) {
		collector->setStartTime(*_startTime);
	}

	if ( _endTime ) {
		collector->setEndTime(*_endTime);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void SCARDAC::processExtents(int threadID) {
	auto *collector = _collector.get();
	if ( threadID > 1 && !_collector->threadSafe() ) {
		collector = Collector::Open(_archive.c_str());
		setTimeWindow(collector);
	}
	Worker worker(this, threadID, collector);

	WorkQueueItem item;
	while ( !_exitRequested ) {
		try {
			item = _workQueue.pop();
		}
		catch ( Client::QueueClosedException & ) {
			SEISCOMP_DEBUG("[%i] Work queue closed", threadID);
			return;
		}

		if ( !item.extent ) {
			SEISCOMP_INFO("[%i] Read last extent, closing queue", threadID);
			_workQueue.close();
			return;
		}

		worker.processExtent(item.extent, item.foundInDB);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool SCARDAC::generateTestData() {
	if ( !query()->driver()->isConnected() ) {
		SEISCOMP_ERROR("Database connection not available");
		return false;
	}

	setLoadInventoryEnabled(true);
	if ( !reloadInventory() ) {
		SEISCOMP_ERROR("Could not load inventory");
		return false;
	}

	DatabaseObjectWriter dbWriter(*query(), true);

	vector<string> toks;
	Core::split(toks, _testData.c_str(), ",");

	int gaps;
	int overlaps;
	int segments;
	double days;
	double gaplen;
	double overlaplen;
	if ( toks.size() != 5 ||
	     !Core::fromString(days, toks[0]) ||
	     !Core::fromString(gaps, toks[1]) ||
	     !Core::fromString(gaplen, toks[2]) ||
	     !Core::fromString(overlaps, toks[3]) ||
	     !Core::fromString(overlaplen, toks[4]) ||
	     gaps < 0 || overlaps < 0 || (gaps > 0 && gaplen <= 0) ||
	     (overlaps > 0 && overlaplen <= 0 ) ) {
		SEISCOMP_ERROR("Invalid format in parameter 'generate-test-data'");
		return false;
	}

	auto end = Core::Time::UTC();
	auto start = end - Core::TimeSpan(days * 86400.0, 0);
	segments = gaps + overlaps + 1;
	Core::TimeSpan segStep(days * 86400.0 / segments);
	Core::TimeSpan gapLen(gaplen);
	Core::TimeSpan overlapLen(overlaplen);

	auto *inv = Client::Inventory::Instance()->inventory();
	for ( size_t iNet = 0; iNet < inv->networkCount(); ++iNet ) {
		auto *net = inv->network(iNet);

		for ( size_t iSta = 0; iSta < net->stationCount(); ++iSta ) {
			auto *sta = net->station(iSta);

			for ( size_t iLoc = 0; iLoc < sta->sensorLocationCount(); ++iLoc ) {
				auto *loc = sta->sensorLocation(iLoc);

				for ( size_t iCha = 0; iCha < loc->streamCount(); ++iCha ) {
					auto *cha = loc->stream(iCha);
					WaveformStreamID wid(net->code(), sta->code(),
					                     loc->code(), cha->code(), "");
					double sr(static_cast<double>(cha->sampleRateNumerator()) /
					          cha->sampleRateDenominator());

					DataExtentPtr ext = DataExtent::Create();
					ext->setParent(_dataAvailability.get());
					ext->setWaveformID(wid);
					ext->setStart(start);
					ext->setEnd(end);
					ext->setUpdated(end);
					ext->setLastScan(end);

					auto *attExt = new DataAttributeExtent();
					attExt->setStart(start);
					attExt->setEnd(end);
					attExt->setUpdated(end);
					attExt->setQuality("M");
					attExt->setSampleRate(sr);
					attExt->setSegmentCount(gaps + overlaps);
					ext->add(attExt);

					auto t = start;
					DataSegment *seg;
					for ( int i = 0; i < segments; ++i ) {
						seg = new DataSegment();
						if ( i <= gaps ) {
							seg->setStart(t);
							t += segStep;
							seg->setEnd(t - gapLen);
						}
						else {
							seg->setStart(t - overlapLen);
							t += segStep;
							seg->setEnd(t);
						}

						seg->setQuality("M");
						seg->setSampleRate(sr);
						seg->setOutOfOrder(false);
						seg->setUpdated(end);
						ext->add(seg);
					}

					if ( dbWriter(ext.get()) ) {
						SEISCOMP_INFO("Wrote extent for stream: %s.%s.%s.%s",
						               net->code().c_str(),
						               sta->code().c_str(),
						               loc->code().c_str(),
						               cha->code().c_str());
					}
					else {
						SEISCOMP_WARNING("Could not write extent for stream: "
						                 "%s.%s.%s.%s",
						                 net->code().c_str(),
						                 sta->code().c_str(),
						                 loc->code().c_str(),
						                 cha->code().c_str());
					}
				}
			}
		}
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
} // ns DataAvailability
} // ns Seiscomp
