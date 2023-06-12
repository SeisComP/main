/***************************************************************************
 * Copyright (C) gempa GmbH                                                *
 * All rights reserved.                                                    *
 * Contact: gempa GmbH (seiscomp-dev@gempa.de)                             *
 *                                                                         *
 * Author: Jan Becker                                                      *
 * Email: jabe@gempa.de                                                    *
 *                                                                         *
 * GNU Affero General Public License Usage                                 *
 * This file may be used under the terms of the GNU Affero                 *
 * Public License version 3.0 as published by the Free Software Foundation *
 * and appearing in the file LICENSE included in the packaging of this     *
 * file. Please review the following information to ensure the GNU Affero  *
 * Public License version 3.0 requirements will be met:                    *
 * https://www.gnu.org/licenses/agpl-3.0.html.                             *
 *                                                                         *
 * Other Usage                                                             *
 * Alternatively, this file may be used in accordance with the terms and   *
 * conditions contained in a signed written agreement between you and      *
 * gempa GmbH.                                                             *
 ***************************************************************************/


#ifndef SEISCOMP_CUSTOM_MAGNITUDE_NUTTLI_H__
#define SEISCOMP_CUSTOM_MAGNITUDE_NUTTLI_H__


#include <seiscomp/core/version.h>
#include <seiscomp/processing/magnitudeprocessor.h>


namespace {


class MNMagnitude : public Seiscomp::Processing::MagnitudeProcessor {
	// ----------------------------------------------------------------------
	//  X'truction
	// ----------------------------------------------------------------------
	public:
		//! C'tor
		MNMagnitude();


	// ----------------------------------------------------------------------
	//  MagnitudeProcessor interface
	// ----------------------------------------------------------------------
	public:
		std::string amplitudeType() const override;
		void finalizeMagnitude(Seiscomp::DataModel::StationMagnitude *magnitude) const override;


	protected:
		bool setup(const Seiscomp::Processing::Settings &settings) override;

		// The MagnitudeProcessor interface has changed with API version 11
		// and API version 12.
		// The following compile time condition accounts for that.
		Status computeMagnitude(double amplitude,
		                        const std::string &unit,
		                        double period,
		                        double snr,
		                        double delta, double depth,
		                        const Seiscomp::DataModel::Origin *hypocenter,
		                        const Seiscomp::DataModel::SensorLocation *receiver,
		                        const Seiscomp::DataModel::Amplitude *,
		                        const Locale *,
		                        double &value) override;

		bool treatAsValidMagnitude() const override;

		Status estimateMw(const Seiscomp::Config::Config *config,
		                  double magnitude, double &estimateMw,
		                  double &stdError) override;


	private:
		bool   _validValue;
		double _minSNR;
		double _minPeriod;
		double _maxPeriod;
		double _minDistance;
		double _maxDistance;
};


}


#endif
