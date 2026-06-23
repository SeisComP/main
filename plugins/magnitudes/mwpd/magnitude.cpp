/***************************************************************************
 * SeisComP duration-amplitude moment-magnitude plugin (mwpd)              *
 *                                                                         *
 * Magnitude processor: displacement integral over T0 -> M0 -> Mwpd, with  *
 * the INGV_EE distance correction, the PREM step depth correction and the *
 * >90 s duration ramp. Faithful to Early-est calculate_Raw_Mwpd_Mag +     *
 * calculate_corrected_Mwpd_Mag.                                           *
 *                                                                         *
 * GNU Affero General Public License Usage - see LICENSE.                  *
 ***************************************************************************/


#define SEISCOMP_COMPONENT Mwpd

#include <seiscomp/logging/log.h>

#include <cfloat>
#include <cmath>

#include "mwpd.h"


namespace Seiscomp {
namespace Magnitudes {
namespace Mwpd {


REGISTER_MAGNITUDEPROCESSOR(MagnitudeProcessor_Mwpd, MWPD_TYPE);


MagnitudeProcessor_Mwpd::MagnitudeProcessor_Mwpd()
: Processing::MagnitudeProcessor(MWPD_TYPE) {
	_minimumDistanceDeg = _cfg.minDistanceDeg;
	_maximumDistanceDeg = _cfg.maxDistanceDeg;
}


bool MagnitudeProcessor_Mwpd::setup(const Processing::Settings &settings) {
	if ( !Processing::MagnitudeProcessor::setup(settings) ) {
		return false;
	}

	_cfg = MwpdConfig();
	if ( !readMwpdConfig(settings, std::string("magnitudes.") + type(), _cfg) ) {
		return false;
	}

	_minimumDistanceDeg = _cfg.minDistanceDeg;
	_maximumDistanceDeg = _cfg.maxDistanceDeg;

	SEISCOMP_DEBUG("%s: dist=%.1f-%.1f deg distCorr=%d depthCorr=%d durRamp=%d",
	               type().c_str(), _cfg.minDistanceDeg, _cfg.maxDistanceDeg,
	               _cfg.useDistanceCorr, _cfg.useDepthCorr, _cfg.useDurationRamp);
	return true;
}


MagnitudeProcessor_Mwpd::Status
MagnitudeProcessor_Mwpd::computeMagnitude(
	double amplitude, const std::string &unit,
	double period, double /*snr*/,
	double delta, double depth,
	const DataModel::Origin *,
	const DataModel::SensorLocation *,
	const DataModel::Amplitude *,
	const Locale *,
	double &value) {

	if ( amplitude <= 0.0 ) {
		return AmplitudeOutOfRange;
	}

	// The displacement integral is carried in nm*s (as Mwp carries nm).
	double ampNmS = amplitude;
	if ( !convertAmplitude(ampNmS, unit, MWPD_AMP_UNIT) ) {
		return InvalidAmplitudeUnit;
	}

	// T0 (source duration) arrives in the amplitude period (seconds).
	const double duration = period;

	if ( depth < 0.0 ) {
		depth = 0.0;
	}

	// --- raw Mwpd (calculate_Raw_Mwpd_Mag) --------------------------------
	const double amplitudeIntegral = ampNmS * 1.0e-9;   // nm*s -> m*s
	double moment = mwpdMomentConstant(_cfg) * amplitudeIntegral * delta;
	if ( moment <= FLT_MIN ) {
		return Error;
	}

	double raw = (2.0 / 3.0) * (std::log10(moment) - 9.1);
	if ( _cfg.useDistanceCorr ) {
		raw -= mwpdDistanceCorrection(delta, depth);
	}

	// --- corrected Mwpd (calculate_corrected_Mwpd_Mag) --------------------
	double corr = raw;

	if ( _cfg.useDepthCorr ) {
		const double dc = mwpdDepthCorrection(depth);
		if ( dc > -998.0 ) {   // -999 marks an invalid (too-shallow) lookup
			corr += dc;
		}
	}

	// Moment scaling for large interplate-thrust / tsunami events, Lomax &
	// Michelini (2009) eq (5a): M0pd = M0*(M0/M0cutoff)^0.45 for M0 >= M0cutoff
	// (Mw ~ 7.2), i.e. in magnitude terms Mwpd = raw + 0.45*(raw - 7.2). The
	// function is continuous at the cutoff. The paper restricts this to
	// interplate-thrust/tsunami events; event-type classification is not
	// available here, so it is applied to all large events (configurable).
	if ( _cfg.useDurationRamp && raw > _cfg.magCutoff ) {
		corr += (raw - _cfg.magCutoff) * _cfg.rampPow;
	}

	value = corr;

	SEISCOMP_DEBUG("%s: ampInt=%g nm*s T0=%.1fs delta=%.2f depth=%.1f "
	               "-> logM0=%.2f raw=%.2f Mwpd=%.2f",
	               type().c_str(), ampNmS, duration, delta, depth,
	               std::log10(moment), raw, value);

	return OK;
}


}
}
}
