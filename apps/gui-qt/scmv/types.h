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


#ifndef __TYPES_H___
#define __TYPES_H___


#ifndef Q_MOC_RUN
#include <seiscomp/core/enumeration.h>
#endif
#include <QObject>


struct StationGlyphs {
	static const char WARNING  = '!';
	static const char ERROR    = '?';
	static const char DISABLED = 'x';
	static const char DEFAULT  = '\0';
};





template <typename T>
class Singleton : public T {
	private:
		Singleton() {}

	public:
		static T* Instance() {
			static Singleton instance;
			return &instance;
		}
};



class ApplicationStatusImpl {
	public:
		MAKEENUM(
			Mode,
			EVALUES(
				GROUND_MOTION,
				QC
			),
			ENAMES(
				"GOUND_MOTION",
				"QC"
			)
		);

	protected:
		ApplicationStatusImpl()
		 : _mode(GROUND_MOTION),
		   _isTriggering(false) {
		}

	public:
		void setMode(Mode mode) {
			_mode = mode;
		}

		Mode mode() const {
			return _mode;
		}

		void setTriggering(bool triggering) {
			_isTriggering = triggering;
		}

		bool isTriggering() const {
			return _isTriggering;
		}

	private:
		Mode _mode;
		bool _isTriggering;
};




struct QCStatus {
	enum Status {
		NOT_SET = 0x0,
		OK,
		WARNING,
		ERROR,
		StatusCount
	};
};




class QCParameterImpl : public QObject {
	Q_OBJECT

	public:
		MAKEENUM(
			Parameter,
			EVALUES(
				AVAILABILITY,
				GAPS_COUNT,
				DELAY,
				GAPS_INTERVAL,
				GAPS_LENGTH,
				LATENCY,
				OFFSET,
				OVERLAPS_INTERVAL,
				OVERLAPS_LENGTH,
				OVERLAPS_COUNT,
				RMS,
				SPIKES_INTERVAL,
				SPIKES_AMPLITUDE,
				SPIKES_COUNT,
				TIMING_QUALITY
			),
			ENAMES(
				"AVAILABILITY",
				"GAPS_COUNT",
				"DELAY",
				"GAPS_INTERVAL",
				"GAPS_LENGTH",
				"LATENCY",
				"OFFSET",
				"OVERLAPS_INTERVAL",
				"OVERLAPS_LENGTH",
				"OVERLAPS_COUNT",
				"RMS",
				"SPIKES_INTERVAL",
				"SPIKES_AMPLITUDE",
				"SPIKES_COUNT",
				"TIMING_QUALITY"
			)
		);

	signals:
		void parameterChanged(int p);


	public:
		Parameter parameter() {
			return _parameter;
		}

		void setParameter(Parameter parameter) {
			_parameter = parameter;
			emit parameterChanged((int)_parameter);
		}

	protected:
		QCParameterImpl()
		 : _parameter(DELAY) {
		}

	private:
		Parameter _parameter;

};




typedef Singleton<ApplicationStatusImpl> ApplicationStatus;
typedef Singleton<QCParameterImpl> QCParameter;


#endif
