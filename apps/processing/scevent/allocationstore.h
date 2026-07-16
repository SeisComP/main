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


#ifndef SEISCOMP_APPLICATIONS_EVENTTOOL_ALLOCATIONSTORE_H
#define SEISCOMP_APPLICATIONS_EVENTTOOL_ALLOCATIONSTORE_H


#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/datetime.h>

#include <mutex>
#include <string>
#include <vector>


struct sqlite3;


namespace Seiscomp::Client {


DEFINE_SMARTPOINTER(AllocationStore);

/**
 * @brief Persistent store for eventIDs reserved on behalf of secondary instances by
 * the eventID-sync main instance.
 *
 * The store survives restarts and can hold a much larger set of origin/eventID
 * reservations than the transient in-memory cache. It is backed by a single SQLite
 * table:
 *
 *   allocation(origin_time TEXT, event_id TEXT, origin_id TEXT PRIMARY KEY,
 *              origin_xml BLOB)
 *
 * origin_time is stored as an ISO8601 UTC string so that lexicographic range queries
 * (BETWEEN) are equivalent to chronological range queries.
 *
 * All public methods are internally synchronized, so the store may be used from
 * multiple threads. In practice it is only accessed while the association mutex is
 * held, but the internal guard keeps it self-contained.
 */
class AllocationStore : public Core::BaseObject {
	public:
		struct Row {
			Core::Time  originTime;
			std::string eventID;
			std::string originID;
			std::string originXML;
		};


	public:
		AllocationStore() = default;
		~AllocationStore();

		AllocationStore(const AllocationStore &) = delete;
		AllocationStore &operator=(const AllocationStore &) = delete;

		/**
		 * @brief Opens (and, if necessary, creates) the SQLite database at the given
		 *        path and ensures the schema exists.
		 * @return true on success.
		 */
		bool open(const std::string &path);

		//! Closes the database. Safe to call multiple times.
		void close();

		//! Whether the store is currently open.
		bool isOpen() const;

		/**
		 * @brief Inserts or replaces a reservation. Keyed by originID, so a repeated
		 *        reservation for the same origin updates the row.
		 * @return true on success.
		 */
		bool put(const Core::Time &originTime,
		         const std::string &eventID,
		         const std::string &originID,
		         const std::string &originXML);

		/**
		 * @brief Fast path: returns the eventID stored for the given origin publicID,
		 *        or an empty string if none exists.
		 */
		std::string findByOriginID(const std::string &originID);

		/**
		 * @brief Loads all rows whose origin_time lies within [start, end], ordered by
		 *        origin_time ascending.
		 * @return true on success (even if no rows matched).
		 */
		bool loadRange(const Core::Time &start, const Core::Time &end,
		               std::vector<Row> &out);

		/**
		 * @brief Deletes all rows whose origin_time is strictly older than the given
		 *        time.
		 * @return number of rows deleted, or -1 on error.
		 */
		int prune(const Core::Time &olderThan);


	private:
		bool exec(const char *sql);


	private:
		mutable std::mutex  _mutex;
		sqlite3            *_db{nullptr};
};


}


#endif
