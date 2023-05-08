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


#ifndef SEISCOMP_APPLICATIONS_MNCURSESPLUGIN_H
#define SEISCOMP_APPLICATIONS_MNCURSESPLUGIN_H


#include <seiscomp/plugins/monitor/monitoroutplugininterface.h>

#include <mutex>
#include <thread>


namespace Seiscomp {
namespace Applications {


DEFINE_SMARTPOINTER(MNcursesPlugin);


class MNcursesPlugin : public MonitorOutPluginInterface {
	DECLARE_SC_CLASS(MNcursesPlugin);

	// ----------------------------------------------------------------------
	// Nested types
	// ----------------------------------------------------------------------
	public:
		enum TextAttrib { NORMAL = 0x0, HIGHLIGHT, TEXTATTRIB_QUNATITY };

	private:
		typedef std::map<Client::Status::Tag, std::string> Header;
		typedef std::map<Client::Status::Tag, int>         ColumnSizes;
		typedef std::vector<Client::Status::Tag>           TagOrder;


	// ----------------------------------------------------------------------
	// X'struction
	// ----------------------------------------------------------------------
	public:
		MNcursesPlugin();
		virtual ~MNcursesPlugin() {}


	// ----------------------------------------------------------------------
	// Public interface
	// ----------------------------------------------------------------------
	public:
		virtual bool print(const ClientTable& table);
		virtual bool initOut(const Config::Config& cfg);
		virtual bool deinitOut();
		virtual bool refreshOut();
		virtual bool clearOut();


	// ----------------------------------------------------------------------
	// Private interface
	// ----------------------------------------------------------------------
	private:
		bool print(const std::string &str, TextAttrib attrib = NORMAL);
		bool printTable(ClientTable &table);
		void readUserInput();
		bool init();
		void updateColumnSizes(const ClientTable &table);
		std::string formatLine(ClientInfoData &vec);
		void initDataStructures(Client::Status::Tag key, const std::string &description);
		int findTag(Client::Status::Tag) const;


	// ----------------------------------------------------------------------
	// Private data member
	// ----------------------------------------------------------------------
	private:
		void                *_context;
		Header               _header;
		ColumnSizes          _columnSizes;
		TagOrder             _tagOrder;
		unsigned int         _currentLine;
		Client::Status::Tag  _activeTag;
		bool                 _reverseSortOrder;
		ClientTable          _clientTableCache;
		std::mutex           _dataStructureMutex;
		std::thread         *_inputThread{nullptr};

};


} // namespace Application
} // namespace Seiscomp


#endif
