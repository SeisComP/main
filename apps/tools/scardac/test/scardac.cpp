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


#define SEISCOMP_TEST_MODULE scardac

#include "../scardac.h"

// RHEL7 fix for "undefined reference to `boost::filesystem::detail::copy_file"
#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

#include <seiscomp/core/system.h>

#include <seiscomp/datamodel/databasereader.h>
#include <seiscomp/datamodel/dataavailability.h>
#include <seiscomp/datamodel/dataextent.h>

#include <seiscomp/core/timewindow.h>

#include <seiscomp/io/database.h>

#include <seiscomp/system/environment.h>
#include <seiscomp/system/pluginregistry.h>

#include <seiscomp/unittest/unittests.h>

#include <boost/test/included/unit_test.hpp>


namespace utf = boost::unit_test;
namespace fs = boost::filesystem;

using namespace Seiscomp;
using namespace std;

#define ASSERT_MSG(cond, msg) do \
{ if (!(cond)) { \
	ostringstream oss; \
	oss << __FILE__ << "(" << __LINE__ << "): "<< msg << endl; cerr << oss.str(); \
	abort(); } \
} while(0)
#define ASSERT_EQUAL_MSG(obj1, obj2, msg) \
ASSERT_MSG(obj1 == obj2, msg << ": " << #obj1 " == " #obj2 \
           " has failed [" << obj1 << " != " << obj2 << "]");

#define CHECK_EQUAL_MSG(obj1, obj2, msg) \
if ( obj1 != obj2 ) { \
	BOOST_ERROR(msg << ": " << #obj1 " == " #obj2 \
	            " has failed [" << obj1 << " != " << obj2 << "]"); \
}


void checkObjectCount(DataModel::DatabaseReaderPtr reader, const string &ctx,
                      size_t extents = 0, size_t attributeExtents = 0,
                      size_t segments = 0) {
	CHECK_EQUAL_MSG(extents, reader->getObjectCount(
	                    nullptr, DataModel::DataExtent::TypeInfo()),
	                ctx);
	CHECK_EQUAL_MSG(attributeExtents, reader->getObjectCount(
	                    nullptr, DataModel::DataAttributeExtent::TypeInfo()),
	                ctx);
	CHECK_EQUAL_MSG(segments, reader->getObjectCount(
	                    nullptr, DataModel::DataSegment::TypeInfo()),
	                ctx);
}

DataModel::DataAvailabilityPtr checkEqual(DataModel::DatabaseReaderPtr reader,
                               const DataModel::DataAvailabilityPtr daExpected,
                               const string &ctx) {

	DataModel::DataAvailabilityPtr daFound = reader->loadDataAvailability();
	BOOST_ASSERT_MSG(daFound, "Could not load data availability");

	// make sure data availability information produced by second scardac run
	// is identical except for the each extent's lastScan time
	ASSERT_EQUAL_MSG(daExpected->dataExtentCount(), daFound->dataExtentCount(),
	                 "Different number of DataExtents in second application "
	                 "run");
	for ( size_t iExt = 0; iExt < daExpected->dataExtentCount(); ++iExt ) {
		auto *extExpected = daExpected->dataExtent(iExt);
		auto *extFound = daFound->dataExtent(iExt);
		reader->load(extFound);
		BOOST_CHECK(extExpected->waveformID() == extFound->waveformID());
		CHECK_EQUAL_MSG(extExpected->start().iso(), extFound->start().iso(), ctx);
		CHECK_EQUAL_MSG(extExpected->end().iso(), extFound->end().iso(), ctx);
		CHECK_EQUAL_MSG(extExpected->updated().iso(), extFound->updated().iso(), ctx);
		CHECK_EQUAL_MSG(extExpected->segmentOverflow(), extFound->segmentOverflow(), ctx);
		BOOST_CHECK(extExpected->lastScan() < extFound->lastScan());
		ASSERT_EQUAL_MSG(extExpected->dataAttributeExtentCount(),
		                 extFound->dataAttributeExtentCount(), ctx);
		ASSERT_EQUAL_MSG(extExpected->dataSegmentCount(),
		                 extFound->dataSegmentCount(), ctx << ": Different number of DataSegments");

		for ( size_t iAttExt = 0; iAttExt < extExpected->dataAttributeExtentCount(); ++iAttExt ) {
			auto *attExtExpected = extExpected->dataAttributeExtent(iAttExt);
			auto *attExtFound = extFound->dataAttributeExtent(iAttExt);
			if ( *attExtExpected != *attExtFound ) {
				BOOST_ERROR(ctx << ": AttributeExtent missmatch at index " << iAttExt);
				BOOST_CHECK_EQUAL(attExtExpected->start().iso(), attExtFound->start().iso());
				BOOST_CHECK_EQUAL(attExtExpected->end().iso(), attExtFound->end().iso());
				BOOST_CHECK_EQUAL(attExtExpected->updated().iso(), attExtFound->updated().iso());
				BOOST_CHECK_EQUAL(attExtExpected->segmentCount(), attExtFound->segmentCount());
			}
		}

		for ( size_t iSeg = 0; iSeg < extExpected->dataSegmentCount(); ++iSeg ) {
			auto *segExpected = extExpected->dataSegment(iSeg);
			auto *segFound = extFound->dataSegment(iSeg);
			if  (*segExpected != *segFound) {
				BOOST_ERROR(ctx << ": DataSegment missmatch at index " << iSeg);
				BOOST_CHECK_EQUAL(segExpected->start().iso(), segFound->start().iso());
				BOOST_CHECK_EQUAL(segExpected->end().iso(), segFound->end().iso());
				BOOST_CHECK_EQUAL(segExpected->updated().iso(), segFound->updated().iso());
				BOOST_CHECK_EQUAL(segExpected->sampleRate(), segFound->sampleRate());
				BOOST_CHECK_EQUAL(segExpected->quality(), segFound->quality());
				BOOST_CHECK_EQUAL(segExpected->outOfOrder(), segFound->outOfOrder());
			}
		}
	}

	return daFound;
}

void copy(const string &src, const string &dst, bool contentOnly = false,
          bool createPathToDst = false) {
	try {
		SC_FS_DECLARE_PATH(from, src);
		SC_FS_DECLARE_PATH(to, dst);

		ASSERT_MSG(fs::exists(from),
		           "Could not copy " << src << " to " << dst <<
		           ": Source does not exist");

		if ( fs::is_directory(from) ) {
			if ( !contentOnly && SC_FS_FILE_NAME(from) != SC_FS_FILE_NAME(to) ) {
				to = to / SC_FS_FILE_PATH(from);
			}

			if ( !fs::exists(to) ) {
				createPathToDst ? fs::create_directories(to) :
				                  fs::create_directory(to);
			}

			fs::directory_iterator end_it;
			for ( fs::directory_iterator it(from); it != end_it; ++it ) {
				copy(SC_FS_DE_PATH(it).string(),
				     (to / SC_FS_FILE_PATH(SC_FS_DE_PATH(it))).string());
			}
		}
		else {
			if ( createPathToDst ) {
				// create path to destination file if it does not exists
				if ( SC_FS_HAS_PARENT_PATH(to) &&
				     !fs::exists(SC_FS_PARENT_PATH(to)) ) {
					fs::create_directories(SC_FS_PARENT_PATH(to));
				}
			}

			fs::copy_file(from, to);
		}
	} catch ( exception &e ) {
		ASSERT_MSG(false, "Could not copy: " << e.what());
	}
}

DEFINE_SMARTPOINTER(TestSCARDAC);

DataAvailability::SCARDAC *createApp(const vector<string> &args) {
	char **argv = new char*[args.size()];
	for ( size_t i = 0; i < args.size(); ++i ) {
		size_t size = args[i].size() + 1;
		argv[i] = new char[size];
		memset(argv[i], 0, size);
		strncpy(argv[i], args[i].c_str(), size);
	}

	auto *app = new DataAvailability::SCARDAC(args.size(), argv);
	delete[] argv;
	return app;
}


DataModel::DatabaseReader *createReader(const string &dbURI) {
	System::PluginRegistry::Instance()->addPluginName("dbsqlite3");
	System::PluginRegistry::Instance()->loadPlugins();

	IO::DatabaseInterface* db = IO::DatabaseInterface::Open(dbURI.c_str());
	ASSERT_MSG(db, "Could not connect to database: " << dbURI);

	return new DataModel::DatabaseReader(db);
}

DataModel::DatabaseReader *runApp(const string &dbURI, const vector<string> &args) {
	DataAvailability::SCARDACPtr app = createApp(args);
	int res = app->exec();
	BOOST_CHECK_MESSAGE(!res, "Application exited with error code: " << res);

	return createReader(dbURI);
}

struct F {
	F() {
		BOOST_TEST_MESSAGE("Creating Fixture");

		const auto &testName = boost::unit_test::framework::current_test_case().p_name;
		auto *env = Environment::Instance();

		// copy empty but initialized database
		auto dbSrcPath = env->installDir() + "/seiscomp.db";
		auto dbDstPath = env->configDir() + "/seiscomp.db";

		BOOST_TEST_MESSAGE("Setup test database: " << dbDstPath);

		std::ifstream src(dbSrcPath, std::ios::binary);
		std::ofstream dst(dbDstPath, std::ios::binary);
		dst << src.rdbuf();

		// create database reader to validate results
		dbURI = "sqlite3://" + dbDstPath;

		// copy test archive
		auto archiveSrcDir = env->installDir();
		archiveSrcDir += "/archive/";
		archiveSrcDir += testName;
		archiveDir = env->configDir();
		BOOST_ASSERT_MSG(!archiveDir.empty(), "Local config dir not set");
		archiveDir += "/archive";

		BOOST_TEST_MESSAGE("Setup SDS archive from: " << archiveSrcDir);

		SC_FS_DECLARE_PATH(path, archiveDir);
		if ( fs::exists(path) ) {
			fs::remove_all(path);
		}
		copy(archiveSrcDir, archiveDir, true);

		appName = string("scardac-");
		appName += testName;
		BOOST_TEST_MESSAGE("Fixture created");
	}

	~F() {
		BOOST_TEST_MESSAGE("Fixture destroyed");
	}

	string archiveDir;
	string appName;
	string dbURI;
};



BOOST_FIXTURE_TEST_SUITE(base_main_scardac, F)
//BOOST_AUTO_TEST_SUITE(empty, * utf::fixture<DBFixture>())
//BOOST_FIXTURE_TEST_SUITE(empty, DBFixture)
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>




//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
BOOST_AUTO_TEST_CASE(empty) {
	DataModel::DatabaseReaderPtr reader = createReader(dbURI);
	checkObjectCount(reader, "empty archive");
}
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>



//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
BOOST_AUTO_TEST_CASE(basic) {
	auto mseedFile = archiveDir + "/2019/AM/R0F05/SHZ.D/AM.R0F05.00.SHZ.D.2019.214";

	Core::Time updated;
	auto mtime = boost::filesystem::last_write_time(mseedFile);
	if ( mtime >= 0 ) {
		updated = mtime;
	}

	DataModel::DatabaseReaderPtr reader;

	BOOST_TEST_MESSAGE("initial scan");

	// run scardac, measure execution time window
	Core::TimeWindow tw;
	tw.setStartTime(Core::Time::UTC());
	reader = runApp(dbURI, {appName});
	tw.setEndTime(Core::Time::UTC());

	DataModel::DataAvailabilityPtr da = reader->loadDataAvailability();
	BOOST_ASSERT_MSG(da, "Could not load data availability");

	// extent
	BOOST_ASSERT_MSG(da->dataExtentCount(), "No extent found");
	auto *ext = da->dataExtent(0);
	reader->load(ext);
	BOOST_CHECK_EQUAL(ext->dataAttributeExtentCount(), 1);
	BOOST_CHECK_EQUAL(ext->dataSegmentCount(), 1);
	auto &wfid = ext->waveformID();
	BOOST_CHECK_EQUAL(wfid.networkCode(), "AM");
	BOOST_CHECK_EQUAL(wfid.stationCode(), "R0F05");
	BOOST_CHECK_EQUAL(wfid.locationCode(), "00");
	BOOST_CHECK_EQUAL(wfid.channelCode(), "SHZ");
	BOOST_CHECK_EQUAL(ext->start().iso(), "2019-08-02T17:59:58.217999Z");
	BOOST_CHECK_EQUAL(ext->end().iso(), "2019-08-02T18:01:04.837999Z");
	BOOST_CHECK_EQUAL(ext->updated().iso(), updated.iso());
	BOOST_CHECK_MESSAGE(tw.contains(ext->lastScan()),
	                    "last scan time not in execution time window");
	BOOST_CHECK_MESSAGE(!ext->segmentOverflow(), "unexpected segment overflow");

	// attribute extent
	auto *attExt = ext->dataAttributeExtent(0);
	BOOST_CHECK_EQUAL(attExt->start().iso(), ext->start().iso());
	BOOST_CHECK_EQUAL(attExt->end().iso(), ext->end().iso());
	BOOST_CHECK_EQUAL(attExt->sampleRate(), 50.0);
	BOOST_CHECK_EQUAL(attExt->quality(), "D");
	BOOST_CHECK_EQUAL(attExt->updated().iso(), ext->updated().iso());
	BOOST_CHECK_EQUAL(attExt->segmentCount(), ext->dataSegmentCount());

	// segment
	auto *seg = ext->dataSegment(0);
	BOOST_CHECK_EQUAL(seg->start().iso(), ext->start().iso());
	BOOST_CHECK_EQUAL(seg->end().iso(), ext->end().iso());
	BOOST_CHECK_EQUAL(seg->sampleRate(), attExt->sampleRate());
	BOOST_CHECK_EQUAL(seg->quality(), attExt->quality());
	BOOST_CHECK_EQUAL(seg->updated().iso(), ext->updated().iso());
	BOOST_CHECK(!seg->outOfOrder());

	// run scardac a second time on an exiting database
	string ctx("rescan: mtime");
	BOOST_TEST_MESSAGE(ctx);
	reader = runApp(dbURI, {appName});
	checkEqual(reader, da, ctx);

	ctx = "rescan: deep-scan";
	BOOST_TEST_MESSAGE(ctx);
	reader = runApp(dbURI, {appName, "--deep-scan"});
	checkEqual(reader, da, ctx);

	ctx = "rescan: empty archive, stream ignored ";
	BOOST_TEST_MESSAGE(ctx);
	ASSERT_MSG(fs::remove(SC_FS_PATH(mseedFile)),
	           "Could not remove: " << mseedFile);
	reader = runApp(dbURI, {
	        appName, "--deep-scan",
	        "--exclude", "AM.*",
	        "--include", "GE.*"
	});
	checkObjectCount(reader, ctx, 1, 1, 1);

	ctx = "rescan: empty archive, stream ignored (2) ";
	BOOST_TEST_MESSAGE(ctx);
	reader = runApp(dbURI, {
	        appName, "--deep-scan",
	        "--include", "AM.*.SHX"
	});
	checkObjectCount(reader, ctx, 1, 1, 1);

	ctx = "rescan: empty archive, stream matched ";
	BOOST_TEST_MESSAGE(ctx);
	reader = runApp(dbURI, {
	        appName, "--deep-scan",
	        "--include", "AM.*.SHZ"
	});
	checkObjectCount(reader, ctx);
}
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>



//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
BOOST_AUTO_TEST_CASE(nslc) {
	auto mseedFile = archiveDir + "/2019/AM/R0F05/SHZ.D/AM.R0F05.00.SHZ.D.2019.214";
	auto nslcFile = archiveDir + "/nslc.txt";

	Core::Time updated;
	auto mtime = boost::filesystem::last_write_time(mseedFile);
	if ( mtime >= 0 ) {
		updated = mtime;
	}

	DataModel::DatabaseReaderPtr reader;

	BOOST_TEST_MESSAGE("initial scan");

	// run scardac, measure execution time window
	Core::TimeWindow tw;
	tw.setStartTime(Core::Time::UTC());
	reader = runApp(dbURI, {appName, "--nslc", nslcFile});
	tw.setEndTime(Core::Time::UTC());

	DataModel::DataAvailabilityPtr da = reader->loadDataAvailability();
	BOOST_ASSERT_MSG(da, "Could not load data availability");

	// extent
	BOOST_ASSERT_MSG(da->dataExtentCount(), "No extent found");
	auto *ext = da->dataExtent(0);
	reader->load(ext);
	BOOST_CHECK_EQUAL(ext->dataAttributeExtentCount(), 1);
	BOOST_CHECK_EQUAL(ext->dataSegmentCount(), 1);
	auto &wfid = ext->waveformID();
	BOOST_CHECK_EQUAL(wfid.networkCode(), "AM");
	BOOST_CHECK_EQUAL(wfid.stationCode(), "R0F05");
	BOOST_CHECK_EQUAL(wfid.locationCode(), "00");
	BOOST_CHECK_EQUAL(wfid.channelCode(), "SHZ");
	BOOST_CHECK_EQUAL(ext->start().iso(), "2019-08-02T17:59:58.217999Z");
	BOOST_CHECK_EQUAL(ext->end().iso(), "2019-08-02T18:01:04.837999Z");
	BOOST_CHECK_EQUAL(ext->updated().iso(), updated.iso());
	BOOST_CHECK_MESSAGE(tw.contains(ext->lastScan()),
	                    "last scan time not in execution time window");
	BOOST_CHECK_MESSAGE(!ext->segmentOverflow(), "unexpected segment overflow");

	// attribute extent
	auto *attExt = ext->dataAttributeExtent(0);
	BOOST_CHECK_EQUAL(attExt->start().iso(), ext->start().iso());
	BOOST_CHECK_EQUAL(attExt->end().iso(), ext->end().iso());
	BOOST_CHECK_EQUAL(attExt->sampleRate(), 50.0);
	BOOST_CHECK_EQUAL(attExt->quality(), "D");
	BOOST_CHECK_EQUAL(attExt->updated().iso(), ext->updated().iso());
	BOOST_CHECK_EQUAL(attExt->segmentCount(), ext->dataSegmentCount());

	// segment
	auto *seg = ext->dataSegment(0);
	BOOST_CHECK_EQUAL(seg->start().iso(), ext->start().iso());
	BOOST_CHECK_EQUAL(seg->end().iso(), ext->end().iso());
	BOOST_CHECK_EQUAL(seg->sampleRate(), attExt->sampleRate());
	BOOST_CHECK_EQUAL(seg->quality(), attExt->quality());
	BOOST_CHECK_EQUAL(seg->updated().iso(), ext->updated().iso());
	BOOST_CHECK(!seg->outOfOrder());
}
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>




//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
BOOST_AUTO_TEST_CASE(multiday) {
	auto basePath = archiveDir + "/2023/AM/R0F05/SHZ.D/AM.R0F05.00.SHZ.D.2023.";
	auto mseedFile0 = basePath + "239";
	auto mseedFile1 = basePath + "240";
	auto mseedFile2 = basePath + "241";
	auto mseedFile3 = basePath + "242";

	Core::Time updated0;
	Core::Time updated1;
	auto mtime = boost::filesystem::last_write_time(mseedFile0);
	if ( mtime >= 0) {
		updated0 = mtime;
	}
	mtime = boost::filesystem::last_write_time(mseedFile1);
	if ( mtime >= 0) {
		updated1 = mtime;
	}

	string ctx("initial scan");
	BOOST_TEST_MESSAGE(ctx);

	DataModel::DatabaseReaderPtr reader = runApp(dbURI, {appName});

	DataModel::DataAvailabilityPtr da = reader->loadDataAvailability();
	BOOST_ASSERT_MSG(da, "Could not load data availability");

	// extent
	BOOST_ASSERT_MSG(da->dataExtentCount(), "No extent found");
	auto *ext = da->dataExtent(0);
	reader->load(ext);
	BOOST_CHECK_EQUAL(ext->dataAttributeExtentCount(), 1);
	BOOST_CHECK_EQUAL(ext->dataSegmentCount(), 3);
	auto &wfid = ext->waveformID();
	BOOST_CHECK_EQUAL(wfid.networkCode(), "AM");
	BOOST_CHECK_EQUAL(wfid.stationCode(), "R0F05");
	BOOST_CHECK_EQUAL(wfid.locationCode(), "00");
	BOOST_CHECK_EQUAL(wfid.channelCode(), "SHZ");
	BOOST_CHECK_EQUAL(ext->start().iso(), "2023-08-27T22:59:56.941Z");
	BOOST_CHECK_EQUAL(ext->end().iso(), "2023-08-30T01:00:05.581Z");
	BOOST_CHECK_MESSAGE(!ext->segmentOverflow(), "unexpected segment overflow");

	// attribute extent
	auto *attExt = ext->dataAttributeExtent(0);
	BOOST_CHECK_EQUAL(attExt->start().iso(), ext->start().iso());
	BOOST_CHECK_EQUAL(attExt->end().iso(), ext->end().iso());
	BOOST_CHECK_EQUAL(attExt->sampleRate(), 50.0);
	BOOST_CHECK_EQUAL(attExt->quality(), "D");
	BOOST_CHECK_EQUAL(attExt->updated().iso(), ext->updated().iso());
	BOOST_CHECK_EQUAL(attExt->segmentCount(), ext->dataSegmentCount());

	// segment
	auto *seg0 = ext->dataSegment(0);
	BOOST_CHECK_EQUAL(seg0->start().iso(), ext->start().iso());
	BOOST_CHECK_EQUAL(seg0->end().iso(), "2023-08-28T00:00:05.641Z");
	BOOST_CHECK_EQUAL(seg0->sampleRate(), attExt->sampleRate());
	BOOST_CHECK_EQUAL(seg0->quality(), attExt->quality());
	BOOST_CHECK_EQUAL(seg0->updated().iso(), updated0.iso());
	BOOST_CHECK(!seg0->outOfOrder());

	auto *seg1 = ext->dataSegment(1);
	BOOST_CHECK_EQUAL(seg1->start().iso(), "2023-08-28T00:00:05.901Z");
	BOOST_CHECK_EQUAL(seg1->end().iso(), "2023-08-28T01:09:43.661Z");
	BOOST_CHECK_EQUAL(seg1->sampleRate(), attExt->sampleRate());
	BOOST_CHECK_EQUAL(seg1->quality(), attExt->quality());
	BOOST_CHECK_EQUAL(seg1->updated().iso(), updated1.iso());
	BOOST_CHECK(!seg1->outOfOrder());

	auto *seg2 = ext->dataSegment(2);
	BOOST_CHECK_EQUAL(seg2->start().iso(), "2023-08-28T01:09:48.901Z");
	BOOST_CHECK_EQUAL(seg2->end().iso(), ext->end().iso());
	BOOST_CHECK_EQUAL(seg2->sampleRate(), attExt->sampleRate());
	BOOST_CHECK_EQUAL(seg2->quality(), attExt->quality());
	BOOST_CHECK_EQUAL(seg2->updated().iso(), ext->updated().iso());
	BOOST_CHECK(!seg2->outOfOrder());

	auto oneSecond = Core::TimeSpan(1, 0);

	// touch mseedFile0
	ctx = "touch mseedFile0";
	BOOST_TEST_MESSAGE(ctx);
	auto lastScan = ext->lastScan();
	lastScan.setUSecs(0);
	auto future = lastScan + oneSecond;
	boost::filesystem::last_write_time(mseedFile0, future);
	reader = runApp(dbURI, {appName});
	ext->setUpdated(future);
	attExt->setUpdated(future);
	seg0->setUpdated(future);
	auto daFound = checkEqual(reader, da, ctx);

	// touch mseedFile1
	ctx = "touch mseedFile1";
	BOOST_TEST_MESSAGE(ctx);
	lastScan = daFound->dataExtent(0)->lastScan();
	lastScan.setUSecs(0);
	future = lastScan + oneSecond;
	auto past = lastScan - oneSecond;
	boost::filesystem::last_write_time(mseedFile0, past);
	boost::filesystem::last_write_time(mseedFile1, future);
	boost::filesystem::last_write_time(mseedFile2, past);
	boost::filesystem::last_write_time(mseedFile3, past);
	reader = runApp(dbURI, {appName});
	ext->setUpdated(future);
	attExt->setUpdated(future);
	seg1->setUpdated(future);
	seg2->setUpdated(future);
	daFound = checkEqual(reader, da, ctx);

	// touch mseedFile2
	ctx = "touch mseedFile2";
	BOOST_TEST_MESSAGE(ctx);
	lastScan = daFound->dataExtent(0)->lastScan();
	lastScan.setUSecs(0);
	future = lastScan + oneSecond;
	past = lastScan - oneSecond;
	boost::filesystem::last_write_time(mseedFile0, past);
	boost::filesystem::last_write_time(mseedFile1, past);
	boost::filesystem::last_write_time(mseedFile2, future);
	boost::filesystem::last_write_time(mseedFile3, past);
	reader = runApp(dbURI, {appName});
	ext->setUpdated(future);
	attExt->setUpdated(future);
	seg2->setUpdated(future);
	daFound = checkEqual(reader, da, ctx);

	// touch mseedFile3
	ctx = "touch mseedFile3";
	BOOST_TEST_MESSAGE(ctx);
	lastScan = daFound->dataExtent(0)->lastScan();
	lastScan.setUSecs(0);
	future = lastScan + oneSecond;
	past = lastScan - oneSecond;
	boost::filesystem::last_write_time(mseedFile0, past);
	boost::filesystem::last_write_time(mseedFile1, past);
	boost::filesystem::last_write_time(mseedFile2, past);
	boost::filesystem::last_write_time(mseedFile3, future);
	reader = runApp(dbURI, {appName});
	ext->setUpdated(future);
	attExt->setUpdated(future);
	seg2->setUpdated(future);
	daFound = checkEqual(reader, da, ctx);

	// remove mseedFile2: seg2 is split into 2 segments
	ctx = "remove mseedFile2";
	BOOST_TEST_MESSAGE(ctx);


	SC_FS_DECLARE_PATH(path, mseedFile2);
	SC_FS_DECLARE_PATH(pathTmp, mseedFile2 + ".tmp");
	fs::rename(path, pathTmp);

	DataModel::DataSegmentPtr seg2Original = seg2;
	DataModel::DataSegmentPtr seg2a = new DataModel::DataSegment();
	DataModel::DataSegmentPtr seg2b = new DataModel::DataSegment();
	*seg2a = *seg2;
	*seg2b = *seg2;

	seg1->setUpdated(past);
	seg2a->setUpdated(past);
	seg2a->setEnd(Core::Time(2023, 8, 29, 0, 0, 5, 801000));
	seg2b->setStart(Core::Time(2023, 8, 30, 0, 0, 2, 881000));

	ext->removeDataSegment(2);
	ext->add(seg2a.get());
	ext->add(seg2b.get());
	attExt->setSegmentCount(4);

	reader = runApp(dbURI, {appName});
	daFound = checkEqual(reader, da, ctx);

	// restore mseedFile2: seg2a and seg2b is merged into seg2 again
	ctx = "restore mseedFile2";
	BOOST_TEST_MESSAGE(ctx);
	// make sure last file is not read
	boost::filesystem::last_write_time(mseedFile3, past);
	fs::rename(pathTmp, path);
	// file must be touched
	lastScan = daFound->dataExtent(0)->lastScan() + Core::TimeSpan(1, 0);
	lastScan.setUSecs(0);
	boost::filesystem::last_write_time(mseedFile2, lastScan);

	ext->removeDataSegment(2);
	ext->removeDataSegment(2);
	ext->add(seg2Original.get());
	attExt->setSegmentCount(3);
	ext->setUpdated(lastScan);
	attExt->setUpdated(lastScan);
	seg2Original->setUpdated(lastScan);

	reader = runApp(dbURI, {appName});
	daFound = checkEqual(reader, da, ctx);
}
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>




//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
BOOST_AUTO_TEST_CASE(timewindow) {
	auto basePath = archiveDir + "/2023/AM/R0F05/SHZ.D/AM.R0F05.00.SHZ.D.2023.";
	auto mseedFile0 = basePath + "239";
	auto mseedFile1 = basePath + "240";
	auto mseedFile2 = basePath + "241";
	auto mseedFile3 = basePath + "242";

	string ctx("end before first chunk");
	BOOST_TEST_MESSAGE(ctx);
	DataModel::DatabaseReaderPtr reader;
	reader = runApp(dbURI, {appName, "--end", "2023-08-26"});
	checkObjectCount(reader, ctx);

	ctx = "end before first record";
	BOOST_TEST_MESSAGE(ctx);
	reader = runApp(dbURI, {appName, "--end", "2023-08-27T22:59:00"});
	checkObjectCount(reader, ctx);

	ctx = "end on first record start";
	BOOST_TEST_MESSAGE(ctx);
	reader = runApp(dbURI, {appName, "--end", "2023-08-27T22:59:56.941"});
	checkObjectCount(reader, ctx);

	ctx = "start after last chunk";
	BOOST_TEST_MESSAGE(ctx);
	reader = runApp(dbURI, {appName, "--start", "2023-08-31"});
	checkObjectCount(reader, ctx);

	ctx = "start after last record";
	BOOST_TEST_MESSAGE(ctx);
	reader = runApp(dbURI, {appName, "--start", "2023-08-30T01:00:06"});
	checkObjectCount(reader, ctx);

	ctx = "end within first segment";
	BOOST_TEST_MESSAGE(ctx);
	reader = runApp(dbURI, {appName, "--end", "2023-08-27T23:00:00"});
	checkObjectCount(reader, ctx, 1, 1, 1);

	ctx = "start within last record";
	BOOST_TEST_MESSAGE(ctx);
	reader = runApp(dbURI, {
	    appName,
	    "--start", "2023-08-30T01:00:05",
	    "--modified-since", "1"
	});
	checkObjectCount(reader, ctx, 1, 1, 2);

	ctx = "entire archive";
	BOOST_TEST_MESSAGE(ctx);
	reader = runApp(dbURI, { appName, "--deep-scan"});
	checkObjectCount(reader, ctx, 1, 1, 3);

//	// extent
//	BOOST_ASSERT_MSG(da->dataExtentCount(), "No extent found");
//	auto *ext = da->dataExtent(0);
//	reader->load(ext);
//	BOOST_CHECK_EQUAL(ext->dataAttributeExtentCount(), 1);
//	BOOST_CHECK_EQUAL(ext->dataSegmentCount(), 3);
//	auto &wfid = ext->waveformID();
//	BOOST_CHECK_EQUAL(wfid.networkCode(), "AM");
//	BOOST_CHECK_EQUAL(wfid.stationCode(), "R0F05");
//	BOOST_CHECK_EQUAL(wfid.locationCode(), "00");
//	BOOST_CHECK_EQUAL(wfid.channelCode(), "SHZ");
//	BOOST_CHECK_EQUAL(ext->start().iso(), "2023-08-27T22:59:56.941Z");
//	BOOST_CHECK_EQUAL(ext->end().iso(), "2023-08-30T01:00:05.581Z");
//	BOOST_CHECK_MESSAGE(!ext->segmentOverflow(), "unexpected segment overflow");

//	// attribute extent
//	auto *attExt = ext->dataAttributeExtent(0);
//	BOOST_CHECK_EQUAL(attExt->start().iso(), ext->start().iso());
//	BOOST_CHECK_EQUAL(attExt->end().iso(), ext->end().iso());
//	BOOST_CHECK_EQUAL(attExt->sampleRate(), 50.0);
//	BOOST_CHECK_EQUAL(attExt->quality(), "D");
//	BOOST_CHECK_EQUAL(attExt->updated().iso(), ext->updated().iso());
//	BOOST_CHECK_EQUAL(attExt->segmentCount(), ext->dataSegmentCount());

//	// segment
//	auto *seg0 = ext->dataSegment(0);
//	BOOST_CHECK_EQUAL(seg0->start().iso(), ext->start().iso());
//	BOOST_CHECK_EQUAL(seg0->end().iso(), "2023-08-28T00:00:05.641Z");
//	BOOST_CHECK_EQUAL(seg0->sampleRate(), attExt->sampleRate());
//	BOOST_CHECK_EQUAL(seg0->quality(), attExt->quality());
//	BOOST_CHECK_EQUAL(seg0->updated().iso(), updated0.iso());
//	BOOST_CHECK(!seg0->outOfOrder());
}
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>




//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
BOOST_AUTO_TEST_CASE(streamfilter) {
	BOOST_TEST_MESSAGE("initial scan");

	// run scardac
	DataModel::DatabaseReaderPtr reader;
	reader = runApp(dbURI, {appName, "--exclude", "AA.*"});

	DataModel::DataAvailabilityPtr da = reader->loadDataAvailability();
	BOOST_ASSERT_MSG(da, "Could not load data availability");

	// extent
	BOOST_ASSERT_MSG(da->dataExtentCount(), "No extent found");
	auto *ext = da->dataExtent(0);
	reader->load(ext);
	BOOST_CHECK_EQUAL(ext->dataAttributeExtentCount(), 1);
	BOOST_CHECK_EQUAL(ext->dataSegmentCount(), 1);
	auto &wfid = ext->waveformID();
	BOOST_CHECK_EQUAL(wfid.networkCode(), "AM");
	BOOST_CHECK_EQUAL(wfid.stationCode(), "R0F05");
	BOOST_CHECK_EQUAL(wfid.locationCode(), "00");
	BOOST_CHECK_EQUAL(wfid.channelCode(), "SHZ");
}
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>




//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
BOOST_AUTO_TEST_SUITE_END()
