scamp measures several different kinds of amplitudes from waveform data.
It listens for origins and measures amplitudes in time windows determined
from the origin. Thus, in contrast to amplitudes measured by :ref:`scautopick`
the considered time windows can depend on epicentral distance.
The resulting amplitude objects are sent to the "AMPLITUDE"
messaging group. scamp is the counterpart of :ref:`scmag`. Usually, all
amplitudes are computed at once by scamp and then published.
Only very rarely an amplitude needs to be recomputed if the location of an
origin changes significantly. The amplitude can be reused by :ref:`scmag`, making
magnitude computation and update efficient. Currently, the automatic picker
in SeisComP, scautopick, also measures a small set of amplitudes
(namely "snr" and "mb", the signal-to-noise ratio and the amplitude used in
mb magnitude computation, respectively) for each automatic pick in fixed
time windows. If there already exists an amplitude, e.g. a previously determined
one by scautopick, scamp will not measure it again for the respective stream.

Amplitudes are also needed, however, for manual picks. scamp does this as well.
Arrivals with weight smaller than 0.5 (default) in the corresponding Origin are
discarded. This minimum weight can be configured with
:confval:`amptool.minimumPickWeight`.


Amplitude Types
===============

Amplitudes of the following types are currently computed for their corresponding
magnitudes.


Local distances
---------------

:term:`Md <magnitude, duration (Md)>`
   Duration magnitude as described in https://earthquake.usgs.gov/research/software/#HYPOINVERSE

:term:`Mjma <magnitude, JMA (M_JMA)>`
   Mjma is computed on displacement data using body waves of period < 30s.

:term:`ML <magnitude, local (ML)>`
   Local magnitude calculated on the vertical component using a correction term
   to fit with the standard ML.

:term:`MLh <magnitude, local horizontal (MLh)>`
   Local magnitude calculated on the horizontal components to SED specifications.

:term:`MLv <magnitude, local vertical (MLv)>`
   Local magnitude calculated on the vertical component using a correction term
   to fit with the standard ML.

:term:`MLr <magnitude, local GNS/GEONET (MLr)>`
   Local magnitude calculated from MLv amplitudes based on GNS/GEONET specifications
   for New Zealand.

:term:`AMN <magnitude, Nuttli (MN)>`
   Nuttli magnitude for Canada and other Cratonic regions.


Teleseismic distances
---------------------

:term:`mb <magnitude, body-wave (mb)>`
   Narrow band body wave magnitude measured on a WWSSN-SP filtered trace

:term:`mBc <magnitude, cumulative body-wave (mBc)>`
   Cumulative body wave magnitude

:term:`mB <magnitude, broadband body-wave (mB)>`
   Broad band body wave magnitude

:term:`Mwp <magnitude, broadband P-wave moment (Mwp)>`
   The body wave magnitude of Tsuboi et al. (1995)

:term:`Ms_20 <magnitude, surface wave (Ms_20)>`
   Surface-wave magnitude at 20 s period

:term:`Ms(BB) <magnitude, broadband surface wave (Ms(BB))>`
   Broad band surface-wave magnitude

Note that in order to be used by scmag, the input amplitude names for the
various magnitude types must match exactly with the exception of the AMN amplitudes
required by the :term:`MN (Nuttli) magnitude <magnitude, Nuttli (MN)>`.


Acceleration Input Data
=======================

For amplitudes to be computed, the input waveforms are usually given in velocity.
Acceleration data, e.g. from strong-motion instruments must therefore be transformed
to velocity. The transformation is enabled by activating the response correction.
Activate the correction in the global bindings for all
types or in a new Amplitude type profile for specific types.

Example global binding parameters for computing MLv amplitudes from accleration
data. Here, the frequency range is limited to 0.5 - 20 Hz: ::

   amplitudes.MLv.enableResponses = true
   amplitudes.MLv.resp.taper = 5
   amplitudes.MLv.resp.minFreq = 0.5
   amplitudes.MLv.resp.maxFreq = 20


Re-processing
=============

*scamp* can be used to reprocess and to update amplitudes, e.g. when inventory paramters
had to be changed retrospectively. Updating ampitudes requires waveform access.
The update can be performed

1. In **offline processing** based on XML files (:confval:`--ep`). :confval:`--reprocess<reprocess>`
   will replace exisiting amplitudes. Updated values can be dispatched to the messing by
   :ref:`scdispatch` making them available for further processing, e.g. by :ref:`scmag`.

   **Example:**

   .. code-block:: sh

      $ scamp --ep evtID.xml -d [type]://[host]/[database] --reprocess > evtID_update.xml
      $ scdispatch -O merge -H [host] -i evtID_update.xml

#. **With messaging** by setting :confval:`start-time` or :confval:`end-time`.
   All parameters are read from the database. :confval:`--commit<commit>` will
   send the updated parameters to the messing system making them available for
   further processing, e.g. by :ref:`scmag`. Otherwise, XML output is generated.

   **Example:**

   .. code-block:: sh

      $ scamp -u testuser -H [host] --commit \
              --start-time '2016-10-15 00:00:00' --end-time '2016-10-16 19:20:00'
