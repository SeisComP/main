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


#ifndef SEISCOMP_GUI_MESSAGEMONITOR_PLUGIN_PRELIMINARYORIGIN_H__
#define SEISCOMP_GUI_MESSAGEMONITOR_PLUGIN_PRELIMINARYORIGIN_H__

#include "../../plugin.h"

namespace Seiscomp {
namespace Gui {
namespace MessageMonitor {


class PreliminaryOrigin : public QObject,
                          public Plugin {

	Q_OBJECT
	Q_INTERFACES(Seiscomp::Gui::MessageMonitor::Plugin)

	public:
		PreliminaryOrigin();

	public:
		const char* name() const;

		QWidget* create(QWidget* parent, QObject* signaller) const;
};

}
}
}

#endif
