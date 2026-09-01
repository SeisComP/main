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


#define SEISCOMP_COMPONENT SCEVENT

#include "allocationstore.h"

#include <seiscomp/logging/log.h>

#include <sqlite3.h>


namespace Seiscomp::Client {


namespace {

// Fixed-format ISO8601 UTC timestamp with microsecond precision. The format is chosen
// so that lexicographic ordering matches chronological ordering, which lets range
// queries use a plain BETWEEN on the text column.
const char *ISO_FORMAT_OUT = "%Y-%m-%dT%H:%M:%S.%6f";
// Parsing uses the variable-width %f so it tolerates values written by other tooling;
// the fixed-width writer above still guarantees sortable output.
const char *ISO_FORMAT_IN = "%Y-%m-%dT%H:%M:%S.%f";

std::string toISO(const Core::Time &t) {
	return t.toString(ISO_FORMAT_OUT);
}

}


AllocationStore::~AllocationStore() {
	close();
}


bool AllocationStore::open(const std::string &path) {
	std::lock_guard<std::mutex> l(_mutex);

	if ( _db ) {
		return true;
	}

	// SQLITE_OPEN_FULLMUTEX: serialize access inside sqlite so the handle is safe to
	// use even if reached from more than one thread.
	int rc = sqlite3_open_v2(
		path.c_str(), &_db,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
		nullptr
	);
	if ( rc != SQLITE_OK ) {
		SEISCOMP_ERROR("AllocationStore: cannot open '%s': %s", path.c_str(),
		               _db ? sqlite3_errmsg(_db) : "out of memory");
		if ( _db ) {
			sqlite3_close(_db);
			_db = nullptr;
		}
		return false;
	}

	// A busy timeout avoids spurious SQLITE_BUSY errors if another process
	// (e.g. an operator inspecting the file) holds a short lock.
	sqlite3_busy_timeout(_db, 5000);

	if ( !exec("PRAGMA journal_mode=WAL;")
	  || !exec("PRAGMA synchronous=NORMAL;")
	  || !exec(
	         "CREATE TABLE IF NOT EXISTS allocation ("
	         "  origin_time TEXT NOT NULL,"
	         "  event_id    TEXT NOT NULL,"
	         "  origin_id   TEXT NOT NULL,"
	         "  origin_xml  BLOB NOT NULL,"
	         "  PRIMARY KEY (origin_id)"
	         ");")
	  || !exec(
	         "CREATE INDEX IF NOT EXISTS idx_allocation_time "
	         "ON allocation(origin_time);") ) {
		sqlite3_close(_db);
		_db = nullptr;
		return false;
	}

	SEISCOMP_INFO("AllocationStore: opened '%s'", path.c_str());
	return true;
}


void AllocationStore::close() {
	std::lock_guard<std::mutex> l(_mutex);
	if ( _db ) {
		sqlite3_close(_db);
		_db = nullptr;
	}
}


bool AllocationStore::isOpen() const {
	std::lock_guard<std::mutex> l(_mutex);
	return _db != nullptr;
}


bool AllocationStore::exec(const char *sql) {
	// Caller holds _mutex.
	char *err = nullptr;
	int rc = sqlite3_exec(_db, sql, nullptr, nullptr, &err);
	if ( rc != SQLITE_OK ) {
		if ( err ) {
			SEISCOMP_ERROR("AllocationStore: SQL error: %s", err);
			sqlite3_free(err);
		}
		else {
			// Corner cases: If _db is NULL, already closed, or otherwise a bad handle
			// err is unmodified. SQLITE_NOMEM is returned if the construction of the
			// err string fails due to memory shortages.

#if SQLITE_VERSION_NUMBER >= 3007015
			SEISCOMP_ERROR("AllocationStore: SQL error: %s",
			               sqlite3_errstr(rc));
#else
			// sqlite3_errstr() was added in SQLite 3.7.15, return numeric code instead
			SEISCOMP_ERROR("AllocationStore: SQL error: code %d", rc);
#endif
		}
		return false;
	}
	return true;
}


bool AllocationStore::put(const Core::Time &originTime,
                          const std::string &eventID,
                          const std::string &originID,
                          const std::string &originXML) {
	std::lock_guard<std::mutex> l(_mutex);
	if ( !_db ) {
		return false;
	}

	const char *sql =
		"INSERT OR REPLACE INTO allocation "
		"(origin_time, event_id, origin_id, origin_xml) "
		"VALUES (?, ?, ?, ?);";

	sqlite3_stmt *stmt = nullptr;
	if ( sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK ) {
		SEISCOMP_ERROR("AllocationStore: prepare(put) failed: %s",
		               sqlite3_errmsg(_db));
		return false;
	}

	const std::string iso = toISO(originTime);
	sqlite3_bind_text(stmt, 1, iso.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, eventID.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, originID.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_blob(stmt, 4, originXML.data(),
	                  static_cast<int>(originXML.size()), SQLITE_TRANSIENT);

	bool ok = sqlite3_step(stmt) == SQLITE_DONE;
	if ( !ok ) {
		SEISCOMP_ERROR("AllocationStore: step(put) failed: %s", sqlite3_errmsg(_db));
	}
	sqlite3_finalize(stmt);
	return ok;
}


std::string AllocationStore::findByOriginID(const std::string &originID) {
	std::lock_guard<std::mutex> l(_mutex);
	if ( !_db ) {
		return {};
	}

	const char *sql =
		"SELECT event_id FROM allocation WHERE origin_id = ? LIMIT 1;";

	sqlite3_stmt *stmt = nullptr;
	if ( sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK ) {
		SEISCOMP_ERROR("AllocationStore: prepare(findByOriginID) failed: %s",
		               sqlite3_errmsg(_db));
		return {};
	}

	sqlite3_bind_text(stmt, 1, originID.c_str(), -1, SQLITE_TRANSIENT);

	std::string eventID;
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		const unsigned char *txt = sqlite3_column_text(stmt, 0);
		if ( txt ) {
			eventID = reinterpret_cast<const char *>(txt);
		}
	}

	sqlite3_finalize(stmt);
	return eventID;
}


bool AllocationStore::loadRange(const Core::Time &start, const Core::Time &end,
                                std::vector<Row> &out) {
	std::lock_guard<std::mutex> l(_mutex);
	if ( !_db ) {
		return false;
	}

	const char *sql =
		"SELECT origin_time, event_id, origin_id, origin_xml "
		"FROM allocation "
		"WHERE origin_time BETWEEN ? AND ? "
		"ORDER BY origin_time ASC;";

	sqlite3_stmt *stmt = nullptr;
	if ( sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK ) {
		SEISCOMP_ERROR("AllocationStore: prepare(loadRange) failed: %s",
		               sqlite3_errmsg(_db));
		return false;
	}

	const std::string isoStart = toISO(start);
	const std::string isoEnd = toISO(end);
	sqlite3_bind_text(stmt, 1, isoStart.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, isoEnd.c_str(), -1, SQLITE_TRANSIENT);

	int rc;
	while ( (rc = sqlite3_step(stmt)) == SQLITE_ROW ) {
		Row row;

		const unsigned char *t = sqlite3_column_text(stmt, 0);
		if ( t ) {
			row.originTime.fromString(
				reinterpret_cast<const char *>(t), ISO_FORMAT_IN);
		}

		const unsigned char *eid = sqlite3_column_text(stmt, 1);
		if ( eid ) {
			row.eventID = reinterpret_cast<const char *>(eid);
		}

		const unsigned char *oid = sqlite3_column_text(stmt, 2);
		if ( oid ) {
			row.originID = reinterpret_cast<const char *>(oid);
		}

		const void *blob = sqlite3_column_blob(stmt, 3);
		int blobLen = sqlite3_column_bytes(stmt, 3);
		if ( blob && blobLen > 0 ) {
			row.originXML.assign(static_cast<const char *>(blob),
			                     static_cast<size_t>(blobLen));
		}

		out.push_back(std::move(row));
	}

	sqlite3_finalize(stmt);

	if ( rc != SQLITE_DONE ) {
		SEISCOMP_ERROR("AllocationStore: step(loadRange) failed: %s",
		               sqlite3_errmsg(_db));
		return false;
	}

	return true;
}


int AllocationStore::prune(const Core::Time &olderThan) {
	std::lock_guard<std::mutex> l(_mutex);
	if ( !_db ) {
		return -1;
	}

	const char *sql = "DELETE FROM allocation WHERE origin_time < ?;";

	sqlite3_stmt *stmt = nullptr;
	if ( sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK ) {
		SEISCOMP_ERROR("AllocationStore: prepare(prune) failed: %s",
		               sqlite3_errmsg(_db));
		return -1;
	}

	const std::string iso = toISO(olderThan);
	sqlite3_bind_text(stmt, 1, iso.c_str(), -1, SQLITE_TRANSIENT);

	int deleted = -1;
	if ( sqlite3_step(stmt) == SQLITE_DONE ) {
		deleted = sqlite3_changes(_db);
	}
	else {
		SEISCOMP_ERROR("AllocationStore: step(prune) failed: %s", sqlite3_errmsg(_db));
	}

	sqlite3_finalize(stmt);
	return deleted;
}


}

