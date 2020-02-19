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


#ifndef SEISCOMP_QC_QCOUTAGE_H__
#define SEISCOMP_QC_QCOUTAGE_H__

#include <map>

#include <seiscomp/qc/qcprocessor.h>
#include <seiscomp/plugins/qc/qcplugin.h>


namespace Seiscomp {
namespace Applications {
namespace Qc {


DEFINE_SMARTPOINTER(QcPluginOutage);

class QcPluginOutage : public QcPlugin {
    DECLARE_SC_CLASS(QcPluginOutage);

public:
    QcPluginOutage();
    bool init(QcApp* app, QcConfig *cfg, std::string streamID);
    std::string registeredName() const;
    std::vector<std::string> parameterNames() const;
    void update();

private:
    std::map<std::string, Core::Time> _recent;

    bool fillUp(const Processing::QcParameter *qcp);
};

}
}
}
#endif
