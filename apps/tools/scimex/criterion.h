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


#ifndef SCIMEX_CRITERION_H
#define SCIMEX_CRITERION_H


#include <utility>
#include <vector>
#include <string>
#include <functional>

#include <seiscomp/client/application.h>
#include <seiscomp/datamodel/magnitude.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/utils/leparser.h>


namespace Seiscomp {
namespace Applications {


struct FilterContext {
	DataModel::Origin *org;
	DataModel::Magnitude *mag;
};


class Criterion : public Utils::LeExpression {
	// ----------------------------------------------------------------------
	// Nested Types
	// ----------------------------------------------------------------------
	private:
		using DoublePair = std::pair<double, double>;

	public:
		using LatitudeRange = DoublePair;
		using LongitudeRange = DoublePair;
		using MagnitudeRange = DoublePair;
		using AgencyIDs = std::vector<std::string>;


	// ----------------------------------------------------------------------
	// X'truction
	// ----------------------------------------------------------------------
	public:
		~Criterion() override {}


	// ----------------------------------------------------------------------
	// Expression interface
	// ----------------------------------------------------------------------
	public:
		//! context will be of type FilterContext
		bool eval(const void *context) override;


	// ----------------------------------------------------------------------
	// Public member functions
	// ----------------------------------------------------------------------
	public:
		const LatitudeRange &latitudeRange() const;
		void setLatitudeRange(double lat0, double lat1);

		const LongitudeRange &longitudeRange() const;
		void setLongitudeRange(double lon0, double lon1);

		const MagnitudeRange &magnitudeRange() const;
		void setMagnitudeRange(double mag0, double mag1);

		size_t arrivalCount() const;
		void setArrivalCount(size_t count);

		const AgencyIDs& agencyIDs() const;
		void setAgencyIDs(const AgencyIDs &ids);

	private:
		bool isInLatLonRange(double lat, double lon);
		bool isInMagnitudeRange(double mag);
		bool checkArrivalCount(size_t count);
		bool checkAgencyID(const std::string &id);

	// ----------------------------------------------------------------------
	// Private data members
	// ----------------------------------------------------------------------
	private:
		LatitudeRange  _latitudeRange;
		LongitudeRange _longitudeRange;
		MagnitudeRange _magnitudeRange;
		size_t         _arrivalCount;
		AgencyIDs      _agencyIDs;
};


class CriterionFactory : public Utils::LeFactory {
	public:
		CriterionFactory(const std::string &sinkName, Client::Application *app);

	public:
		Utils::LeExpression *createExpression(const std::string &name) const override;

	private:
		bool configGetLatitude(const std::string &prefix, const std::string &name, Criterion *criterion) const;
		bool configGetLongitude(const std::string &prefix, const std::string &name, Criterion *criterion) const;
		bool configGetMagnitude(const std::string &prefix, const std::string &name, Criterion *criterion) const;
		bool configGetArrivalCount(const std::string &prefix, const std::string &name, Criterion *criterion) const;
		bool configGetAgencyID(const std::string &prefix, const std::string &name, Criterion *criterion) const;

		template <typename FuncObj>
		bool getRangeFromConfig(const std::string &prefix, const std::string &name, FuncObj func) const;

	private:
		std::string          _sinkName;
		Client::Application *_app;
};


} // namespace Applications
} // namespace Seiscomp


#endif
