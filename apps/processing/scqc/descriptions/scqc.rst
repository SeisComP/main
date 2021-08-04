scqc determines waveform quality control (QC) parameters of data streams. The
output parameters are time averaged in terms of waveform quality messages.
In regular intervals report messages are sent containing the short term average
representation of the specific QC parameter for a given time span. Alarm messages
are generated if the short term average (e.g. 90 s) of a QC parameter differs from
the long term average (e.g. 3600 s) by more than a defined threshold.
To avoid an excessive load, QC messages are sent distributed over time. Storing
QC parameters into the database should be avoided by configuring, e.g.,
:confval:`plugins.default.archive.interval` as otherwise the database grows
quickly.

The QC parameters can be interactively observed using :ref:`scqcv` or :ref:`scmm`.


Technology
==========

scqc uses :term:`plugins <plugin>` to compute the waveform quality control (QC)
parameters. The plugins can be found in @DATADIR@/plugins/qc. They are loaded
by default or selected by :ref:`configuration setup <scqc-setup>`.


QC parameters
=============

.. _scqc-fig-times:

.. figure:: media/times.png
   :align: center
   :width: 16cm

   Times describing the data records, e.g., for calculating
   :ref:`delay<scqc-delay>` or :ref:`latency<scqc-latency>`.
   :math:`t_{now}` may be measured differently depending on the QC parameter.

The following QC parameters are determined when the corresponding plugin is
loaded:

.. _scqc-availability:

availability [%]
 **Plugin:** qcplugin_availability

 **Description:** Availability of data in the configured time span.

.. _scqc-delay:

delay [s]
 **Plugin:** qcplugin_delay

 **Description:** Time difference between current time, :math:`t_{now}`, and
 time of the last record in the |scname| system (see :ref:`Figure<scqc-fig-times>`):

 .. math::

   delay = t_{now} - t_{2}.

 .. note ::

    Current time is measured during reception of a record and updated in the
    report intervals configured for delay.

.. _scqc-gaps:

gaps (count [counts], interval [s], length [s])
 **Plugin:** qcplugin_gap

 **Description:** In case of a data gap between two consecutive records this
 parameter delivers the gap interval time (interval), the mean length of the gaps
 (length) and the number of the gaps (count) over the configured time interval.

 .. hint ::

    gaps interval = 0 for gaps count = 1 but gaps interval > 0
    for gaps count > 1. This may be important when evaluating the intervals.

.. _scqc-latency:

latency [s]
 **Plugin:** qcplugin_latency

 **Description:** Time difference between current time, :math:`t_{now}`, and
 arrival time of the last record (see :ref:`Figure<scqc-fig-times>`):

 .. math::

   latency = t_{now} - t_{arr}.

 .. note ::

    Current time is measured during reception of a record and updated in the
    report intervals configured for latency.

.. _scqc-offset:

offset [counts]
 **Plugin:** qcplugin_offset

 **Description:** Average value of all samples of a record.

.. _scqc-outage:

outage [time string]
 **Plugin:** qcplugin_outage

 **Description:** Delivers the start and the end time of a data outage (gap).

.. _scqc-overlap:

overlaps (count [counts], interval [s], length [s])
 **Plugin:** qcplugin_overlap

 **Description:** In case of overlaps between two consecutive records this parameter
 delivers the overlaps interval time (interval), the mean length (length) of the
 overlaps and the number of the overlaps (count) over the configured time interval.

 .. hint ::

    overlaps interval = 0 for overlaps count = 1 but overlaps interval > 0
    for overlaps count > 1. This may be important when evaluating the intervals.

.. _scqc-rms:

rms [counts]
 **Plugin:** qcplugin_rms

 **Description:** Offset corrected root mean square (RMS) value of a record.

.. _scqc-spike:

spikes (count [counts], interval [s], amplitude [counts])
 **Plugin:** qcplugin_spike

 **Description:** In case of the occurrence of a spike in a record this parameter
 delivers the time interval between consecutive spikes (interval), the mean
 amplitude of the spike (amplitude) and the number of the spikes (count) over the
 configured time interval. Internally a list of spikes is stored (spike time,
 spike amplitude); the spike finder algorithm is still preliminary.

 .. hint ::

    spikes interval = 0 for spikes count = 1 but spikes interval > 0
    for spikes count > 1. This may be important when evaluating the intervals.

.. _scqc-timing:

timing [%]
 **Plugin:** qcplugin_timing

 **Description:** miniSEED record timing quality (0 - 100 %) as written into the
 miniSEED records by the digitizer.


.. _scqc-setup:

Setup
=====

The configuration can be adjusted in the module configuration (:file:`scqc.cfg`).

#. Select the streams for which to compute the QC parameters. Be default, only
   streams defined global bindings are considered:

   * :confval:`use3Components`: Select to consider the 3 components related to
     the stream defined by global bingings. Reguires to **select**
     :confval:`useConfiguredStreams`.
   * :confval:`streamMask`: Freely choose any available stream to compute the QC
     parameters for. This requires to **unselect** :confval:`useConfiguredStreams`.
     Regular expressions may be used, e.g.:

     * Consider BHZ streams from all networks, stations and locations: ::

          streamMask = ""(.+)\.(.+)\.(.*)\.(BHZ)$"

     * Consider any component from BH, LH and HH streams: ::

          streamMask = "(.+)\.(.+)\.(.*)\.((BH)|(LH)|(HH))?$"

#. Load the QC and other plugins, e.g. for data acquisition.
   The QC plugins loaded by default are :code:`qcplugin_availability`,
   :code:`qcplugin_delay`, :code:`qcplugin_gap`, :code:`qcplugin_latency`,
   :code:`qcplugin_offset`, :code:`qcplugin_outage`, :code:`qcplugin_overlap`,
   :code:`qcplugin_rms`, :code:`qcplugin_spike` and :code:`qcplugin_timing`.

   Adjust :confval:`plugins` for setting plugins explicitly and to exclude the other
   ones, e.g. ::

      plugins = qcplugin_availability, qcplugin_spike

   Further :term:`RecordStream` plugins for reading data may be considered.

#. Configure the plugin parameters: Each plugin considers specific parameters
   which can be configured separately or the default section.
