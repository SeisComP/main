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

#include <seiscomp/io/database.h>

#include <seiscomp/utils/stringfirewall.h>

#include <thread>
#include <string>

namespace Seiscomp {
namespace DataAvailability {

class SCARDAC;

class Worker {
	public:
		Worker(const SCARDAC *app, int id, Collector *collector);

		void processExtent(DataModel::DataExtent *extent, bool foundInDB);

	protected:
		using Segments = std::vector<DataModel::DataSegmentPtr>;

		bool dbConnect();

		DataModel::DatabaseIterator dbSegments(size_t &segmentsOutside);
		bool readChunkSegments(Segments &segments, const std::string &chunk,
		                       DataModel::DataSegmentPtr chunkSeg,
		                       const Core::Time &mtime,
		                       const Core::TimeWindow &window);

		void diffSegment(DataModel::DatabaseIterator &db_seg_it,
		                 DataModel::DataSegment *chunkSeg, bool extent = false);

		bool writeExtent(DataModel::Operation op);
		bool syncSegments();
		void syncExtent();
		void readAttExtMillis(DataModel::DataAttributeExtent *attExt);

	protected:
		const SCARDAC*                  _app{nullptr};
		int                             _id{0};
		CollectorPtr                    _collector;

		// initialized by first processExtent call and reused subsequently
		DataModel::DatabaseReaderPtr    _db;

		// variables valid per processExtent call
		DataModel::DataExtent          *_extent{nullptr};
		IO::DatabaseInterface::OID      _extentOID{IO::DatabaseInterface::INVALID_OID};
		std::string                     _sid;
		bool                            _segmentOverflow{false};
		size_t                          _segCount{0};
		Segments                        _segmentsStore;
		Segments                        _segmentsRemove;
};

DEFINE_SMARTPOINTER(SCARDAC);
class SCARDAC : public Client::Application {
	// ----------------------------------------------------------------------
	//  X'truction
	// ----------------------------------------------------------------------
	public:
		//! C'tor
		SCARDAC(int argc, char **argv);

		//! Destructor
		~SCARDAC() override = default;

	// ----------------------------------------------------------------------
	//  Protected types
	// ----------------------------------------------------------------------
	protected:
		struct WorkQueueItem {
			WorkQueueItem() = default;
			WorkQueueItem(DataModel::DataExtent *extent, bool foundInDB)
			 : extent(extent), foundInDB(foundInDB) {}

			DataModel::DataExtent  *extent{nullptr};
			bool                    foundInDB{false};
		};

		using ExtentMap = std::map<std::string, DataModel::DataExtent*>;
		using WorkerList = std::vector<std::thread*>;

	// ----------------------------------------------------------------------
	//  Protected functions
	// ----------------------------------------------------------------------
	protected:
		void createCommandLineDescription() override;
		bool initConfiguration() override;
		bool validateParameters() override;
		bool run() override;
		void done() override;

		void setTimeWindow(Collector *collector);
		void processExtents(int threadID);
		bool generateTestData();

	// ----------------------------------------------------------------------
	//  Implementation
	// ----------------------------------------------------------------------
	protected:
		using WFIDList = std::vector<std::string>;

		// configuration parameters
		std::string     _archive{"sds://@ROOTDIR@/var/lib/archive"};
		int             _threads{1};
		float           _jitter{0.5};
		size_t          _maxSegments{1000000};
		std::string     _wfidFile;
		std::string     _start;
		std::string     _end;
		OPT(Core::Time) _startTime;
		OPT(Core::Time) _endTime;
		WFIDList        _include;
		WFIDList        _exclude;
		bool            _deepScan{false};
		std::string     _modifiedSince;
		std::string     _modifiedUntil;
		OPT(Core::Time) _mtimeStart;
		OPT(Core::Time) _mtimeEnd;

		Util::WildcardStringFirewall         _wfidFirewall;

		std::string                          _testData;
		DataAvailability::CollectorPtr       _collector{nullptr};

		DataModel::DataAvailabilityPtr       _dataAvailability{nullptr};

		// Thread safe queue of extends to process
		Client::ThreadedQueue<WorkQueueItem> _workQueue;

	private:
		friend class Worker;

		/**
		 * @brief print usage information
		 */
		void printUsage() const override;
};

} // ns DataAvailability
} // ns Seiscomp

#endif // SEISCOMP_DATAAVAILABILITY_SCARDAC_H

