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

#define MAX_THREADS 1000
#define MAX_BATCHSIZE 1000

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
#include <vector>


using namespace std;
using namespace Seiscomp::DataModel;

namespace Seiscomp {
namespace DataAvailability {

namespace {

inline
bool equalsNoUpdated(const DataSegment *s1,
                     const DataSegment *s2) {
	return s1->start()      == s2->start() &&
	       s1->end()        == s2->end() &&
	       s1->sampleRate() == s2->sampleRate() &&
	       s1->quality()    == s2->quality() &&
	       s1->outOfOrder() == s2->outOfOrder();
}

inline
bool compareSegmentStart(const DataSegmentPtr &a, const DataSegmentPtr &b) {
	return a->start() < b->start();
}

inline
void updateExtent(DataExtent &ext, const DataSegment *seg) {
	// first segment: update extent start time
	if ( !ext.start() )
		ext.setStart(seg->start());

	// check for last end time which is not necessarily to be found
	// in last segment of last chunk
	if ( seg->end() > ext.end() )
		ext.setEnd(seg->end());

	DataAttributeExtent *attExt = ext.dataAttributeExtent(
	    DataAttributeExtentIndex(seg->sampleRate(), seg->quality()));
	if ( attExt == NULL ) {
		attExt = new DataAttributeExtent();
		attExt->setSampleRate(seg->sampleRate());
		attExt->setQuality(seg->quality());
		attExt->setStart(seg->start());
		attExt->setEnd(seg->end());
		attExt->setUpdated(seg->updated());
		attExt->setSegmentCount(1);
		ext.add(attExt);
	}
	else {
		// update of start time not necessary since segments are process in
		// sequential order in respect to their start time
		if ( seg->end() > attExt->end() )
			attExt->setEnd(seg->end());
		if ( seg->updated() > attExt->updated() )
			attExt->setUpdated(seg->updated());
		attExt->setSegmentCount(attExt->segmentCount() + 1);
	}
}

} // ns anonymous
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Worker::Worker(const SCARDAC *app, int id, Collector *collector)
: _app(app), _id(id), _collector(collector), _extent(NULL) {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Worker::processExtent(DataExtent *extent, bool foundInDB) {
	if ( extent == NULL ) return;

	const WaveformStreamID &wid = extent->waveformID();

	_extent = extent;
	_sid = streamID(wid);
	_segmentsRemove.clear();
	_segmentsAdd.clear();
	_currentSegment = NULL;

//	vector<DataAttributeExtentIndex> attributeIndices;

	SEISCOMP_INFO("[%i] %s: start processing", _id, _sid.c_str());
	Collector::DataChunks chunks;
	_collector->collectChunks(chunks, _extent->waveformID());

	if ( chunks.empty() ) {
		SEISCOMP_INFO("[%i] %s: found no data chunks", _id, _sid.c_str());
	}
	else {
		SEISCOMP_INFO("[%i] %s: found %lu data chunks", _id, _sid.c_str(),
		              static_cast<unsigned long>(chunks.size()));
		SEISCOMP_DEBUG("[%i] %s: first: %s", _id, _sid.c_str(),
		               chunks.front().c_str());
		SEISCOMP_DEBUG("[%i] %s: last : %s", _id, _sid.c_str(),
		               chunks.back().c_str());
	}

	DatabaseIterator db_seg_it;
	// check if extent exists
	if ( foundInDB ) {
		// query existing segments
		if ( ! dbConnect(_dbRead, "read") ) {
			SEISCOMP_ERROR("[%i] %s: could not query existing attribute "
			               "extents and data segments", _id, _sid.c_str());
			return;
		}

		// load existing data attribute extents and segments
		_dbRead->loadDataAttributeExtents(_extent);
		db_seg_it = dbSegments();
	}
	else if ( ! writeExtent(OP_ADD) )
		return;

	Core::Time now = Core::Time::GMT();
	DataExtent scanExt("tmp_" + _sid);

	// iterate over all stream chunks
	Segments segments;
	size_t segCount = 0;
	Core::TimeWindow chunkWindow;
	for ( const auto &chunk : chunks ) {
		if ( _app->_exitRequested || scanExt.segmentOverflow() ) {
			break;
		}

		if ( !_collector->chunkTimeWindow(chunkWindow, chunk) ) {
			SEISCOMP_WARNING("[%i] %s: invalid chunk time window, skipping: %s",
			                 _id, _sid.c_str(), chunk.c_str() );
			continue;
		}

		Core::Time mtime = _collector->chunkMTime(chunk);
		if ( !mtime ) mtime = now;
		if ( mtime > scanExt.updated() ) scanExt.setUpdated(mtime);

		// check if chunk was modified since last scan
		if ( false ) { /*!_app->_deepScan && _extent->lastScan() && mtime <= _extent->lastScan() ) {
			// chunk not modified since last scan, advance db iterator to segment
			// containing end time of chunk
			Core::Time chunkEnd = chunkStart + TimeSpan(86400, 0);
			for ( ; *seg_it && !_app->_exitRequested; ++seg_it ) {
				DataSegment s = DataSegment::Cast(*dbSegIt);
				if ( ! s )
					continue;
				if ( s->start() <= chunkEnd ) {
					dbSegment = s;
				}
				else
					break;
			}*/
		}
		else {
			if ( !readChunkSegments(segments, chunk, mtime, chunkWindow) )
				continue;

			// process chunk segments with the exception of the last element
			// which might be extended later on by records of the next data
			// chunk
			for ( auto it = segments.cbegin(),
			      last = --segments.cend(); it != segments.cend(); ++it ) {
				if ( _app->_exitRequested ) return;

				// check for segment overflow
				if ( _app->_maxSegments >= 0 &&
				     !scanExt.segmentOverflow() &&
				     segCount >= (unsigned long)_app->_maxSegments ) {
					scanExt.setSegmentOverflow(true);
					SEISCOMP_WARNING("[%i] %s: segment overflow detected",
					                 _id, _sid.c_str());
				}

				_currentSegment = *it;

				if ( it == last ) break;

				++segCount;

				// update extent and attribute extent boundaries
				updateExtent(scanExt, _currentSegment.get());

				// remove database segments no longer found in chunk
				if ( !scanExt.segmentOverflow() &&
				     !findDBSegment(db_seg_it, _currentSegment.get() ) ) {
					addSegment(_currentSegment);
				}
			}
		}
	}

	// process last segment
	if ( _currentSegment && !scanExt.segmentOverflow() ) {
		// update extent and attribute extent boundaries
		updateExtent(scanExt, _currentSegment.get());

		// remove database segments no longer found in chunk
		if ( !scanExt.segmentOverflow() &&
		     !findDBSegment(db_seg_it, _currentSegment.get()) ) {
			addSegment(_currentSegment);
		}
	}

	// remove trailing database segments
	for ( ; !_app->_exitRequested && *db_seg_it; ++db_seg_it ) {
		DataSegmentPtr dbSeg = DataSegment::Cast(*db_seg_it);
		dbSeg->setParent(_extent);
		removeSegment(dbSeg);
	}
	flushSegmentBuffers();

	// sync attribute extents with database
	syncAttributeExtents(scanExt);

	// update extent
	if ( _currentSegment ) {
		_extent->setLastScan(now);
		bool extentModified = false;
		if ( _extent->start() != scanExt.start() || _extent->end() != scanExt.end() ||
		     _extent->updated() != scanExt.updated() ||
		     _extent->segmentOverflow() != _extent->segmentOverflow() ) {
			_extent->setStart(scanExt.start());
			_extent->setEnd(scanExt.end());
			_extent->setUpdated(scanExt.updated());
			_extent->setSegmentOverflow(scanExt.segmentOverflow());
			extentModified = true;
		}

		if ( writeExtent(OP_UPDATE) ) {
			SEISCOMP_INFO("[%i] %s: extent %s: %s ~ %s", _id, _sid.c_str(),
			              string(extentModified?"modified":"unchanged").c_str(),
			              _extent->start().iso().c_str(),
			              _extent->end().iso().c_str());
		}
	}
	else {
		// no segments found, remove entire extent and all attribute extents
		if ( writeExtent(OP_REMOVE) ) {
			SEISCOMP_INFO("[%i] %s: extent removed", _id, _sid.c_str());
		}
	}

	db_seg_it.close();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Worker::dbConnect(DatabaseReaderPtr &db, const char *info) {
	while ( ! _app->_exitRequested && ( !db || !db->driver()->isConnected() ) ) {
		SEISCOMP_DEBUG("[%i] initializing database %s connection", _id, info);
		IO::DatabaseInterfacePtr dbInterface =
		        IO::DatabaseInterface::Open(_app->databaseURI().c_str());
		if ( dbInterface ) {
			db = new DatabaseReader(dbInterface.get());
		}
		else {
			SEISCOMP_DEBUG("[%i] trying to reconnect in 5s", _id);
			for ( int i = 0; i < 5 && ! _app->_exitRequested; ++i )
				sleep(1);
		}
	}
	if ( db && db->driver()->isConnected() )
		return true;

	SEISCOMP_ERROR("[%i] could not initializing database %s connection", _id, info);
	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
#define _T(name) _dbRead->driver()->convertColumnName(name)
DatabaseIterator Worker::dbSegments() {
	std::ostringstream oss;
	oss << "SELECT DataSegment.* "
	       "FROM DataSegment, PublicObject AS PDataExtent "
	    << "WHERE PDataExtent." << _T("publicID") << "='"
	    <<     _dbRead->toString(_extent->publicID()) << "' AND "
	           "DataSegment._parent_oid=PDataExtent._oid "
	       "ORDER BY DataSegment." << _T("start") << " ASC, "
	                "DataSegment." << _T("start_ms") << " ASC";

	return _dbRead->getObjectIterator(oss.str(), DataSegment::TypeInfo());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Worker::readChunkSegments(Segments &segments, const std::string &chunk,
                               const Core::Time &mtime,
                               const Core::TimeWindow &window) {
	segments.clear();

	Collector::RecordIteratorPtr it;
	try {
		it = _collector->begin(chunk, _extent->waveformID());
	} catch ( CollectorException &e) {
		SEISCOMP_WARNING("[%i] %s: %s: %s",
		                 _id, _sid.c_str(), chunk.c_str(), e.what());
		return false;
	}

	DataSegmentPtr segment = _currentSegment;

	uint32_t records         = 0;
	uint32_t gaps            = 0;
	uint32_t overlaps        = 0;
	uint32_t outOfOrder      = 0;
	uint32_t rateChanges     = 0;
	uint32_t qualityChanges  = 0;
	double availability      = 0; // in seconds

	while ( it->next() && !_app->_exitRequested ) {
		// assert valid sampling rate
		if ( it->sampleRate() <= 0 ) {
			++records;
			SEISCOMP_WARNING("[%i] %s: invalid sampling rate in record #%i",
			                 _id, _sid.c_str(), records);
			continue;
		}

		// set time jitter to half of sample time
		double jitter = _app->_jitter / it->sampleRate();

		// check if record can be merged with current segment
		bool merge = false;
		if ( segment ) {
			// gap
			if ( (it->startTime() - segment->end()).length() > jitter ) {
				++gaps;
				SEISCOMP_DEBUG("[%i] %s: detected gap: %s ~ %s (%.1fs)",
				               _id, _sid.c_str(),
				               segment->end().iso().c_str(),
				               it->startTime().iso().c_str(),
				               (it->startTime() - segment->end()).length());
			}
			// overlap
			else if ( (segment->end() - it->startTime()).length() > jitter ) {
				++overlaps;
				SEISCOMP_DEBUG("[%i] %s: detected overlap: %s ~ %s (%.1fs)",
				               _id, _sid.c_str(), it->startTime().iso().c_str(),
				               segment->end().iso().c_str(),
				               (segment->end() - it->startTime()).length());
			}
			else {
				merge = true;
			}

			// sampling rate change
			if ( segment->sampleRate() != it->sampleRate() ) {
				++rateChanges;
				SEISCOMP_DEBUG("[%i] %s: detected change of sampling rate at "
				               "%s: %.1f -> %.1f", _id, _sid.c_str(),
				               it->startTime().iso().c_str(),
				               segment->sampleRate(), it->sampleRate());
				merge = false;
			}

			// quality change
			if ( segment->quality() != it->quality() ) {
				++qualityChanges;
				SEISCOMP_DEBUG("[%i] %s: detected change of quality at %s "
				               "%s -> %s", _id, _sid.c_str(),
				               it->startTime().iso().c_str(),
				               segment->quality().c_str(),
				               it->quality().c_str());
				merge = false;
			}
		}

		if ( merge ) {
			// check if first record is merged with segment of previous chunk:
			// update time if this chunk's mtime is greater the segment mtime
			if ( records == 0 && mtime > segment->updated() )
				segment->setUpdated(mtime);
			segment->setEnd(it->endTime());
		}
		else {
			bool ooo = false;
			if ( segment ) {
				segments.push_back(segment.get());
				if ( it->startTime() < segment->start() ) {
					ooo = true;
					++outOfOrder;
				}
			}
			segment = new DataSegment();
			segment->setStart(it->startTime());
			segment->setEnd(it->endTime());
			segment->setUpdated(mtime);
			segment->setSampleRate(it->sampleRate());
			segment->setQuality(it->quality());
			segment->setOutOfOrder(ooo);
			segment->setParent(_extent);
		}

		records += 1;
		availability += (it->endTime() - it->startTime()).length();
//		SEISCOMP_DEBUG("%s - %s (%.3fs)", it->startTime().iso().c_str(),
//		               it->endTime().iso().c_str(),
//		               (it->endTime() - it->startTime()).length());
	}

	// save last segment
	if ( segment ) {
		segments.push_back(segment.get());

		// sort segment vector according start time if out of order data
		// was detected
		if ( outOfOrder > 0 ) {
			sort(segments.begin(), segments.end(), compareSegmentStart);
		}

		// check segments for duplicated start time, keep segment with largest
		// time window
		uint32_t dropped = 0;
		Segments::iterator it = segments.begin();
		Segments::iterator last = it++;
		while ( it != segments.end() ) {
			if ( (*it)->start() != (*last)->start() ) {
				++it; ++last;
				continue;
			}

			SEISCOMP_DEBUG("[%i] %s: dropping segment with duplicated start "
			                "time: %s", _id, _sid.c_str(),
			                (*it)->start().iso().c_str());
			segments.erase((*it)->end() > (*last)->end() ? last : it );
			++dropped;
		}

		SEISCOMP_DEBUG("[%i] %s: %s, results:\n"
		               "  time window          : %s ~ %s (%.1fs)\n"
		               "  modification time    : %s\n"
		               "  segments             : %lu\n"
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
		               mtime.iso().c_str(), (unsigned long)segments.size(),
		               gaps, overlaps, outOfOrder, dropped, rateChanges,
		               qualityChanges, records,
		               availability/window.length()*100.0, availability);
	}
	else {
		SEISCOMP_WARNING("[%i] %s: found no data in chunk: %s ", _id,
		                 _sid.c_str(), chunk.c_str());
		return false;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Worker::findDBSegment(DatabaseIterator &it, const DataSegment *segment) {
	if ( !segment || !*it ) return false;

	for ( ; !_app->_exitRequested && *it; ++it ) {
		DataSegmentPtr dbSeg = DataSegment::Cast(*it);
		if ( dbSeg->start() > segment->start() )
			break;
		if ( equalsNoUpdated(dbSeg.get(), segment) ) {
			++it;
			return true;
		}

		dbSeg->setParent(_extent);
		removeSegment(dbSeg);
	}

	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Worker::removeSegment(const DataSegmentPtr &segment){
	_segmentsRemove.push_back(segment);
	if ( _segmentsRemove.size() + _segmentsAdd.size() >=
	     (unsigned long) _app->_batchSize ) {
		flushSegmentBuffers();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Worker::addSegment(const DataSegmentPtr &segment) {
	_segmentsAdd.push_back(segment);
	if ( _segmentsRemove.size() + _segmentsAdd.size() >=
	     (unsigned long) _app->_batchSize ) {
		flushSegmentBuffers();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Worker::flushSegmentBuffers() {
	if ( _segmentsRemove.empty() && _segmentsAdd.empty() ) return;

	if ( dbConnect(_dbWrite, "write") ) {
		_dbWrite->driver()->start();

		for ( Segments::iterator it = _segmentsRemove.begin();
		      it != _segmentsRemove.end(); ) {
			if ( _dbWrite->remove(it->get()) )
				it = _segmentsRemove.erase(it);
			else
				++it;
		}
		for ( Segments::iterator it = _segmentsAdd.begin();
		      it != _segmentsAdd.end(); ) {
			if ( _dbWrite->write(it->get()) )
				it = _segmentsAdd.erase(it);
			else
				++it;
		}
		_dbWrite->driver()->commit();
	}

	if ( !_segmentsRemove.empty() ) {
		SEISCOMP_ERROR("[%i] %s: failed to add %lu segments",
		               _id, _sid.c_str(), (long unsigned) _segmentsRemove.size());
		_segmentsRemove.clear();
	}

	if ( !_segmentsAdd.empty() ) {
		SEISCOMP_ERROR("[%i] %s: failed to add %lu segments",
		               _id, _sid.c_str(), (long unsigned) _segmentsAdd.size());
		_segmentsAdd.clear();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Worker::writeExtent(Operation op) {
	if ( op == OP_UNDEFINED || ! dbConnect(_dbWrite, "write") )
		return false;

	_dbWrite->driver()->start();
	if ( ( op == OP_ADD    && ! _dbWrite->write(_extent)  ) ||
	     ( op == OP_UPDATE && ! _dbWrite->update(_extent) ) ||
	     ( op == OP_REMOVE && ! _dbWrite->remove(_extent) ) ) {
		_dbWrite->driver()->rollback();
		SEISCOMP_ERROR("[%i] %s: could not %s extent: %s", _id, _sid.c_str(),
		               op.toString(), _extent->publicID().c_str());
		return false;
	}

	_dbWrite->driver()->commit();
	SEISCOMP_DEBUG("[%i] %s: %s extent: %s", _id, _sid.c_str(),
	               string(op == OP_ADD   ?"added":
	                      op == OP_UPDATE?"updated":"removed").c_str(),
	               _extent->publicID().c_str());
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Worker::syncAttributeExtents(const DataExtent &tmpExt) {
	if ( ! dbConnect(_dbWrite, "write") )
		return false;

	_dbWrite->driver()->start();

	// remove attribute extents no longer existing, update those who have changed
	for ( size_t i = 0; i < _extent->dataAttributeExtentCount(); ) {
		DataAttributeExtent *attExt = _extent->dataAttributeExtent(i);
		DataAttributeExtent *tmpAttExt = tmpExt.dataAttributeExtent(attExt->index());

		if ( tmpAttExt == NULL ) {
			SEISCOMP_DEBUG("[%i] %s: removing attribute extent with index %f,%s",
			               _id, _sid.c_str(), attExt->sampleRate(),
			               attExt->quality().c_str());
			_dbWrite->remove(attExt);
			_extent->removeDataAttributeExtent(i);
			continue;
		}

		if ( *attExt != *tmpAttExt ) {
			*attExt = *tmpAttExt;
			SEISCOMP_DEBUG("[%i] %s: updating attribute extent with index %f,%s",
			                _id, _sid.c_str(), attExt->sampleRate(),
			               attExt->quality().c_str());
			_dbWrite->update(attExt);
		}

		++i;
	}

	// add new attribute extents
	for ( size_t i = 0; i < tmpExt.dataAttributeExtentCount(); ++i ) {
		DataAttributeExtent *tmpAttExt = tmpExt.dataAttributeExtent(i);
		DataAttributeExtent *attExt = _extent->dataAttributeExtent(tmpAttExt->index());
		if ( attExt == NULL ) {
			attExt = new DataAttributeExtent(*tmpAttExt);
			SEISCOMP_DEBUG("[%i] %s: adding attribute extent with index %f,%s",
			                _id, _sid.c_str(), attExt->sampleRate(),
			               attExt->quality().c_str());
			_extent->add(attExt);
			_dbWrite->write(attExt);
		}
	}

	_dbWrite->driver()->commit();
	return true;
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
SCARDAC::~SCARDAC() {
	for ( WorkerList::iterator it = _worker.begin(); it != _worker.end(); ++it ) {
		delete *it;
		*it = NULL;
	}
	_worker.clear();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void SCARDAC::createCommandLineDescription() {
	commandline().addGroup("Collector");
	commandline().addOption("Collector", "archive,a",
	                        "Type and location of the waveform archive",
	                        &_archive);
	commandline().addOption("Collector", "threads",
	                        "Number of threads scanning the archive in parallel",
	                        &_threads);
	commandline().addOption("Collector", "batch-size",
	                        "Batch size of database transactions used when "
	                        "updating data availability segments, allowed "
	                        "range: [1,1000]",
	                        &_batchSize);
	commandline().addOption("Collector", "jitter",
	                        "Acceptable derivation of end time and start time "
	                        "of successive records in multiples of sample time",
	                        &_jitter);
/**
	commandline().addOption("Collector", "from",
	                        "Start time for data availablity check. "
	                        "Format: YYYY-mm-dd['T'HH:MM:SS]",
	                        &_from);
	commandline().addOption("Collector", "to",
	                        "End time for data availablity check. "
	                        "Format: YYYY-mm-dd['T'HH:MM:SS]",
	                        &_to);
	commandline().addOption("Collector", "deep-scan",
	                        "Process all data chunks independ of their "
	                        "modification time");
*/
	commandline().addOption("Collector", "generate-test-data",
	                        "For each stream in inventory generate test data. "
	                        "Format: days,gaps,gapseconds,overlaps,"
	                        "overlapseconds",
	                        &_testData);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool SCARDAC::initConfiguration() {
	if ( !Client::Application::initConfiguration() ) return false;

	try {
		_archive = SCCoreApp->configGetString("archive");
	}
	catch (...) {}

	try {
		_threads = SCCoreApp->configGetInt("threads");
	}
	catch (...) {}

	try {
		_batchSize = SCCoreApp->configGetInt("batchSize");
	}
	catch (...) {}

	try {
		_jitter = SCCoreApp->configGetDouble("jitter");
	}
	catch (...) {}

	try {
		_maxSegments = SCCoreApp->configGetInt("maxSegments");
	}
	catch (...) {}


	try {
		_deepScan = SCCoreApp->commandline().hasOption("deep-scan") ||
		            SCCoreApp->configGetBool("deepScan");
	}
	catch (...) {}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool SCARDAC::validateParameters() {
	if ( !Client::Application::validateParameters() ) return false;

	// database connection configured, no need to fetch parameters
	if ( !databaseURI().empty() ) {
		setMessagingEnabled(false);
		setDatabaseEnabled(true, false);
	}

	// thread count
	if ( _threads < 1 || _threads > MAX_THREADS ) {
		SEISCOMP_ERROR("invalid number of threads, allowed range: [1,%i]",
		               MAX_THREADS);
		return false;
	}

	// batch size
	if ( _batchSize < 1 || _batchSize > MAX_BATCHSIZE ) {
		SEISCOMP_ERROR("invalid batch size, allowed range: [1,%i]",
		               MAX_BATCHSIZE);
		return false;
	}

	// jitter samples
	if ( _jitter < 0 ) {
		SEISCOMP_ERROR("invalid jitter value, minimum value: 0");
		return false;
	}

	// start time
	if ( !_from.empty() && !Core::fromString(_startTime, _from) ) {
		SEISCOMP_ERROR("invalid start time value");
		return false;
	}

	// end time
	if ( !_to.empty() && !Core::fromString(_endTime, _to) ) {
		SEISCOMP_ERROR("invalid end time value");
		return false;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool SCARDAC::run() {
	if ( !_testData.empty() )
		return generateTestData();

	SEISCOMP_INFO("Configuration:\n"
	              "  archive     : %s\n"
	              "  threads     : %i\n"
	              "  batch size  : %i\n"
	              "  jitter      : %f\n"
	              "  max segments: %i"/**\n"
	              "  deep scan   : %s"*/,
	              _archive.c_str(), _threads, _batchSize,
	              _jitter, _maxSegments/**, Core::toString(_deepScan).c_str()*/);

	_collector = Collector::Open(_archive.c_str());
	if ( !_collector ) {
		SEISCOMP_ERROR("Could not create data availability collector from "
		               "URL: %s", _archive.c_str());
		return false;
	}

	// disable public object cache
	PublicObject::SetRegistrationEnabled(false);
	Notifier::Disable();

	// query all extents stored in database so far and add them to extent map
	int count = query()->loadDataExtents(&_dataAvailability);
	SEISCOMP_INFO("loaded %i extents (streams) from database", count);

	// add existing extents to extent map
	for ( size_t i = 0; i < _dataAvailability.dataExtentCount(); ++i) {
		DataExtent *extent = _dataAvailability.dataExtent(i);
		_extentMap[streamID(extent->waveformID())] = extent;
	}

	SEISCOMP_INFO("scanning archive for streams");
	Collector::WaveformIDs wids;
	_collector->collectWaveformIDs(wids);

	// stop here if archive is empty and no extents have been found in database
	if ( wids.empty() && _extentMap.empty() ) {
		SEISCOMP_INFO("archive is empty and no extents found in database");
		return true;
	}

	// Create N worker threads. If the collector is marked as not thread safe
	// a new collector instance needs to be created starting with the 2nd
	// worker instance.
	SEISCOMP_INFO("creating %i worker threads", _threads);
	for ( int i = 1; i <= _threads; ++i ) {
		_worker.push_back(new boost::thread(boost::bind(
		        &SCARDAC::processExtents, this, i)));
	}

	// add extents to work queue, push may block if queue size is exceeded
	for ( ExtentMap::const_iterator it = _extentMap.begin();
	      it != _extentMap.end(); ++it ) {
		_workQueue.push(WorkQueueItem(it->second, true));
	}

	// search for new streams and create new extents
	size_t oldSize = _extentMap.size();
	SEISCOMP_INFO("processing new streams");
	for ( const auto &wid : wids ) {
		if ( _extentMap.find(wid.first) != _extentMap.end() ) {
			continue;
		}

		DataExtent *extent = DataExtent::Create();
		extent->setWaveformID(wid.second);
		_dataAvailability.add(extent);
		_extentMap[wid.first] = extent;
		_workQueue.push(WorkQueueItem(extent, false));
	}
	SEISCOMP_INFO("found %lu new streams in archive",
	              static_cast<unsigned long>(_extentMap.size() - oldSize));

	// a NULL object is used to signal end of queue
	_workQueue.push(WorkQueueItem());
	SEISCOMP_INFO("last stream pushed, waiting for worker to terminate");

	// wait for all workers to terminate
	for ( WorkerList::const_iterator it = _worker.begin();
	      it != _worker.end(); ++it ) {
		(*it)->join();
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void SCARDAC::processExtents(int threadID) {
	Collector *collector = _collector.get();
	if ( threadID > 1 && !_collector->threadSafe() ) {
		collector = Collector::Open(_archive.c_str());
	}
	Worker worker(this, threadID, collector);

	WorkQueueItem item;
	while ( !_exitRequested ) {
		try {
			item = _workQueue.pop();
		}
		catch ( Client::QueueClosedException & ) {
			SEISCOMP_DEBUG("[%i] work queue closed", threadID);
			return;
		}

		if ( item.extent == NULL ) {
			SEISCOMP_INFO("[%i] read last extent, closing queue", threadID);
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
		SEISCOMP_ERROR("database connection not available");
		return false;
	}

	setLoadInventoryEnabled(true);
	if ( !reloadInventory() ) {
		SEISCOMP_ERROR("could not load inventory");
		return false;
	}

	DatabaseObjectWriter dbWriter(*query(), true, _batchSize);

	vector<string> toks;
	Core::split(toks, _testData.c_str(), ",");

	int gaps, overlaps, segments;
	double days, gaplen, overlaplen;
	if ( toks.size() != 5 ||
	     !Core::fromString(days, toks[0]) ||
	     !Core::fromString(gaps, toks[1]) ||
	     !Core::fromString(gaplen, toks[2]) ||
	     !Core::fromString(overlaps, toks[3]) ||
	     !Core::fromString(overlaplen, toks[4]) ||
	     gaps < 0 || overlaps < 0 || (gaps > 0 && gaplen <= 0) ||
	     (overlaps > 0 && overlaplen <= 0 ) ) {
		SEISCOMP_ERROR("invalid format in parameter 'generate-test-data'");
		return false;
	}

	Core::Time end = Core::Time::GMT();
	Core::Time start = end - Core::TimeSpan(days * 86400.0, 0);
	segments = gaps + overlaps + 1;
	Core::TimeSpan segStep(days * 86400.0 / segments);
	Core::TimeSpan gapLen(gaplen);
	Core::TimeSpan overlapLen(overlaplen);

	DataModel::Inventory *inv = Client::Inventory::Instance()->inventory();
	for ( size_t iNet = 0; iNet < inv->networkCount(); ++iNet ) {
		Network *net = inv->network(iNet);
		for ( size_t iSta = 0; iSta < net->stationCount(); ++iSta ) {
			Station *sta = net->station(iSta);
			for ( size_t iLoc = 0; iLoc < sta->sensorLocationCount(); ++iLoc ) {
				SensorLocation *loc = sta->sensorLocation(iLoc);
				for ( size_t iCha = 0; iCha < loc->streamCount(); ++iCha ) {
					Stream *cha = loc->stream(iCha);
					WaveformStreamID wid(net->code(), sta->code(),
					                     loc->code(), cha->code(), "");
					double sr(static_cast<double>(cha->sampleRateNumerator()) /
					          cha->sampleRateDenominator());
					DataExtentPtr ext = DataExtent::Create();
					ext->setParent(&_dataAvailability);
					ext->setWaveformID(wid);
					ext->setStart(start);
					ext->setEnd(end);
					ext->setUpdated(end);
					ext->setLastScan(end);

					DataAttributeExtent *attExt = new DataAttributeExtent();
					attExt->setStart(start);
					attExt->setEnd(end);
					attExt->setUpdated(end);
					attExt->setQuality("M");
					attExt->setSampleRate(sr);
					attExt->setSegmentCount(gaps + overlaps);
					ext->add(attExt);

					Core::Time t = start;
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
						SEISCOMP_INFO("wrote extent for stream: %s.%s.%s.%s",
						               net->code().c_str(),
						               sta->code().c_str(),
						               loc->code().c_str(),
						               cha->code().c_str());
					}
					else {
						SEISCOMP_WARNING("could not write extent for stream: "
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
