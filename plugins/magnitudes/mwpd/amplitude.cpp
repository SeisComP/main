/***************************************************************************
 * SeisComP duration-amplitude moment-magnitude plugin (mwpd)              *
 *                                                                         *
 * Amplitude processor. From the raw vertical window it reproduces two     *
 * Early-est streams:                                                      *
 *   1. BRB-HP displacement: counts/gain -> velocity, high-passed at       *
 *      MWPD_GAIN_FREQUENCY, single-integrated to displacement; the running *
 *      double integral is accumulated into separate positive- and         *
 *      negative-displacement lobes (int_int_sum_pos/neg).                  *
 *   2. HF envelope: raw band-passed 1-5 Hz, squared, boxcar-smoothed; its  *
 *      peak/level (90/80/50/20 %) crossings give the source duration T0.   *
 * The emitted amplitude is the displacement integral at T0 (nm*s); T0 is   *
 * carried as the amplitude "period".                                       *
 *                                                                         *
 * Ports timedomain_processing.c (mwpd + T0 loops) and                     *
 * timedomain_processing_data.c (calculate_Raw_Mwpd_Mag, calculate_duration).*
 *                                                                         *
 * GNU Affero General Public License Usage - see LICENSE.                  *
 ***************************************************************************/


#define SEISCOMP_COMPONENT Mwpd

#include <seiscomp/logging/log.h>
#include <seiscomp/math/filter/butterworth.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/sensorlocation.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

#include "mwpd.h"


using namespace Seiscomp::Math::Filtering::IIR;


namespace Seiscomp {
namespace Magnitudes {
namespace Mwpd {


namespace {

// Centred boxcar smoothing (half-width in samples), matching the T50 envelope
// smoothing. Edges shrink the window to the available samples.
void boxcarSmooth(std::vector<double> &x, int halfWidth) {
	if ( halfWidth < 1 || x.size() < 2 ) {
		return;
	}
	const int n = static_cast<int>(x.size());

	// Prefix sums for an O(n) moving average.
	std::vector<double> pre(n + 1, 0.0);
	for ( int i = 0; i < n; ++i ) {
		pre[i + 1] = pre[i] + x[i];
	}

	std::vector<double> out(n);
	for ( int i = 0; i < n; ++i ) {
		int a = std::max(0, i - halfWidth);
		int b = std::min(n - 1, i + halfWidth);
		out[i] = (pre[b + 1] - pre[a]) / (b - a + 1);
	}
	x.swap(out);
}

// Source-duration estimate from the level-crossing indices (relative to the
// pick, in samples). Ports calculate_duration().
double durationFromLevels(int idx90, int idx80, int idx50, int idx20,
                          double dt, double floorSec) {
	const double t9 = idx90 * dt;
	const double t8 = idx80 * dt;
	const double t5 = idx50 * dt;
	const double t2 = idx20 * dt;

	const double durEstimate = (t5 + t8) / 2.0;
	double factor = (durEstimate - 20.0) / 40.0;   // 0->1 for 20->60 s
	if ( factor < 0.0 ) {
		factor = 0.0;
	}
	else if ( factor > 1.0 ) {
		factor = 1.0;
	}

	double duration = t9 * (1.0 - factor) + t2 * factor;
	if ( duration < floorSec ) {
		duration = floorSec;
	}
	return duration;
}

}  // namespace


REGISTER_AMPLITUDEPROCESSOR(AmplitudeProcessor_Mwpd, MWPD_TYPE);


AmplitudeProcessor_Mwpd::AmplitudeProcessor_Mwpd()
: Processing::AmplitudeProcessor(MWPD_TYPE) {
	setUsedComponent(Vertical);
	setUnit(MWPD_AMP_UNIT);
	// Compute progressively as data streams (Early-est style): the processor
	// emits a growing Mwpd and finalizes as soon as T0 resolves/terminates,
	// rather than waiting the full (long) signal window. maxDuration is just
	// the upper bound on how long we keep looking.
	setUpdateEnabled(true);
	applyConfig();
}


void AmplitudeProcessor_Mwpd::applyConfig() {
	// Pick at signalStart=0; integration starts analysisPreP before it (taken
	// from the noise side of the window). Signal end covers the longest T0.
	setSignalStart(0.0);
	setSignalEnd(_cfg.maxDuration);
	setNoiseStart(-(_cfg.analysisPreP + 30.0));
	setNoiseEnd(-_cfg.analysisPreP);

	setMinDist(_cfg.minDistanceDeg);
	setMaxDist(_cfg.maxDistanceDeg);
	setMinSNR(0);   // T0 has its own S/N termination; no scalar SNR gate
	computeTimeWindow();
}


bool AmplitudeProcessor_Mwpd::setup(const Processing::Settings &settings) {
	if ( !Processing::AmplitudeProcessor::setup(settings) ) {
		return false;
	}

	_cfg = MwpdConfig();
	if ( !readMwpdConfig(settings, std::string("amplitudes.") + type(), _cfg) ) {
		return false;
	}

	applyConfig();

	// Travel-time table for the S-P window cap (graceful if unavailable).
	_ttt = nullptr;
	if ( _cfg.useSpCap ) {
		_ttt = Seiscomp::TravelTimeTableInterface::Create(_cfg.tttBackend.c_str());
		if ( !_ttt || !_ttt->setModel(_cfg.tttModel) ) {
			SEISCOMP_WARNING("%s: travel-time table '%s'/'%s' unavailable; "
			                 "S-P window cap disabled",
			                 type().c_str(), _cfg.tttBackend.c_str(),
			                 _cfg.tttModel.c_str());
			_ttt = nullptr;
		}
	}

	SEISCOMP_DEBUG("%s: gainFreq=%.2fHz HF=%.1f-%.1fHz preP=%.1fs maxDur=%.0fs spCap=%d",
	               type().c_str(), _cfg.highpassCorner, _cfg.hfFmin, _cfg.hfFmax,
	               _cfg.analysisPreP, _cfg.maxDuration, _ttt != nullptr);
	return true;
}


double AmplitudeProcessor_Mwpd::computeSPSeconds() const {
	if ( !_ttt || !_environment.hypocenter || !_environment.receiver ) {
		return -1.0;
	}
	try {
		const double slat = _environment.hypocenter->latitude().value();
		const double slon = _environment.hypocenter->longitude().value();
		double sdep = 0.0;
		try { sdep = _environment.hypocenter->depth().value(); }
		catch ( ... ) {}
		const double rlat = _environment.receiver->latitude();
		const double rlon = _environment.receiver->longitude();
		double relev = 0.0;
		try { relev = _environment.receiver->elevation(); }
		catch ( ... ) {}

		const double tP = _ttt->computeTime("P", slat, slon, sdep, rlat, rlon, relev);
		const double tS = _ttt->computeTime("S", slat, slon, sdep, rlat, rlon, relev);
		if ( tP > 0.0 && tS > 0.0 && tS > tP ) {
			return tS - tP;
		}
	}
	catch ( ... ) {}
	return -1.0;
}


const DoubleArray *AmplitudeProcessor_Mwpd::processedData(Component comp) const {
	return comp == targetComponent() ? &_processedData : nullptr;
}


bool AmplitudeProcessor_Mwpd::computeAmplitude(
	const DoubleArray &data,
	size_t i1, size_t i2,
	size_t /*si1*/, size_t /*si2*/,
	double offset,
	AmplitudeIndex *dt,
	AmplitudeValue *amplitude,
	double *period, double *snr) {

	const double fsamp = _stream.fsamp;
	if ( fsamp <= 0.0 ) {
		setStatus(Error, 1);
		return false;
	}
	const double deltaTime = 1.0 / fsamp;

	const double gain = std::fabs(_streamConfig[targetComponent()].gain);
	if ( gain == 0.0 ) {
		setStatus(MissingGain, 1);
		return false;
	}

	const int n      = static_cast<int>(i2);          // end of usable window
	const int iPick  = static_cast<int>(i1);           // signalStart = pick
	const int preP   = static_cast<int>(_cfg.analysisPreP * fsamp + 0.5);
	const int iStart = iPick - preP;                   // integration start
	if ( iStart < 0 || n > static_cast<int>(data.size()) || n - iPick < 16 ) {
		// Not enough data past the pick yet. With incremental updates this is a
		// "keep waiting" state, NOT an error: leave the status InProgress so the
		// base keeps feeding. (A terminal status here would kill the processor.)
		return false;
	}

	const double *raw = static_cast<const double *>(data.data());

	// =====================================================================
	//  1) BRB-HP displacement and its positive/negative running integrals.
	// =====================================================================
	// Velocity (counts -> m/s), demeaned by the noise offset, high-passed at
	// the BRB-HP corner. We keep it as _processedData for inspection.
	_processedData.resize(n);
	double *vel = _processedData.typedData();
	for ( int i = 0; i < n; ++i ) {
		vel[i] = (raw[i] - offset) / gain;
	}

	ButterworthHighpass<double> hp(_cfg.hpOrder, _cfg.highpassCorner, fsamp);
	hp.apply(n, vel);

	// Running single integral (displacement) and double integral split into
	// positive- and negative-displacement lobes (Early-est mwpd loop).
	const int nAcc = (n - iStart) + 1;     // index 0 == analysis start
	std::vector<double> intPos(nAcc, 0.0);
	std::vector<double> intNeg(nAcc, 0.0);
	double intSum = 0.0, accPos = 0.0, accNeg = 0.0;
	int k = 0;
	for ( int i = iStart; i < n; ++i ) {
		++k;
		intSum += vel[i] * deltaTime;          // displacement [m]
		const double area = intSum * deltaTime; // [m*s]
		if ( intSum >= 0.0 ) {
			accPos += area;
		}
		else {
			accNeg += -area;
		}
		intPos[k] = accPos;
		intNeg[k] = accNeg;
	}

	// =====================================================================
	//  2) High-frequency envelope and the source duration T0 (durRaw).
	// =====================================================================
	double durRaw;          // source-duration estimate [s] (used for the ramp)
	bool   t0Resolved  = false;
	bool   t0Terminated = false; // T0 search hit a termination criterion (final)
	double peakRelSec  = -1.0;   // time of the envelope peak after P (debug)

	if ( _cfg.fixedDuration ) {
		durRaw = _cfg.fixedDurationVal;
		t0Resolved  = true;
		t0Terminated = true;
	}
	else {
		std::vector<double> env(n);
		for ( int i = 0; i < n; ++i ) {
			env[i] = raw[i];
		}
		ButterworthBandpass<double> bp(_cfg.hfOrder, _cfg.hfFmin, _cfg.hfFmax, fsamp);
		bp.apply(n, env.data());
		for ( int i = 0; i < n; ++i ) {
			env[i] *= env[i];                  // square
		}
		const int smoothHW = static_cast<int>(_cfg.smoothHalfWidth * fsamp + 0.5);
		boxcarSmooth(env, smoothHW);
		// NOTE: env stays as the squared (power) envelope -- Early-est's
		// data_float_t50 is squared-then-smoothed and is NOT square-rooted; the
		// 90/80/50/20% level tracking below runs on power, so a "20% of peak"
		// crossing is at 0.2 in power (= ~0.45 in amplitude).

		// Level tracking from the pick onward (Early-est T0 loop).
		double ampNoise = env[iPick];
		if ( ampNoise <= 0.0 ) {
			ampNoise = DBL_MIN;
		}
		double ampPeak = -1.0;
		int    idxPeak = 0;
		double amp90 = -1.0, amp80 = -1.0, amp50 = -1.0, amp20 = -1.0;
		int idx90 = 0, idx80 = 0, idx50 = 0, idx20 = 0;
		const int minDurSamp = static_cast<int>(_cfg.minDuration * fsamp + 0.5);
		const int smooth2    = static_cast<int>(2.0 * _cfg.smoothHalfWidth * fsamp + 0.5);
		durRaw = -1.0;

		for ( int i = iPick; i < n; ++i ) {
			const double amp = env[i];
			const int rel = i - iPick;

			if ( amp > ampPeak ) {             // new peak: unset all levels
				ampPeak = amp;
				idxPeak = rel;
				amp90 = amp80 = amp50 = amp20 = -1.0;
			}
			else {
				if ( amp90 < 0.0 ) {
					if ( amp <= 0.9 * ampPeak ) { amp90 = amp; idx90 = rel; }
				}
				else if ( amp > 0.9 * ampPeak ) {
					amp90 = amp80 = amp50 = amp20 = -1.0;
				}
				if ( amp80 < 0.0 ) {
					if ( amp <= 0.8 * ampPeak ) { amp80 = amp; idx80 = rel; }
				}
				else if ( amp > 0.8 * ampPeak ) {
					amp80 = amp50 = amp20 = -1.0;
				}
				if ( amp50 < 0.0 ) {
					if ( amp <= 0.5 * ampPeak ) { amp50 = amp; idx50 = rel; }
				}
				else if ( amp > 0.5 * ampPeak ) {
					amp50 = amp20 = -1.0;
				}
				if ( amp20 < 0.0 ) {
					if ( amp <= 0.2 * ampPeak ) {
						amp20 = amp; idx20 = rel;
						durRaw = durationFromLevels(idx90, idx80, idx50, idx20,
						                            deltaTime, _cfg.durationFloor);
						t0Resolved = true;
					}
				}
				else if ( amp > 0.2 * ampPeak ) {
					amp20 = -1.0;
				}

				// Terminate once below the S/N or peak ratio (after min dur).
				if ( rel > minDurSamp &&
				     ( amp / ampNoise < _cfg.snT0End ||
				       amp / ampPeak  < _cfg.peakRatioT0End ) ) {
					t0Terminated = true;
					break;
				}
			}

			// Terminate if well past twice the established duration.
			if ( durRaw > 0.0 && amp20 > 0.0 &&
			     rel > static_cast<int>(2.0 * durRaw * fsamp + 0.5) &&
			     rel > smooth2 ) {
				t0Terminated = true;
				break;
			}
		}

		peakRelSec = idxPeak * deltaTime;
	}

	// =====================================================================
	//  3) S-P-capped integration over the source duration T0.
	// =====================================================================
	// Early-est integrates the displacement over min(T0, S-P, MAX_MWPD_DUR)
	// (calculate_Raw_Mwpd_Mag / timedomain_processing.c ~1762). Under progressive
	// updates we only finalize once T0 is stable -- its search terminated
	// (t0Terminated) or the full window has streamed -- otherwise the value would
	// be locked on a transient early T0. Until then, keep waiting (InProgress);
	// if T0 never resolves by window end the base drops the station (MWPD_INVALID).
	if ( durRaw <= 0.0 ) {
		return false;                       // T0 not resolved yet -> wait
	}
	const double availDur = (n - iPick) * deltaTime;
	const bool windowFull = availDur >= _cfg.maxDuration - 1.0;
	if ( !t0Terminated && !windowFull ) {
		return false;                       // T0 still provisional -> wait
	}

	if ( durRaw > _cfg.maxDuration ) {
		durRaw = _cfg.maxDuration;
	}
	const double sp = computeSPSeconds();
	double integDur = durRaw;
	if ( sp > 0.0 && integDur > sp ) {
		integDur = sp;                      // S-P cap on the integration window
	}
	if ( availDur < integDur - 1.0 ) {
		return false;                       // data must cover the window -> wait
	}

	int idxDur = static_cast<int>((integDur + _cfg.analysisPreP) / deltaTime + 0.5);
	if ( idxDur >= nAcc ) {
		idxDur = nAcc - 1;
	}
	if ( idxDur < 1 ) {
		return false;
	}

	double amplitudeIntegral = std::max(intPos[idxDur], intNeg[idxDur]);  // [m*s]
	if ( amplitudeIntegral <= 0.0 ) {
		return false;
	}

	// Emit the integral in nm*s (as Mwp emits nm); carry the source-duration T0
	// as the period (the base divides period by fsamp, so multiply here).
	amplitude->value = amplitudeIntegral * 1.0e9;
	*period = durRaw * fsamp;
	*snr = 1000000.0;

	dt->index = iPick;
	dt->begin = iStart - iPick;
	dt->end   = idxDur + iStart - iPick;

	setStatus(Finished, 100.0);             // T0 stable -> finalize
	SEISCOMP_DEBUG("%s.%s.%s: Mwpd FINAL ampInt=%g nm*s T0raw=%.1fs peak@%.1fs "
	               "S-P=%.1fs integDur=%.1fs win=%.0fs (pos=%g neg=%g) gain=%g",
	               _environment.networkCode.c_str(),
	               _environment.stationCode.c_str(),
	               _environment.locationCode.c_str(),
	               amplitude->value, durRaw, peakRelSec, sp, integDur, availDur,
	               intPos[idxDur] * 1.0e9, intNeg[idxDur] * 1.0e9, gain);

	return true;
}


}
}
}
