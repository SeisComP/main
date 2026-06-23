/***************************************************************************
 * SeisComP duration-amplitude moment-magnitude plugin (mwpd)              *
 *                                                                         *
 * Per-station moment magnitude Mwpd (Lomax & Michelini 2009), porting     *
 * Early-est 1.2.9. The displacement is integrated over the source         *
 * duration T0 (estimated from the high-frequency envelope) rather than    *
 * taking the running-integral peak as Mwp does.                           *
 *                                                                         *
 * GNU Affero General Public License Usage - see LICENSE.                  *
 ***************************************************************************/


#ifndef SEISCOMP_MAGNITUDES_MWPD_PLUGIN_H
#define SEISCOMP_MAGNITUDES_MWPD_PLUGIN_H


#include <seiscomp/core/plugin.h>
#include <seiscomp/core/version.h>
#include <seiscomp/processing/amplitudeprocessor.h>
#include <seiscomp/processing/magnitudeprocessor.h>
#include <seiscomp/seismology/ttt.h>


namespace Seiscomp {
namespace Magnitudes {
namespace Mwpd {


//! Registered amplitude/magnitude type string.
#define MWPD_TYPE "Mwpd"

//! Amplitude unit: the displacement integral over T0, carried in nm*s (as Mwp).
#define MWPD_AMP_UNIT "nm*s"


/**
 * Configuration shared by the amplitude and magnitude processors. Defaults
 * reproduce the hard-coded Early-est 1.2.9 constants
 * (timedomain_processing.c / timedomain_processing_data.c).
 */
struct MwpdConfig {
	// --- BRB-HP restitution + displacement integral (amplitude side) ------
	double gainFrequency   = 0.15;   //!< [Hz] MWPD_GAIN_FREQUENCY: BRB high-pass corner
	int    hpOrder         = 4;      //!< Butterworth high-pass order for the BRB-HP trace
	double analysisPreP    = 1.0;    //!< [s] START_ANALYSIS_BEFORE_P_BRB_HP

	// --- HF envelope (T0 estimator) ---------------------------------------
	// Lomax & Michelini (2009) use a narrow ~1.0 Hz Gaussian filter for the
	// duration envelope (their Fig. 2). A narrow band centred on 1 Hz gives a
	// broad, slowly-decaying envelope and hence the long apparent durations
	// (40-200 s) in their Table 1. (Early-est's operational code instead uses a
	// 1-5 Hz Butterworth, which yields much shorter T0.)
	double hfFmin          = 0.5;    //!< [Hz] envelope band low edge (~1 Hz centre)
	double hfFmax          = 1.5;    //!< [Hz] envelope band high edge
	int    hfOrder         = 4;      //!< envelope band-pass order
	double smoothHalfWidth = 5.0;    //!< [s] envelope boxcar smoothing half-width

	// --- T0 source-duration estimation ------------------------------------
	double minDuration     = 20.0;   //!< [s] MIN_MWPD_DUR: earliest the T0 search may terminate
	double maxDuration     = 300.0;  //!< [s] integration / signal-window length (capped at S-P); Early-est integrates over this window unless a robust T0 shortens it
	double durationFloor   = 0.2;    //!< [s] MINIMUN_DURATION
	double snT0End         = 3.0;    //!< SIGNAL_TO_NOISE_RATIO_T0_END
	double peakRatioT0End  = 0.025;  //!< SIGNAL_TO_PEAK_RATIO_T0_END
	bool   fixedDuration   = false;  //!< use a fixed T0 instead of the HF-envelope estimate
	double fixedDurationVal= 60.0;   //!< [s] the fixed T0 when fixedDuration is true

	// --- magnitude formula + corrections ----------------------------------
	double rho             = 3400.0; //!< [kg/m^3] density in MWP_CONST
	double vp              = 7900.0; //!< [m/s] P velocity in MWP_CONST
	double fp              = 2.0;    //!< FP average radiation factor in MWP_CONST
	bool   useDistanceCorr = true;   //!< apply calculate_Mwp_correction (INGV_EE)
	bool   useDepthCorr    = true;   //!< apply get_depth_corr_mwpd_prem (PREM step)
	bool   useDurationRamp = true;   //!< USE_MPWD_CORR: >90 s duration ramp
	double durRampLow      = 90.0;   //!< [s] MWPD_MOMENT_CORR_DUR_CUTOFF_LOW
	double durRampHigh     = 110.0;  //!< [s] MWPD_MOMENT_CORR_DUR_CUTOFF_HIGH
	double magCutoff       = 7.2;    //!< MWPD_MOMENT_CORR_MAG_CUTOFF
	double rampPow         = 0.45;   //!< MWPD_MOMENT_CORR_POW
	double minDistanceDeg  = 5.0;    //!< accept station magnitudes from this distance
	double maxDistanceDeg  = 105.0;  //!< ...up to this distance

	// --- S-P window cap (Early-est limits the integral to the S-P time) ---
	bool        useSpCap   = true;        //!< cap the displacement integral at the S-P time
	std::string tttBackend = "libtau";    //!< travel-time backend for S-P
	std::string tttModel   = "iasp91";    //!< travel-time model for S-P
};


/** Reads MwpdConfig from a binding. @p prefix is "amplitudes.Mwpd" or
 *  "magnitudes.Mwpd". Missing keys keep their (Early-est) defaults. */
bool readMwpdConfig(const Processing::Settings &settings,
                    const std::string &prefix, MwpdConfig &out);


/** Tsuboi (1995) moment constant, reproduced from Early-est:
 *  4 pi * rho * Vp^3 * FP * (10000/90) deg->km * 1000 km->m. */
double mwpdMomentConstant(const MwpdConfig &cfg);

/** INGV_EE distance correction (subtracted from the raw magnitude),
 *  calculate_Mwp_correction_INGV_EE: 0 if delta<0 or depth>100 km. */
double mwpdDistanceCorrection(double deltaDeg, double depthKm);

/** PREM step depth correction get_depth_corr_mwpd_prem (returns -999 if
 *  the depth is below the first table row). */
double mwpdDepthCorrection(double depthKm);


// ---------------------------------------------------------------------------
//  Amplitude processor (registered as "Mwpd"). Vertical broadband only. From
//  the raw window it builds (a) the BRB-HP displacement and its running
//  integral, separated into positive/negative displacement lobes, and (b) the
//  high-frequency envelope from which the source duration T0 is estimated.
//  Emits the displacement integral over T0 (nm*s) and carries T0 as the
//  amplitude "period".
// ---------------------------------------------------------------------------
class SC_SYSTEM_CLIENT_API AmplitudeProcessor_Mwpd : public Processing::AmplitudeProcessor {
	public:
		AmplitudeProcessor_Mwpd();

	public:
		bool setup(const Processing::Settings &settings) override;
		const DoubleArray *processedData(Component comp) const override;

	protected:
		bool computeAmplitude(const DoubleArray &data,
		                      size_t i1, size_t i2,
		                      size_t si1, size_t si2,
		                      double offset,
		                      AmplitudeIndex *dt,
		                      AmplitudeValue *amplitude,
		                      double *period, double *snr) override;

	private:
		void applyConfig();
		//! Theoretical S-P time [s] at the current source/receiver, or -1.
		double computeSPSeconds() const;

	private:
		MwpdConfig _cfg;
		DoubleArray _processedData;
		Seiscomp::TravelTimeTableInterfacePtr _ttt;
};


// ---------------------------------------------------------------------------
//  Magnitude processor (registered as "Mwpd"). Turns the displacement
//  integral into M0 and Mwpd, applying the distance, PREM depth and duration
//  corrections. The source duration T0 arrives in the amplitude "period".
// ---------------------------------------------------------------------------
class SC_SYSTEM_CLIENT_API MagnitudeProcessor_Mwpd : public Processing::MagnitudeProcessor {
	public:
		MagnitudeProcessor_Mwpd();

	public:
		void setDefaults() override {}
		bool setup(const Processing::Settings &settings) override;

		Status computeMagnitude(double amplitude, const std::string &unit,
		                        double period, double snr,
		                        double delta, double depth,
		                        const DataModel::Origin *hypocenter,
		                        const DataModel::SensorLocation *receiver,
		                        const DataModel::Amplitude *,
		                        const Locale *,
		                        double &value) override;

	private:
		MwpdConfig _cfg;
};


}
}
}


#endif
