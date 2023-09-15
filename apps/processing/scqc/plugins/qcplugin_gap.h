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


#ifndef SEISCOMP_QC_QCGAP_H__
#define SEISCOMP_QC_QCGAP_H__

#include <seiscomp/plugins/qc/qcplugin.h>

namespace Seiscomp {
namespace Applications {
namespace Qc {


DEFINE_SMARTPOINTER(QcPluginGap);

class QcPluginGap : public QcPlugin {
	DECLARE_SC_CLASS(QcPluginGap);

public:
	QcPluginGap();
	std::string registeredName() const override;
	std::vector<std::string> parameterNames() const override;
	void timeoutTask() override {};

private:
	void generateReport(const QcBuffer* reportBuffer) const override;
	void generateAlert(const QcBuffer* staBuffer, const QcBuffer* ltaBuffer) const override;
	std::vector<double> _mean(const QcBuffer* sta) const;
	std::vector<double> _stdDev(const QcBuffer* sta, double iMean, double aMean) const;
};



}
}
}
#endif
