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


#ifndef __STATIONDATA_H___
#define __STATIONDATA_H___

#include <string>
#include <map>
#include <memory>

#include <boost/shared_ptr.hpp>

#include <QColor>

#include <seiscomp/datamodel/station.h>
#include <seiscomp/core/datetime.h>
#include <seiscomp/gui/map/mapsymbol.h>
#include <seiscomp/math/filter.h>
#include <seiscomp/datamodel/pick.h>
#include <seiscomp/datamodel/amplitude.h>

#include "mvstationsymbol.h"
#include "types.h"


class MvStationSymbol;


class StationData {
	public:
		typedef boost::shared_ptr<
			Seiscomp::Math::Filtering::InPlaceFilter<double> > GmFilterPtr;

		typedef std::list<Seiscomp::DataModel::PickPtr> TPickCollection;

		struct QCData {
			double           value;
			double           lowerUncertainty;
			double           upperUncertainty;
			QCStatus::Status status;

			QCData()
			 : value(0),
			   lowerUncertainty(0),
			   upperUncertainty(0),
			   status(QCStatus::NOT_SET) {
			}
		};
		typedef std::map<QCParameter::Parameter, QCData> QcDataMap;

	public:
		std::string id;

		Seiscomp::DataModel::StationPtr stationRef;
		MvStationSymbol*                stationSymbolRef;

		bool isEnabled;
		bool isTriggering;
		bool isAssociated;
		bool isSelected;

		int frameSize;

	public:
		double                             gmGain;
		Seiscomp::Core::Time               gmRecordReceiveTime;
		Seiscomp::Core::Time               gmSampleReceiveTime;
		double                             gmMaximumSample;
		GmFilterPtr                        gmFilter;
		QColor                             gmColor;

	public:
		TPickCollection                    tPickCache;
		Seiscomp::Core::Time               tPickTime;

		Seiscomp::DataModel::AmplitudePtr  tAmplitude;
		Seiscomp::Core::Time               tAmplitudeTime;
		Seiscomp::Core::TimeSpan           tAmplitudeTriggerLifeSpan;

	public:
		QcDataMap           qcDataMap;
		QColor              qcColor;
		QCStatus::Status    qcGlobalStatus;


	public:
		StationData() {
			init();
			reset();
		}

	public:
		void init() {
			stationRef       = NULL;
			stationSymbolRef = NULL;
			isEnabled        = true;

			gmGain           = 0;
		}

		void reset() {
			   isTriggering = false;
			   isAssociated = false;
			   isSelected   = false;
			   frameSize    = 0;

			   gmRecordReceiveTime = Seiscomp::Core::Time::Null;
			   gmSampleReceiveTime = Seiscomp::Core::Time::Null;
			   gmMaximumSample     = 0;
			   gmColor             = Qt::black;

			   tPickCache.clear();
			   tPickTime                 =  Seiscomp::Core::Time::Null;
			   tAmplitude                = NULL;
			   tAmplitudeTime            = Seiscomp::Core::Time::Null;
			   tAmplitudeTriggerLifeSpan = 0.0;

			   qcDataMap.clear();
			   qcColor        = Qt::black;
			   qcGlobalStatus = QCStatus::NOT_SET;

		}

		void resetTriggerRelatedData() {
		   isTriggering = false;
		   isAssociated = false;
		   frameSize    = 0;

		   tPickCache.clear();
		   tPickTime                 =  Seiscomp::Core::Time::Null;
		   tAmplitude                = NULL;
		   tAmplitudeTime            = Seiscomp::Core::Time::Null;
		   tAmplitudeTriggerLifeSpan = 0.0;
		}
};


class StationDataCollection {
	// ----------------------------------------------------------------------
	// Nested types
	// ----------------------------------------------------------------------
	private:
		typedef std::list<StationData> StationDataCollectionImpl;
		typedef std::map<std::string, StationData*> StationDataCache;

	public:
		typedef StationDataCollectionImpl::iterator iterator;
		typedef StationDataCollectionImpl::const_iterator const_iterator;


	// ----------------------------------------------------------------------
	// Public collection interface
	// ----------------------------------------------------------------------
	public:
		void add(const StationData& data);
		bool remove(const std::string& id);
		StationData* find(const std::string& id);

	public:
		iterator begin();
		iterator end();
		const_iterator begin() const;
		const_iterator end() const;

	// ----------------------------------------------------------------------
	// Private data members
	// ----------------------------------------------------------------------
	private:
		StationDataCollectionImpl _data;
		StationDataCache          _stationDataCache;

};


#endif
