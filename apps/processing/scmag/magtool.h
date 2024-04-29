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




#ifndef SEISCOMP_MAGTOOL_H
#define SEISCOMP_MAGTOOL_H

#include <string>
#include <map>
#include <set>

#include <seiscomp/datamodel/publicobjectcache.h>
#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/arrival.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/waveformstreamid.h>
#include <seiscomp/datamodel/amplitude.h>
#include <seiscomp/datamodel/stationmagnitude.h>
#include <seiscomp/datamodel/magnitude.h>
#include <seiscomp/processing/magnitudeprocessor.h>
#include <seiscomp/client/application.h>

namespace Seiscomp {
namespace Magnitudes {

class MagTool {
	public:
		MagTool();
		~MagTool();

	public:
		struct SummaryMagnitudeCoefficients {
			OPT(double) a;
			OPT(double) b;

			SummaryMagnitudeCoefficients() {}
			SummaryMagnitudeCoefficients(OPT(double) _a, OPT(double) _b)
			 : a(_a), b(_b) {}
		};

		enum AverageType {
			Default,
			Mean,
			TrimmedMean,
			Median,
			TrimmedMedian,
			MedianTrimmedMean
		};

		struct AverageDescription {
			AverageType type;
			double      parameter;
		};

		typedef std::map<std::string, SummaryMagnitudeCoefficients> Coefficients;
		typedef std::map<std::string, AverageDescription> AverageMethods;
		typedef std::set<std::string> MagnitudeTypes;

	protected:
		// PickID - amplitude association
		typedef std::multimap<std::string, DataModel::AmplitudePtr> StaAmpMap;

		// PickID - origin association
		typedef std::vector<DataModel::OriginPtr> OriginList;
		typedef std::map<std::string, OriginList> OriginMap;

		StaAmpMap  _ampl;
		OriginMap  _orgs;

		DataModel::PublicObjectTimeSpanBuffer _objectCache;

	public:
		void setSummaryMagnitudeEnabled(bool);
		void setSummaryMagnitudeSingleton(bool);
		void setSummaryMagnitudeType(const std::string &type);

		// This is the minimum station magnitude required for any
		// magnitude to contribute to the summary magnitude at all.
		// If this is set to 4 then no magnitude with less than 4
		// station magnitudes is taken into consideration even if this
		// results in no summary magnitude at all. For this reason,
		// the default here is 1 but in a purely automatic system it
		// should be higher, at least 4 is recommended.
		void setSummaryMagnitudeMinStationCount(int nmin);

		void setSummaryMagnitudeBlacklist(const std::vector<std::string> &list);
		void setSummaryMagnitudeWhitelist(const std::vector<std::string> &list);

		void setSummaryMagnitudeDefaultCoefficients(const SummaryMagnitudeCoefficients &);
		void setSummaryMagnitudeCoefficients(const Coefficients &);

		void setAverageMethods(const AverageMethods &);

		void setMinimumArrivalWeight(double);

		bool init(const MagnitudeTypes &mags, const Core::TimeSpan& expiry,
		          bool allowReprocessing, bool staticUpdate, bool keepWeights, double warning);
		void done();

		bool feed(DataModel::Origin*);
		bool feed(DataModel::Pick*);
		bool feed(DataModel::Amplitude*, bool update, bool remove);

		void remove(DataModel::PublicObject *po);


	protected:
		struct MagnitudeEntry {
			MagnitudeEntry() {}
			MagnitudeEntry(Processing::MagnitudeProcessor *p,
			               double v, bool t)
			: proc(p), value(v), passedQC(t) {}

			Processing::MagnitudeProcessor *proc;
			double value;
			bool passedQC;
		};

		typedef std::vector<MagnitudeEntry> MagnitudeList;

		void publicObjectRemoved(DataModel::PublicObject*);
	
		bool _feed(DataModel::Amplitude*, bool update);
	
		// Returns a StationMagnitude for the given Origin, WaveformStreamID
		// and magnitude type. If an instance already exists, it is updated,
		// otherwise a new instance is created.
		DataModel::StationMagnitude*
		getStationMagnitude(DataModel::Origin*,
		                    const DataModel::WaveformStreamID&,
		                    const std::string&, double, bool) const;

		// like _getStationMagnitude
		DataModel::Magnitude*
		getMagnitude(DataModel::Origin*, const std::string&,
		             double, bool* newInstance = NULL) const;

		// like _getStationMagnitude, but no update will be made
		DataModel::Magnitude*
		getMagnitude(DataModel::Origin*, const std::string&,
		             bool* newInstance = NULL) const;

		bool computeStationMagnitude(const DataModel::Amplitude*,
		                             const DataModel::Origin*,
		                             const DataModel::SensorLocation *,
		                              double, double, MagnitudeList&);

		bool computeNetworkMagnitude(DataModel::Origin*, const std::string&, DataModel::MagnitudePtr);
		bool computeSummaryMagnitude(DataModel::Origin*);


		// For the given origin, determine missing picks and arrivals and
		// retrieve them from the database (if configured) Returns the total
		// number of retrieved objects.
		int retrieveMissingPicksAndArrivalsFromDB(const DataModel::Origin*);
	
		//! process new or updated Origin
		// if something changed, returns true, false otherwise
		bool processOrigin(DataModel::Origin*);

		bool processOriginUpdateOnly(DataModel::Origin*);
	
		//! process new or updated Pick
		// if something changed, returns true, false otherwise
		bool processPick(DataModel::Pick*) { return false; }

	private:
		OriginList *createBinding(const std::string &pickID);

		// Caches a pick - origins association
		void bind(const std::string &pickID, DataModel::Origin *origin);
		OriginList *originsForPick(const std::string &pickID);

		bool isTypeEnabledForSummaryMagnitude(const std::string &) const;

		Util::KeyValues *fetchParams(const DataModel::Amplitude *);

		bool computeAverage(AverageDescription &avg,
		                    const std::vector<double> &values,
		                    std::vector<double> &weights,
		                    std::string &method, double &value, double &stdev);

	private:
		typedef Processing::MagnitudeProcessorFactory::ServiceNames MagnitudeTypeList;
		typedef std::multimap<std::string, Processing::MagnitudeProcessorPtr> ProcessorList;
		typedef std::set<std::string> TypeList;
		typedef std::map<std::string, Util::KeyValuesPtr> ParameterMap;

		MagnitudeTypeList  _registeredMagTypes;
		MagnitudeTypes     _magTypes;
		ProcessorList      _processors;
		ParameterMap       _parameters;
		Core::TimeSpan     _cacheSize;

		double             _minimumArrivalWeight{0.5};
		bool               _summaryMagnitudeEnabled{true};
		bool               _summaryMagnitudeSingleton{true};
		int                _summaryMagnitudeMinStationCount{1};
		std::string        _summaryMagnitudeType{"M"};
		TypeList           _summaryMagnitudeBlacklist;
		TypeList           _summaryMagnitudeWhitelist;

		SummaryMagnitudeCoefficients _defaultCoefficients;
		Coefficients       _magnitudeCoefficients;
		AverageMethods     _magnitudeAverageMethods;

		bool               _allowReprocessing{false};
		bool               _staticUpdate{false};
		bool               _keepWeights{false};
		double             _warningLevel;
		size_t             _dbAccesses;

	public:
		Client::Application::ObjectLog *inputPickLog;
		Client::Application::ObjectLog *inputAmpLog;
		Client::Application::ObjectLog *inputOrgLog;
		Client::Application::ObjectLog *outputMagLog;
};

}
}

#endif
