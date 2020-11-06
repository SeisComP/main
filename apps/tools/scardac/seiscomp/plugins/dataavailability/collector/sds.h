/***************************************************************************
 * Copyright (C) 2020 by gempa GmbH                                        *
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

#ifndef SEISCOMP_DATAAVAILABILITY_COLLECTOR_SDS_H
#define SEISCOMP_DATAAVAILABILITY_COLLECTOR_SDS_H

#include <seiscomp/plugins/dataavailability/collector.h>

#include <seiscomp/core/defs.h>
#include <seiscomp/core/record.h>
#include <seiscomp/core/datetime.h>

#include <seiscomp/datamodel/dataextent.h>
#include <seiscomp/datamodel/datasegment.h>
#include <seiscomp/datamodel/waveformstreamid.h>

#include <seiscomp/io/recordinput.h>
#include <seiscomp/io/recordstream/file.h>

#include <boost/filesystem/path.hpp>

#include <vector>


namespace Seiscomp {
namespace DataAvailability {


DEFINE_SMARTPOINTER(SDSCollector);


class SDSCollector : public Collector {

	public:
		class RecordIterator : public Collector::RecordIterator {
			public:
				/**
				 * @brief RecordIterator
				 * @param file The absolute file path
				 * @param wid StreamID the file is expected to contain data for
				 */
				RecordIterator(const std::string &file,
				               const DataModel::WaveformStreamID &wid);

			public:
				~RecordIterator() override = default;

				bool valid() const override;
				bool next() override;
				const Core::Time& startTime() const override;
				const Core::Time& endTime() const override;
				double sampleRate() const override;
				const std::string& quality() const override;

			protected:
				std::string         _file;
				std::string         _sid;
				RecordStream::File  _stream;
				IO::RecordInput     _input;

				RecordPtr           _rec;
				Core::Time          _endTime;
				std::string         _quality;
		};


	public:
		SDSCollector() = default;
		~SDSCollector() override = default;

		bool setSource(const char *source) override;
		void reset() override;
		void collectWaveformIDs(WaveformIDs &wids) override;
		void collectChunks(DataChunks &chunks,
		                   const DataModel::WaveformStreamID &wid) override;
		bool chunkTimeWindow(Core::TimeWindow &window,
		                     const std::string &chunk) override;
		Core::Time chunkMTime(const std::string &chunk) override;
		Collector::RecordIterator* begin(
		        const std::string &chunk,
		        const DataModel::WaveformStreamID &wid) override;
		bool threadSafe() const override;

	protected:

		/**
		 * @brief scanDirectories Recursively scans archive for available
		 * WaveformIDs.
		 * @param wids The collected waveform ids
		 * @param dir Current directory path.
		 * @param depth Remaining directory depth. If 0 the current directory
		 * is scanned for files.
		 */
		virtual void scanDirectory(WaveformIDs &wids,
		                           const boost::filesystem::path &dir,
		                           uint16_t depth = 3);

		/**
		 * @brief scanFiles Scans a directory for available WaveformIDs.
		 * @param wids The collected waveform ids
		 * @param dir The directory path in which to search files
		 */
		virtual void scanFiles(WaveformIDs &wids,
		                       const boost::filesystem::path &dir);

		/**
		 * @brief fileStreamID Returns the stream ID for a given file name
		 * @param filename The file name to extract the stream ID from
		 * @return The stream ID or an empty string if the file names does not
		 * match the expected pattern.
		 */
		virtual std::string fileStreamID(const std::string &filename);

	protected:
		boost::filesystem::path              _basePath;
		std::vector<boost::filesystem::path> _archiveYears;
};

} // ns DataAvailability
} // ns Seiscomp

#endif // SEISCOMP_DATAAVAILABILITY_COLLECTOR_SDS_H
