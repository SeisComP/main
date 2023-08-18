scautopick applies threshold monitoring by searching for waveform anomalies in
form of changes in amplitudes. It is applied for detecting phase arrivals
creating :term:`phase picks <pick>` and for measuring related features and
:term:`amplitudes <amplitude>`. The picks and associated amplitudes and
features are typically provided to modules like :ref:`scautoloc` for locating
the source.

.. note::

   Instead of detecting phase arrivals for source location, scautopick
   can also be applied for detecting simple amplitude exceedence applying filters
   like the :py:func:`MAX` filter. Exceedences are reported as picks and can be
   processed further, e.g. by :ref:`scalert`.


Phase Detections
================

scautopick detects phase onsets for generating :term:`picks <pick>`. Initally,
it searches for detections on the waveform streams defined by global bindings.


P picks
-------

A primary detector is applied first. When a detection is found, 'P' is by default
assigned to the guess of the phase type (phaseHint). The actual guess can be configured by
:confval:`phaseHint`. By default the primary detector applies a robust STA/LTA
detector (:py:func:`STALTA` filter) to waveforms for making detections. Other
detection filters and filter chains can be choosen from the
:ref:`list of SeisComP filters <filter-grammar>`. A guess of the pick type may
be defined by :confval:`phaseHint`.

Waveforms are typically :ref:`pre-filtered <filter-grammar>` before the actual
:py:func:`STALTA` filter. Without further configuration a
running-mean highpass, a cosine taper and a Butterworth bandpass filter of
third order with corner frequencies of 0.7 and 2 Hz are applied before the
:py:func:`STALTA` filter. The entire filter sequence is configurable by
:confval:`filter`, module configuration, or :confval:`detecFilter`, binding
configuration.

Once the STA/LTA ratio has reached a configurable threshold (by default 3) for a
particular stream, a :term:`pick` is set to the time when this
threshold is exceeded (pick time) and the picker is set inactive. The picker is
reactivated for this stream once the STA/LTA ratio falls to the value of 1.5 (default).

The trigger thresholds are configurable:

* Trigger on: :confval:`thresholds.triggerOn` in module configuration or
  :confval:`trigOn` in binding configuration,
* Trigger off: :confval:`thresholds.triggerOff`, module configuration or :confval:`trigOff`,
  binding configuration.

Initial detections can be further adjusted by a second-stage phase re-picker
(post picker) as defined by :confval:`picker`. The re-picker should be tuned
carefully and global bindings parameters :confval:`picker.*` should be
configured accordingly.

After having detected a phase, the re-picker will be inactive and accept no further
detection until

* The amplitudes measured after filtering (:confval:`filter` in module configuration
  or :confval:`detecFilter` in binding configuration) fall below the
  :confval:`thresholds.triggerOff` (module configuration) or :confval:`trigOff`
  (binding configuration) and
* Amplitudes, :math:`A_{trigger}`, measured after filtering reach or
  exceed a threshold determined by :math:`T_{minOffset}` (:confval:`thresholds.minAmplOffset`),
  :math:`T_{dead}` (:confval:`thresholds.deadTime`) and the amplitude of the
  previous pick, :math:`A_{prev}`:

  .. math ::

     A_{trigger} \ge T_{minOffset} + A_{prev} * exp\left(-(dt/T_{dead})^2\right)

  if :math:`T_{dead} > 0`. Otherwise:

  .. math ::

     A_{trigger} \ge T_{minOffset}

  Here, :math:`dt` is the time passed since the last pick.
  :math:`T_{minOffset}` (:confval:`thresholds.minAmplOffset`) is typically similar to
  the trigger threshold, :confval:`thresholds.triggerOn` (module configuration) or
  :confval:`trigOn` (binding configuration).


S picks
-------

Based on the inital detection or pick a secondary picker may applied be applied,
e.g., for picking S phases as defined by :confval:`spicker`. The secondary picker
is halted as soon as new detections are made unless :confval:`killPendingSPickers`
is inactive.

As for the re-picker also the spicker should be tuned carefully and global
bindings parameters :confval:`spicker.*` should be set.

.. csv-table:: Second-stage pickers available by configuration of :confval:`picker` or :confval:`spicker`
   :align: center
   :delim: ,
   :widths: 1 3 1 1 3
   :header: "picker name", "phase", "picker", "spicker", "global bindings parameters"

   "AIC", "P, configurable: :confval:`phaseHint`", "x", "", "picker.AIC.*"
   "BK", "P, configurable: :confval:`phaseHint`", "x", "", "picker.BK.*"
   "S-L2", "S", "", "x", "spicker.L2.*"


Feature extraction
------------------

For extracting features related to picks such as polarization parameters
configure :confval:`fx` and the related global bindings parameters :confval:`fx.*`.


Amplitude Measurements
======================

The second task of scautopick is to calculate amplitudes of a given type for the
corresponding magnitude type (see :ref:`scamp` for a list of amplitude types and
:ref:`scmag` for the magnitude types). Such amplitudes are required by:

* :ref:`scautoloc` for associating phase picks and generating a source location
* EEW (earthquake early warning) systems in order to provide rapid amplitudes for
  magnitudes as soon as source locations are available.

The time window for measuring amplitudes starts at the pick time. The window
length is constant and specific to the amplitude type. It can be adjusted in
global bindings. For example mb is calculated
for a fixed time window of 30 s after the pick, mB for time window of 60s, for
MLv a time window of 150 s is estimated to make sure that S-arrivals are inside
this time window. The pre-calculated amplitudes are sent out and received by
the magnitude tool, :ref:`scmag`.
The fixed time window poses a limitation to EEW system. Howver, a speed-up is
available with :confval:`amplitudes.enableUpdate`.
Read the :ref:`scamp` documentation for more details on amplitude measurements.


Modes of Operation
==================

scautopick usually runs in the background connected to a real-time data source
such as :ref:`Seedlink <seedlink>`. This is referred to as online mode. Another
option to run scautopick is on offline mode with files.


Real-time
---------

In real-time mode the workflow draws like this:

* scautopick reads all of its binding parameters and subscribes to stations
  defined by global binding parameters where :confval:`detecEnable` is set to ``true``.
* The data time window requested from the data source is [system-:confval:`leadTime`, NULL]
  meaning an open end time that causes :ref:`SeedLink <seedlink>` to stream
  real-time data if no more data are in the buffers.
* Each incoming record is filtered according to :confval:`detecFilter`.
* The samples are checked for exceedance of :confval:`trigOn` and in the positive
  case either a post picker (:confval:`picker`) is launched or a :term:`Pick <pick>`
  object will be sent.
* If :confval:`sendDetections` is set to ``true``, a trigger will be sent in any
  case for e.g. debugging.
* After the primary stage has finished (detector only or picker) secondary
  pickers will be launched if configured with :confval:`spicker`.

These steps repeat for any incoming record.

To run scautopick in the background as a daemon module enable and start it ::

$ seiscomp enable scautopick
$ seiscomp start scautopick

For executing on the command line simply call it with appropriate options, e.g. ::

   $ seiscomp exec scautopick -h


Non-real-time
-------------

.. note::

   Due to code changes in the file data source, the command line option
   :option:`--playback` is essential for non-real-time operation. Otherwise a
   real-time time window is set and all records are most likely filtered out.

To tune scautopick or to do playbacks it is helpful to run scautopick not with
a real-time data source but on a defined data set, e.g. a multiplexed sorted miniSEED
volume. scautopick will apply the same workflow as in online mode but the
acquisition of data records has to change. If the input data (file) has been
read, scautopick will exit and furthermore it must not ask for a particular
time window, especially not for a real-time time window. To accomplish that
the command-line parameter :option:`--playback` has to be used. Example:

.. code-block:: sh

   $ scautopick --playback -I data.mseed

This call will process all records in :file:`data.mseed` for which bindings
exist and **send the results to the messaging**. If all data records are processed,
scautopick will exit. The processing steps are similar to the online mode.

Use the :option:`--ep` for offline processing **without messaging**. The results are
printed in :term:`SCML` format. Example:

.. code-block:: sh

   $ scautopick --playback -I data.mseed --ep -d [type]://[host]/[database] > picks.xml
