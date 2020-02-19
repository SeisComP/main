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


#include "plugin.h"
#include "mainwidget.h"
#include <seiscomp/gui/core/application.h>


namespace Seiscomp {
namespace Gui {
namespace MessageMonitor {


Q_EXPORT_PLUGIN2(mm_preliminaryorigin, PreliminaryOrigin)


PreliminaryOrigin::PreliminaryOrigin() {
}


const char* PreliminaryOrigin::name() const {
	return "preliminary origin";
}


QWidget* PreliminaryOrigin::create(QWidget* parent, QObject* signaller) const {
	return new MainWidget(parent, signaller, SCApp->mapsDesc());
}


}
}
}
