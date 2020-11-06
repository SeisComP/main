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

#ifndef SEISCOMP_DATAAVAILABILITY_SCARDAC_H
#define SEISCOMP_DATAAVAILABILITY_SCARDAC_H

#include <seiscomp/plugins/dataavailability/collector.h>

#include <seiscomp/client/application.h>
#include <seiscomp/client/queue.h>
#include <seiscomp/client/queue.ipp>

#include <seiscomp/core/datetime.h>
#include <seiscomp/core/timewindow.h>

#include <seiscomp/datamodel/dataavailability.h>
#include <seiscomp/datamodel/dataextent.h>
#include <seiscomp/datamodel/dataattributeextent.h>
#include <seiscomp/datamodel/datasegment.h>
#include <seiscomp/datamodel/databasearchive.h>

#include <string>

namespace Seiscomp {
namespace DataAvailability {

class SCARDAC;

class Worker {
	public:
		Worker(const SCARDAC *app, int id, Collector *collector);

		void processExtent(DataModel::DataExtent *extent, bool foundInDB);

	private:
		typedef std::vector<DataModel::DataSegmentPtr> Segments;
		typedef std::vector<std::pair<DataModel::DataSegmentPtr,
		                              DataModel::Operation> > SegmentDBBuffer;

		bool dbConnect(DataModel::DatabaseReaderPtr &db, const char *info);

		DataModel::DatabaseIterator dbSegments();
		bool readChunkSegments(Segments &segments, const std::string &chunk,
		                       const Core::Time &mtime,
		                       const Core::TimeWindow &window);

		bool findDBSegment(DataModel::DatabaseIterator &it,
		                   const DataModel::DataSegment *segment);

		void removeSegment(const DataModel::DataSegmentPtr &segment);
		void addSegment(const DataModel::DataSegmentPtr &segment);
		void flushSegmentBuffers();

		bool writeExtent(DataModel::Operation op);
		bool syncAttributeExtents(const DataModel::DataExtent &tmpExt);

	private:
		const SCARDAC*                  _app{nullptr};
		int                             _id{0};
		CollectorPtr                    _collector;

		// initialized by first processExtent call and reused subsequently
		DataModel::DatabaseReaderPtr    _dbRead;
		DataModel::DatabaseReaderPtr    _dbWrite;

		// variables valid per processExtent call
		DataModel::DataExtent          *_extent;
		std::string                     _sid;
		Segments                        _segmentsRemove;
		Segments                        _segmentsAdd;
		DataModel::DataSegmentPtr       _currentSegment;
};

class SCARDAC : public Client::Application {
	// ----------------------------------------------------------------------
	//  X'truction
	// ----------------------------------------------------------------------
	public:
		//! C'tor
		SCARDAC(int argc, char **argv);

		//! Destructor
		~SCARDAC();

	// ----------------------------------------------------------------------
	//  Protected types
	// ----------------------------------------------------------------------
	protected:
		struct WorkQueueItem {
			WorkQueueItem() : extent(NULL), foundInDB(false) {}
			WorkQueueItem(DataModel::DataExtent *extent, bool foundInDB)
			 : extent(extent), foundInDB(foundInDB) {}

			DataModel::DataExtent  *extent;
			bool                    foundInDB;
		};

	// ----------------------------------------------------------------------
	//  Protected functions
	// ----------------------------------------------------------------------
	protected:
		void createCommandLineDescription();
		bool initConfiguration();
		bool validateParameters();
		bool run();

		void processExtents(int threadID);
		bool generateTestData();

	// ----------------------------------------------------------------------
	//  Implementation
	// ----------------------------------------------------------------------
	protected:
		// configuration parameters
		std::string _archive{"sds://@ROOTDIR@/var/lib/archive"};
		int         _threads{1};
		int         _batchSize{100};
		float       _jitter{0.5};
		bool        _deepScan{false};
		int         _maxSegments{1000000};
		std::string _from;
		std::string _to;
		Core::Time  _startTime;
		Core::Time  _endTime;

		std::string                          _testData;
		DataAvailability::CollectorPtr       _collector;

		DataModel::DataAvailability          _dataAvailability;

		// Map of stream ID to extents
		typedef std::map<std::string, DataModel::DataExtent*> ExtentMap;
		ExtentMap                            _extentMap;

		// Thread safe queue of extends to process
		Client::ThreadedQueue<WorkQueueItem> _workQueue;

		typedef std::vector<boost::thread*> WorkerList;
		WorkerList                           _worker;

	private:
		friend class Worker;
};

} // ns DataAvailability
} // ns Seiscomp

#endif // SEISCOMP_DATAAVAILABILITY_SCARDAC_H

