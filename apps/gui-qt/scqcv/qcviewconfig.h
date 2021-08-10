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




#ifndef __QCVIEWCONFIG_H__
#define __QCVIEWCONFIG_H__

#include <QtGui>

#include <seiscomp/gui/core/application.h>
#ifndef Q_MOC_RUN
#include <seiscomp/config/exceptions.h>
#endif

namespace Seiscomp {
namespace Applications {
namespace Qc {

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
class QcViewConfig {

	public:
		QcViewConfig();
		QcViewConfig(Gui::Application *app);
		~QcViewConfig() {};

	private:
		// <colorName, Color>
		typedef QMap<QString, QColor> ColorMap;
		
		typedef struct {
			double min, max;
			int count;
			QString color;
			QString action;
		} Range;
		// <rangeName, Range>
		typedef QMap<QString, Range> RangeMap;
		
		typedef struct {
			RangeMap rangeMap;
			QString color;
		} Score;
		// <scoreName, Score>
		typedef QMap<QString, Score> ScoreMap;
		
		typedef struct {
			RangeMap rangeMap;
			double expire;
			bool useAbsoluteValue;
			QString format;
			QString color;
		} Config;
		// <ConfigName, Config>
		typedef QMap<QString, Config> ConfigMap;
		
		// <ParameterName, ConfigName>
		typedef QMap<QString, QString> ParameterMap;

		// <ParameterName, ConfigName>
		typedef QStringList ParameterList;
	
	private:
		bool init();
		QString formatTimeSpan(double s) const;
		Config readConfig(const std::string& name, Config config);
		Range readRange(const std::string& name, Range range, std::string pre="");
		void readColor(const std::string& name);
 		void readScore(const std::string& name);
//  		Format readFormat(const std::string& name);

	public:
		int streamWidgetLength() const;
		QStringList parameter() const;
		bool cumulative() const;

		int column(const QString& parameterName) const;
		const QString& parameterName(int column) const;

		QColor color(const QString& parameterName, double value) const;
		int count(const QString& parameterName, double value) const;
		QColor sumColor(const QString& scoreName, int score) const;

		QString format(const QString& parameterName, double value) const;
		bool useAbsoluteValue(const QString& parameterName) const;
		bool expired(const QString& parameterName, double dt) const;
// 		Qstring sumAction(const QString& scoreName, int score) const;
// 		Qstring action(const QString& qcParameterName, double value) const;

	private:
		Gui::Application *_app;
		int _streamWidgetLength;
		ParameterMap _parameterMap;
		ParameterList _parameterList;
		ConfigMap _configMap;
		ScoreMap _scoreMap;
		ColorMap	_colorMap;
		bool _cumulative;
		int _formatFloat = 2;
};
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


}
}
}
#endif
