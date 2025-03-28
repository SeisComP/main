/***************************************************************************
 * Copyright (C) 2013 by gempa GmbH
 *
 * Author: Jan Becker
 * Email: jabe@gempa.de
 ***************************************************************************/


#ifndef __GEMPA_INVMGR_CHECK_H__
#define __GEMPA_INVMGR_CHECK_H__


#include <seiscomp/core/timewindow.h>

#include <list>
#include <map>

#include "inv.h"


//! \brief Check class that checks consistency of an inventory.
class Check : public InventoryTask {
	// ------------------------------------------------------------------
	//  Xstruction
	// ------------------------------------------------------------------
	public:
		//! C'tor
		//! Creates a check object with a target inventory
		//! which holds the final merged inventory
		Check(Seiscomp::DataModel::Inventory *inv);

	// ------------------------------------------------------------------
	//  Public interface
	// ------------------------------------------------------------------
	public:
		using Epochs = std::list<OpenTimeWindow>;
		using EpochMap = std::map<std::string, Epochs>;

		bool check();

		bool setMaxDepth(double maxDepth);
		bool setMaxDistance(double maxDistance);
		bool setMaxElevationDifference(double maxElevationDifference);


	// ------------------------------------------------------------------
	//  Private interface
	// ------------------------------------------------------------------
	private:
		template <typename T>
		void checkEpoch(const T *obj);

		template <typename T>
		void checkOverlap(Epochs &epochs, const T *obj);

		template <typename T1, typename T2>
		void checkOutside(const T1 *parent, const T2 *obj);

		template <typename T>
		void checkDescription(const T *obj);

		Seiscomp::DataModel::Sensor *findSensor(const std::string &) const;
		Seiscomp::DataModel::ResponsePAZ *findPAZ(const std::string &) const;
		Seiscomp::DataModel::ResponsePolynomial *findPoly(const std::string &) const;
		Seiscomp::DataModel::ResponseFAP *findFAP(const std::string &) const;

	public:
		double    _maxElevationDifference{500};
		double    _maxDistance{10};
		double    _maxDepth{500};
};


#endif
