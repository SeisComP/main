/***************************************************************************
 * Copyright (C) gempa GmbH                                                *
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


#include <seiscomp/client/application.h>
#include <seiscomp/processing/picker.h>


namespace Seiscomp {
namespace Applications {


class Repicker : public Client::Application {
	// ----------------------------------------------------------------------
	//  X'truction
	// ----------------------------------------------------------------------
	public:
		Repicker(int argc, char **argv);


	// ----------------------------------------------------------------------
	//  Application interface
	// ----------------------------------------------------------------------
	public:
		void printUsage() const override;
		bool validateParameters() override;
		bool init() override;
		bool run() override;


	// ----------------------------------------------------------------------
	//  Private members
	// ----------------------------------------------------------------------
	private:
		struct Settings : AbstractSettings {
			std::string pickerInterface;
			bool        anyPhase{false};
			std::string epFile;

			void accept(SettingsLinker &linker) override;
		};

		Settings              _settings;
		Processing::PickerPtr _picker;
};


}
}
