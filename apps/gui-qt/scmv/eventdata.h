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


#ifndef __EVENTDATA_H___
#define __EVENTDATA_H___

#include <seiscomp/datamodel/event.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/focalmechanism.h>
#include <seiscomp/datamodel/magnitude.h>
#include <seiscomp/datamodel/amplitude.h>
#include <seiscomp/datamodel/arrival.h>
#include <seiscomp/datamodel/pick.h>

#include <seiscomp/datamodel/databasearchive.h>
#include <seiscomp/gui/datamodel/originsymbol.h>
#include <seiscomp/gui/datamodel/tensorsymbol.h>
#include <seiscomp/datamodel/publicobjectcache.h>


template <typename T>
class Data {
	public:
		Data(T* object)
		: _objectPtr(object)
		, _containerCreationTime(Seiscomp::Core::Time::UTC()) {
		}

		virtual ~Data() {}

	public:
		virtual const std::string& id() const = 0;

		const T* object() const {
			return _objectPtr.get();
		}

		const Seiscomp::Core::Time& containerCreationTime() const {
			return _containerCreationTime;
		}

		void setContainerCreationTime(const Seiscomp::Core::Time& time) {
			_containerCreationTime = time;
		}

	private:
		Seiscomp::Core::SmartPointer<T> _objectPtr;
		Seiscomp::Core::Time            _containerCreationTime;

};




class EventData : public Data<Seiscomp::DataModel::Event> {
	public:
		EventData(Seiscomp::DataModel::Event* event,
		          Seiscomp::Gui::OriginSymbol* originSymbol,
		          Seiscomp::Gui::TensorSymbol* tensorSymbol,
		          bool isVisible = true)
		: Data<Seiscomp::DataModel::Event>(event)
		, _originSymbolRef(originSymbol)
		, _tensorSymbolRef(tensorSymbol)
		, _isActive(false)
		, _isSelected(false)
		, _isVisible(isVisible)
		{}

	public:
		virtual const std::string& id() const {
			return object()->publicID();
		}

		const std::string& preferredOriginId() const {
			return object()->preferredOriginID();
		}

		bool isActive() const {
			return _isActive;
		}

		void setActive(bool val) {
			_isActive = val;
		}

		bool isSelected() const {
			return _isSelected;
		}

		void setSelected(bool val) {
			_isSelected = val;
		}

		bool isVisible() const {
			return _isVisible;
		}

		void setVisible(bool val) {
			_isVisible = val;
		}

		Seiscomp::Gui::OriginSymbol* originSymbol() const {
			return _originSymbolRef;
		}

		Seiscomp::Gui::TensorSymbol* tensorSymbol() const {
			return _tensorSymbolRef;
		}

	private:
		Seiscomp::Gui::OriginSymbol *_originSymbolRef;
		Seiscomp::Gui::TensorSymbol *_tensorSymbolRef;

		bool _isActive;
		bool _isSelected;
		bool _isVisible;
};




class ArrivalData : public Data<Seiscomp::DataModel::Arrival> {
	public:
		ArrivalData(Seiscomp::DataModel::Arrival* arrival)
		 : Data<Seiscomp::DataModel::Arrival>(arrival) {
		}

	public:
		virtual const std::string& id() const {
			return object()->pickID();
		}
};




typedef std::list<ArrivalData> ArrivalDataCollection;
typedef std::list<EventData>   EventDataCollection;




class EventDataRepository {
	public:
		typedef EventDataCollection::iterator event_iterator;
		typedef EventDataCollection::const_iterator const_event_iterator;

	public:
		EventDataRepository();
		EventDataRepository(Seiscomp::DataModel::DatabaseArchive* databaseArchive,
		                   const Seiscomp::Core::TimeSpan& timeSpan);

	public:
		event_iterator eventsBegin();
		event_iterator eventsEnd();

		const_event_iterator eventsBegin() const;
		const_event_iterator eventsEnd() const;

		int eventCount() const;

		void setDatabaseArchive(Seiscomp::DataModel::DatabaseArchive* dataBaseArchive);
		void setEventDataLifeSpan(const Seiscomp::Core::TimeSpan& timeSpan);

		bool addEvent(Seiscomp::DataModel::Event* event,
		              Seiscomp::Gui::OriginSymbol* originSymbol,
		              Seiscomp::Gui::TensorSymbol* tensorSymbol,
		              bool passedFilter);
		void addOrigin(Seiscomp::DataModel::Origin* origin);
		void addFocalMechanism(Seiscomp::DataModel::FocalMechanism* fm);
		void addMagnitude(Seiscomp::DataModel::Magnitude* magnitude);
		void addAmplitude(Seiscomp::DataModel::Amplitude* amplitude);
		void addArrival(Seiscomp::DataModel::Arrival* arrival);
		void addPick(Seiscomp::DataModel::Pick* pick);

		EventData* findEvent(const std::string& id);
		EventData* findNextExpiredEvent();
		EventData* findLatestEvent();
		ArrivalData* findArrivalwithPickId(const std::string& pickId);
		Seiscomp::DataModel::Origin* findOrigin(const std::string& id);
		Seiscomp::DataModel::FocalMechanism* findFocalMechanism(const std::string& id);
		Seiscomp::DataModel::Magnitude* findMagnitude(const std::string& id);

		void removeEvent(const std::string& id);

	private:
		ArrivalDataCollection                           _arrivalDataCollection;
		EventDataCollection                             _eventDataCollection;
		Seiscomp::DataModel::PublicObjectTimeSpanBuffer _objectTimeSpanBuffer;

		Seiscomp::Core::TimeSpan _eventDataObjectLifeSpan;
};


#endif
