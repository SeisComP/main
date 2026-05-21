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
//
// processExtent uses a three-phase pipeline:
//
//   1. PLAN  - For every chunk in the archive, decide READ or SKIP based on
//              mtime, --deep-scan, mtime window, and the layout of existing
//              DB segments relative to chunk boundaries. Decisions are
//              propagated to neighbours so that any DB segment crossing a
//              chunk boundary forces both sides to be READ.
//
//   2. BUILD - Construct the new desired segment list for this extent.
//              READ chunks contribute freshly parsed segments from the
//              archive file. SKIP chunks contribute their existing DB
//              segments unchanged. After each chunk's segments are
//              appended, the new tail is merged with the previous tail if
//              they are contiguous within jitter, same sample rate and
//              same quality. The result is a fully ordered, deduplicated
//              segment list representing what the DB should look like.
//
//   3. DIFF  - Walk the desired list and the existing DB list in parallel
//              and produce insert / update / remove operations. The
//              segment counter is set to the size of the desired list.
//
// Properties:
//
//   * Idempotent: running scardac twice on unchanged data is a no-op.
//   * No counter drift: _segCount == size of desired list, by construction.
//   * No false overlaps at chunk boundaries: boundary-spanning DB
//     segments force both adjacent chunks to be re-read.
//   * Single-chunk streams are safe: there is no "trailing segments"
//     phase that can delete segments belonging to the last (skipped) chunk.
//
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

namespace {

// Per-chunk plan record built in Phase 1 and consumed in Phase 2.
struct ChunkPlan {
	std::string         path;
	Core::TimeWindow    window;
	Core::Time          mtime;
	bool                read{false};
	std::string         reason;          // for debug logging
};

// Returns true if two segments can be merged: contiguous within jitter,
// same sample rate, same quality.
inline
bool segmentsContiguous(const DataModel::DataSegment *a,
                        const DataModel::DataSegment *b,
                        float jitterFactor) {
	if ( a->sampleRate() != b->sampleRate() ||
	     a->quality()    != b->quality() ) {
		return false;
	}
	double jitter = jitterFactor / a->sampleRate();
	return std::abs((b->start() - a->end()).length()) <= jitter;
}

// Compare two segments for equality of all persisted fields, including
// `updated`. `updated` carries the max(mtime) of all source chunks that
// contributed to the segment. Including it here ensures that any source
// modification propagates to the DB even when the segment's shape
// (start/end/rate/quality/outOfOrder) is unchanged: scardac only stores a
// coarse summary of each segment and cannot detect record-level changes
// directly, so `updated` is the only signal downstream consumers receive
// that the underlying records may have changed.
inline
bool segmentsEqual(const DataModel::DataSegment *a,
                   const DataModel::DataSegment *b) {
	return a->start()      == b->start() &&
	       a->end()        == b->end()   &&
	       a->sampleRate() == b->sampleRate() &&
	       a->quality()    == b->quality() &&
	       a->outOfOrder() == b->outOfOrder() &&
	       a->updated()    == b->updated();
}

// Deep clone a DB segment into a new DataSegmentPtr (with no parent set).
inline
DataModel::DataSegmentPtr cloneSegment(const DataModel::DataSegment *src) {
	auto clone = new DataModel::DataSegment();
	clone->setStart(src->start());
	clone->setEnd(src->end());
	clone->setUpdated(src->updated());
	clone->setSampleRate(src->sampleRate());
	clone->setQuality(src->quality());
	clone->setOutOfOrder(src->outOfOrder());
	return clone;
}

} // ns anonymous




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
	_segmentsInsert.clear();
	_segmentsUpdate.clear();
	_segmentsRemove.clear();

	SEISCOMP_INFO("[%i] %s: Start processing", _id, _sid.c_str());

	// Capture scan start time BEFORE any chunk is read. This becomes the
	// new lastScan value. Any chunk modified during this scan will have
	// mtime > scanStart and will therefore be picked up on the next run.
	auto scanStart = Core::Time::UTC();

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

	// ------------------------------------------------------------------
	// Load existing DB segments and attribute extents
	// ------------------------------------------------------------------
	std::vector<DataSegmentPtr> dbSegs;
	if ( foundInDB ) {
		if ( !dbConnect() ) {
			SEISCOMP_ERROR("[%i] %s: Could not query existing attribute "
			               "extents and data segments", _id, _sid.c_str());
			return;
		}

		_db->loadDataAttributeExtents(_extent);

		_extentOID = _db->getCachedId(_extent);
		if ( _extentOID != IO::DatabaseInterface::INVALID_OID ) {
			size_t segmentsOutside = 0;
			auto it = dbSegments(segmentsOutside);
			for ( ; *it; ++it ) {
				if ( _app->_exitRequested ) return;
				auto seg = DataSegment::Cast(*it);
				if ( seg ) {
					dbSegs.emplace_back(seg);
				}
			}
			it.close();
		}

		SEISCOMP_DEBUG("[%i] %s: Found existing extent\n"
		               "  start            : %s\n"
		               "  end              : %s\n"
		               "  updated          : %s\n"
		               "  last scan        : %s\n"
		               "  attribute extents: %zu\n"
		               "  DB segments      : %zu\n"
		               "  segment overflow : %i",
		               _id, _sid.c_str(), extent->start().iso(),
		               extent->end().iso(), extent->updated().iso(),
		               extent->lastScan().iso(),
		               extent->dataAttributeExtentCount(),
		               dbSegs.size(),
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

	if ( chunks.empty() ) {
		// nothing in the archive - remove anything that may still be in DB
		for ( auto &seg : dbSegs ) {
			_segmentsRemove.push_back(seg);
		}
		_segCount = 0;
		if ( syncSegments() ) {
			_extent->setLastScan(scanStart);
			syncExtent();
		}
		return;
	}

	// ------------------------------------------------------------------
	// PHASE 1: PLAN - decide READ or SKIP for every chunk
	// ------------------------------------------------------------------
	std::vector<ChunkPlan> plan;
	plan.reserve(chunks.size());
	for ( const auto &chunkPath : chunks ) {
		ChunkPlan p;
		p.path = chunkPath;
		if ( !_collector->chunkTimeWindow(p.window, chunkPath) ) {
			SEISCOMP_WARNING("[%i] %s: Could not determine time window of "
			                 "chunk, skipping: %s",
			                 _id, _sid.c_str(), chunkPath.c_str());
			continue;
		}
		p.mtime = _collector->chunkMTime(chunkPath);
		if ( !p.mtime ) {
			p.mtime = scanStart;
		}

		// time window restriction
		if ( _app->_startTime && p.window.endTime() <= *_app->_startTime ) {
			p.read = false;
			p.reason = "before scan window";
			plan.push_back(p);
			continue;
		}
		if ( _app->_endTime && p.window.startTime() > *_app->_endTime ) {
			p.read = false;
			p.reason = "after scan window";
			plan.push_back(p);
			continue;
		}

		// mtime-based decision (using >= to handle second-resolution
		// filesystem mtimes against microsecond-resolution lastScan)
		if ( !foundInDB ) {
			p.read = true;
			p.reason = "new extent";
		}
		else if ( _app->_deepScan ) {
			p.read = true;
			p.reason = "deep scan";
		}
		else if ( _app->_mtimeEnd && p.mtime > _app->_mtimeEnd ) {
			p.read = false;
			p.reason = "mtime > modified-until";
		}
		else if ( _app->_mtimeStart && p.mtime >= *_app->_mtimeStart ) {
			p.read = true;
			p.reason = "mtime >= modified-since";
		}
		else if ( !_app->_mtimeStart && p.mtime >= _extent->lastScan() ) {
			p.read = true;
			p.reason = "mtime >= last scan";
		}
		else {
			p.read = false;
			p.reason = "unchanged";
		}

		// If the chunk would otherwise be skipped, force a re-read when no
		// existing DB segment STARTS within its window. This catches the
		// case where a chunk was moved out of the archive during a previous
		// scan (causing surrounding segments to be split at the chunk's
		// boundaries) and then later moved back with its original mtime.
		// In that situation the chunk's mtime stays older than lastScan, so
		// the mtime check above will not re-read it, and the boundary
		// READ-propagation step below cannot help either because both
		// neighbours are also SKIP. Without this override the previously
		// split segments would never be merged back.
		//
		// The predicate mirrors what the BUILD phase does for SKIP chunks:
		// a SKIP chunk only contributes DB segments STARTING in its window.
		// If no such segment exists, a SKIP would contribute nothing, so we
		// can safely force a READ instead. Segments that merely spill over
		// from the previous chunk (start before, end inside) do not count
		// here - those belong to the preceding chunk's SKIP contribution.
		//
		// Truly empty chunks (no data ever recorded) will be re-read on
		// every run under this rule, but that cost is negligible: an empty
		// chunk yields zero records and therefore zero segments.
		//
		// dbSegs is sorted by start time; a linear scan with an early
		// break is sufficient in practice. std::lower_bound could replace
		// the loop if this ever shows up as hot.
		if ( !p.read && foundInDB ) {
			bool hasStart = false;
			for ( const auto &seg : dbSegs ) {
				if ( seg->start() >= p.window.endTime() ) break;
				if ( seg->start() >= p.window.startTime() ) {
					hasStart = true;
					break;
				}
			}
			if ( !hasStart ) {
				p.read = true;
				p.reason = "no DB segment starts in chunk window";
			}
		}

		plan.push_back(p);
	}

	if ( plan.empty() ) {
		SEISCOMP_INFO("[%i] %s: No usable chunks after time-window filter",
		              _id, _sid.c_str());
		return;
	}

	// Propagate READ to neighbours when DB segments cross chunk
	// boundaries. Any DB segment that straddles the boundary between two
	// adjacent chunks forces both chunks to be READ, otherwise a fresh
	// read on one side could conflict with the stale representation on
	// the other. We iterate to a fixed point; propagation is purely local
	// and converges in one or two passes.
	bool changed = true;
	while ( changed && !_app->_exitRequested ) {
		changed = false;
		for ( const auto &seg : dbSegs ) {
			for ( size_t i = 0; i + 1 < plan.size(); ++i ) {
				auto &cur  = plan[i];
				auto &next = plan[i + 1];

				// segment crosses the boundary between cur and next
				bool spans = seg->start() <  cur.window.endTime() &&
				             seg->end()   >  next.window.startTime();
				if ( !spans ) continue;

				if ( cur.read && !next.read ) {
					next.read = true;
					next.reason = "neighbour of boundary-spanning DB segment";
					changed = true;
				}
				else if ( next.read && !cur.read ) {
					cur.read = true;
					cur.reason = "neighbour of boundary-spanning DB segment";
					changed = true;
				}
			}
		}
	}

	for ( const auto &p : plan ) {
		SEISCOMP_DEBUG("[%i] %s: Plan: %s (%s, mtime %s, %s)",
		               _id, _sid.c_str(),
		               p.read ? "READ" : "SKIP",
		               p.reason.c_str(),
		               p.mtime.iso().c_str(),
		               p.path.c_str());
	}

	// ------------------------------------------------------------------
	// PHASE 2: BUILD the desired segment list
	// ------------------------------------------------------------------
	//
	// For each chunk:
	//   READ -> parse records and produce chunk segments
	//   SKIP -> copy DB segments whose start falls in this chunk's window
	//
	// After appending any segment, try to merge it with the tail of
	// newSegs (contiguous within jitter, same rate, same quality).
	//
	// Boundary-spanning DB segments are only copied once - by the chunk
	// they START in. The propagation rule above guarantees that if a DB
	// segment spans two chunks, either both are READ (fresh data replaces
	// it entirely and it is never copied here) or both are SKIP (it is
	// copied verbatim by its start chunk and reaches into the end
	// chunk's window without conflict).
	std::vector<DataSegmentPtr> newSegs;
	newSegs.reserve(dbSegs.size() + plan.size());

	auto appendSegment = [&](DataSegmentPtr seg) {
		if ( !newSegs.empty() ) {
			auto &tail = newSegs.back();
			if ( segmentsContiguous(tail.get(), seg.get(), _app->_jitter) ) {
				if ( seg->end() > tail->end() ) {
					tail->setEnd(seg->end());
				}
				if ( seg->updated() > tail->updated() ) {
					tail->setUpdated(seg->updated());
				}
				if ( seg->outOfOrder() ) {
					tail->setOutOfOrder(true);
				}
				return;
			}
		}
		newSegs.push_back(seg);
	};

	auto dbSegsStartingIn = [&](const Core::TimeWindow &win,
	                            std::vector<size_t> &out) {
		out.clear();
		for ( size_t i = 0; i < dbSegs.size(); ++i ) {
			const auto &s = dbSegs[i];
			if ( s->start() >= win.startTime() && s->start() < win.endTime() ) {
				out.push_back(i);
			}
		}
	};

	std::vector<size_t> dbHits;
	for ( const auto &p : plan ) {
		if ( _app->_exitRequested ) {
			return;
		}

		// check segment overflow
		if ( _app->_maxSegments && !_segmentOverflow &&
		     newSegs.size() > _app->_maxSegments ) {
			_segmentOverflow = true;
			SEISCOMP_WARNING("[%i] %s: Segment overflow detected, additional "
			                 "segments will not be added to the database",
			                 _id, _sid.c_str());
			break;
		}

		if ( p.read ) {
			SEISCOMP_DEBUG("[%i] %s: Reading chunk (%s): %s",
			               _id, _sid.c_str(), p.reason.c_str(),
			               p.path.c_str());

			Segments chunkSegs;
			if ( !readChunkSegments(chunkSegs, p.path, p.mtime, p.window) ) {
				// read failed - fall back to existing DB segments to avoid
				// silent data loss
				SEISCOMP_WARNING("[%i] %s: Falling back to DB segments for "
				                 "unreadable chunk: %s",
				                 _id, _sid.c_str(), p.path.c_str());
				dbSegsStartingIn(p.window, dbHits);
				for ( size_t idx : dbHits ) {
					appendSegment(cloneSegment(dbSegs[idx].get()));
				}
				continue;
			}

			for ( auto &s : chunkSegs ) {
				appendSegment(s);
			}
		}
		else {
			SEISCOMP_DEBUG("[%i] %s: Skipping chunk (%s): %s",
			               _id, _sid.c_str(), p.reason.c_str(),
			               p.path.c_str());

			dbSegsStartingIn(p.window, dbHits);
			for ( size_t idx : dbHits ) {
				// clone so the merge step can mutate end/updated without
				// touching the original DB segment, which we still need
				// for the diff phase
				appendSegment(cloneSegment(dbSegs[idx].get()));
			}
		}
	}

	// ------------------------------------------------------------------
	// PHASE 2b: TOUCH - bump each segment's `updated` to the maximum
	// mtime of all chunks whose windows overlap that segment.
	// ------------------------------------------------------------------
	//
	// A multi-chunk segment's `updated` should reflect the latest source
	// modification across ALL chunks contributing data to it - not only
	// chunks that happened to be re-read in this run. The READ/SKIP
	// propagation step ensures correct segment SHAPE, but `updated` is
	// a metadata signal that downstream consumers use to detect record-
	// level changes scardac cannot see directly. Compute it uniformly
	// here.
	//
	// Both newSegs and plan are roughly sorted by time, so the per-segment
	// search walks a small contiguous slice of plan.
	for ( auto &seg : newSegs ) {
		for ( const auto &p : plan ) {
			if ( p.window.endTime() <= seg->start() ) continue;
			if ( p.window.startTime() >= seg->end() ) break;
			if ( p.mtime > seg->updated() ) {
				seg->setUpdated(p.mtime);
			}
		}
	}

	// ------------------------------------------------------------------
	// PHASE 3: DIFF newSegs against dbSegs
	// ------------------------------------------------------------------
	std::sort(newSegs.begin(), newSegs.end(),
	          [](const DataSegmentPtr &a, const DataSegmentPtr &b) {
		return a->start() < b->start();
	});
	std::sort(dbSegs.begin(), dbSegs.end(),
	          [](const DataSegmentPtr &a, const DataSegmentPtr &b) {
		return a->start() < b->start();
	});

	size_t iNew = 0;
	size_t iDb  = 0;
	while ( iNew < newSegs.size() && iDb < dbSegs.size() ) {
		if ( _app->_exitRequested ) return;

		auto &ns = newSegs[iNew];
		auto &ds = dbSegs[iDb];

		if ( ns->start() == ds->start() ) {
			if ( !segmentsEqual(ns.get(), ds.get()) ) {
				// same start, fields differ: update the DB row in place
				ds->setEnd(ns->end());
				ds->setUpdated(ns->updated());
				ds->setSampleRate(ns->sampleRate());
				ds->setQuality(ns->quality());
				ds->setOutOfOrder(ns->outOfOrder());
				_segmentsUpdate.push_back(ds);
				SEISCOMP_DEBUG("[%i] %s: Update DB segment [%s~%s]",
				               _id, _sid.c_str(), ds->start().iso(),
				               ds->end().iso());
			}
			++iNew;
			++iDb;
		}
		else if ( ns->start() < ds->start() ) {
			_segmentsInsert.push_back(ns);
			SEISCOMP_DEBUG("[%i] %s: Add segment [%s~%s]",
			               _id, _sid.c_str(), ns->start().iso(),
			               ns->end().iso());
			++iNew;
		}
		else {
			_segmentsRemove.push_back(ds);
			SEISCOMP_DEBUG("[%i] %s: Remove DB segment [%s~%s]",
			               _id, _sid.c_str(), ds->start().iso(),
			               ds->end().iso());
			++iDb;
		}
	}
	while ( iNew < newSegs.size() ) {
		if ( _app->_exitRequested ) return;
		auto &ns = newSegs[iNew++];
		_segmentsInsert.push_back(ns);
		SEISCOMP_DEBUG("[%i] %s: Add segment [%s~%s]",
		               _id, _sid.c_str(), ns->start().iso(),
		               ns->end().iso());
	}
	while ( iDb < dbSegs.size() ) {
		if ( _app->_exitRequested ) return;
		auto &ds = dbSegs[iDb++];
		_segmentsRemove.push_back(ds);
		SEISCOMP_DEBUG("[%i] %s: Remove DB segment [%s~%s]",
		               _id, _sid.c_str(), ds->start().iso(),
		               ds->end().iso());
	}

	_segCount = newSegs.size();

	if ( syncSegments() ) {
		_extent->setLastScan(scanStart);
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

	DataModel::DataSegmentPtr segment = nullptr;
	double jitter = 0;

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

	// enforce --start/--end time window restriction on segments. Chunks whose
	// window only partially overlaps the user window may still contain
	// segments lying entirely outside it - drop those here. The same
	// inclusive/exclusive semantics are used as for chunks (see PHASE 1
	// PLAN) and for DB segments in dbSegments(), so a segment is kept iff
	//   (no startTime OR seg.end()   >  startTime) AND
	//   (no endTime   OR seg.start() <= endTime)
	uint32_t outsideWindow = 0;
	if ( _app->_startTime || _app->_endTime ) {
		auto isOutside = [this](const DataModel::DataSegmentPtr &s) {
			if ( _app->_startTime && s->end() <= *_app->_startTime ) {
				return true;
			}
			if ( _app->_endTime && s->start() > *_app->_endTime ) {
				return true;
			}
			return false;
		};
		auto newEnd = std::remove_if(segments.begin(), segments.end(),
		                             [&](const DataModel::DataSegmentPtr &s) {
			if ( isOutside(s) ) {
				++outsideWindow;
				return true;
			}
			return false;
		});
		segments.erase(newEnd, segments.end());
	}

	SEISCOMP_DEBUG("[%i] %s: %s, Results:\n"
	               "  time window          : %s ~ %s (%.1fs)\n"
	               "  modification time    : %s\n"
	               "  segments             : %zu\n"
	               "  gaps                 : %i\n"
	               "  overlaps             : %i\n"
	               "  out of order segments: %i\n"
	               "  dropped segments     : %i\n"
	               "  outside scan window  : %i\n"
	               "  sampling rate changes: %i\n"
	               "  quality changes      : %i\n"
	               "  records              : %i\n"
	               "  availability         : %.2f%% (%.1fs)",
	               _id, _sid.c_str(), chunk.c_str(),
	               window.startTime().iso().c_str(),
	               window.endTime().iso().c_str(), static_cast<double>(window.length()),
	               mtime.iso().c_str(), segments.size(), gaps, overlaps,
	               outOfOrder, dropped, outsideWindow, rateChanges, qualityChanges, records,
	               availability / static_cast<double>(window.length()) * 100.0, availability);

	return true;
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
	SEISCOMP_INFO("[%i] %s: Synchronizing segments with database "
	              "(insert/update/remove): %zu/%zu/%zu",
	              _id, _sid.c_str(),
	              _segmentsInsert.size(), _segmentsUpdate.size(),
	              _segmentsRemove.size());

	if ( _app->_exitRequested || !dbConnect() || !_extent ) {
		return false;
	}

	// Order matters: remove before insert frees any (start_time, parent)
	// uniqueness constraint that an inserted segment might collide with;
	// update lives between because it neither frees nor allocates rows.

	// 1. remove
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

	// 2. update
	for ( auto &segment : _segmentsUpdate ) {
		if ( _app->_exitRequested ) {
			return false;
		}

		segment->setParent(_extent);
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

	// 3. insert
	for ( auto &segment : _segmentsInsert ) {
		if ( _app->_exitRequested ) {
			return false;
		}

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

	_dataAvailability = new ::DataAvailability();

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
