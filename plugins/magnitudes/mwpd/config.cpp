/***************************************************************************
 * SeisComP duration-amplitude moment-magnitude plugin (mwpd)              *
 *                                                                         *
 * Shared configuration reader and the Early-est constants/corrections     *
 * (moment constant, INGV_EE distance correction, PREM step depth          *
 * correction).                                                            *
 *                                                                         *
 * GNU Affero General Public License Usage - see LICENSE.                  *
 ***************************************************************************/


#define SEISCOMP_COMPONENT Mwpd

#include <seiscomp/logging/log.h>

#include <cmath>

#include "mwpd.h"


namespace Seiscomp {
namespace Magnitudes {
namespace Mwpd {


// PREM depth correction table (depth_corr_mwpd.h), depth[km] -> delta-magnitude,
// 0..600 km. Step lookup: the correction of the largest table depth <= h.
namespace {

const int    NUM_DEPTH_CORR = 11;
const double DEPTH_CORR[NUM_DEPTH_CORR][2] = {
	{  0.0,  0.00 }, { 71.0,  0.00 }, { 80.0,  0.00 }, {171.0, -0.01 },
	{220.0,  0.05 }, {271.0,  0.06 }, {371.0,  0.07 }, {400.0,  0.12 },
	{471.0,  0.15 }, {571.0,  0.18 }, {600.0,  0.22 }
};

}


double mwpdMomentConstant(const MwpdConfig &cfg) {
	// Early-est MWP_CONST (Tsuboi 1995):
	//   4 pi * rho * Vp^3 * FP * (10000/90 deg->km) * 1000 (km->m)
	return 4.0 * M_PI * cfg.rho * cfg.vp * cfg.vp * cfg.vp * cfg.fp
	       * (10000.0 / 90.0) * 1000.0;
}


double mwpdDistanceCorrection(double deltaDeg, double depthKm) {
	// calculate_Mwp_correction_INGV_EE (20150312 coefficients).
	if ( deltaDeg < 0.0 || depthKm > 100.0 ) {
		return 0.0;
	}
	return -1.32e-6 * deltaDeg * deltaDeg * deltaDeg
	       + 2.40e-4 * deltaDeg * deltaDeg
	       - 0.0146  * deltaDeg
	       + 0.314;
}


double mwpdDepthCorrection(double depthKm) {
	// get_depth_corr_mwpd_prem: largest table depth <= h; -999 if below row 0.
	int index = NUM_DEPTH_CORR - 1;
	while ( index >= 0 && DEPTH_CORR[index][0] > depthKm ) {
		--index;
	}
	return ( index < 0 ) ? -999.0 : DEPTH_CORR[index][1];
}


bool readMwpdConfig(const Processing::Settings &settings,
                    const std::string &prefix, MwpdConfig &out) {
	// BRB-HP restitution + displacement integral.
	settings.getValue(out.gainFrequency,   prefix + ".gainFrequency");
	settings.getValue(out.hpOrder,         prefix + ".highpassOrder");
	settings.getValue(out.analysisPreP,    prefix + ".analysisPreP");

	// HF envelope.
	settings.getValue(out.hfFmin,          prefix + ".hfFmin");
	settings.getValue(out.hfFmax,          prefix + ".hfFmax");
	settings.getValue(out.hfOrder,         prefix + ".hfOrder");
	settings.getValue(out.smoothHalfWidth, prefix + ".smoothHalfWidth");

	// T0 estimation.
	settings.getValue(out.minDuration,     prefix + ".minDuration");
	settings.getValue(out.maxDuration,     prefix + ".maxDuration");
	settings.getValue(out.snT0End,         prefix + ".snT0End");
	settings.getValue(out.peakRatioT0End,  prefix + ".peakRatioT0End");
	settings.getValue(out.fixedDuration,   prefix + ".fixedDuration");
	settings.getValue(out.fixedDurationVal,prefix + ".fixedDurationValue");

	// Magnitude formula + corrections.
	settings.getValue(out.rho,             prefix + ".density");
	settings.getValue(out.vp,              prefix + ".vp");
	settings.getValue(out.fp,              prefix + ".radiation");
	settings.getValue(out.useDistanceCorr, prefix + ".distanceCorrection");
	settings.getValue(out.useDepthCorr,    prefix + ".depthCorrection");
	settings.getValue(out.useDurationRamp, prefix + ".durationRamp");
	settings.getValue(out.minDistanceDeg,  prefix + ".minimumDistance");
	settings.getValue(out.maxDistanceDeg,  prefix + ".maximumDistance");

	if ( out.hfFmax <= out.hfFmin ) {
		SEISCOMP_ERROR("%s: hfFmax (%f) must be > hfFmin (%f)",
		               prefix.c_str(), out.hfFmax, out.hfFmin);
		return false;
	}
	if ( out.maxDuration <= out.minDuration ) {
		SEISCOMP_ERROR("%s: maxDuration (%f) must be > minDuration (%f)",
		               prefix.c_str(), out.maxDuration, out.minDuration);
		return false;
	}

	return true;
}


}
}
}
