scqcv provides both, a brief overview of the seismometer network status and a
detailed view of specific stream parameters. Quality Control (QC) parameters,
determined by :ref:`scqc` and sent via messaging system, are received and displayed.

scqcv allows to interactively disable or to enable streams for automatic data processing
based on the observed QC reports.


Detailed QC reports per station
===============================

:ref:`fig-tab-view` shows the tabular view of the QC report messages.
Each QC parameter is shown in a column. The default sorting by stream code can
be changed by pressing a header field of a parameter. A widget displaying the
currently received waveform data is shown by pressing the cell with the streamID.
Positioning the mouse over the parameter cell a ToolTip indicates
more detailed information on the selected parameter. Typing a regular
expression in the "StreamIDFilter" text entry field results in a stream code
filter, only displaying the matching stream codes with QC parameter. Green
colored fields indicate that the QC parameter values lie within the configured
"good" interval. Red colors indicate that the QC parameters lie outside the
tolerated value interval -- this stream might have an issue. All :ref:`colors are
configurable <scqcv-sec-setup>`.
Click on the table header to sort by the selected value or drag the columns to
another position. The order of the columns is controlled by :confval:`parameter`.
By clicking on a streamID, the past waveforms are displayed at length configured
by :confval:`streamWidget.length`.

To **disable / enable** a station click on the respective station field in the
*enabled* column.

.. _fig-tab-view:

.. figure:: media/scqcv/Tabulator_view_of_scqcv.png
   :width: 16cm
   :align: center

   Tabulator view of scqcv


Station overview
================

:ref:`fig-status-overview` shows the status overview grouped by network code.
The status is color coded and the color is derived from a
:ref:`score <scqcv-sec-scoring>` per station.

The more the color usually varies from green to dark red, the worse the waveform data might be.
A dark red color indicates a stream with low quality, e.g. high latency.
Light to darker red
represents a badness sum of related QC parameters. Colors are subject to
changes in near future and are configurable. Pressing a stream code item opens a
single line table
with detailed information of the selected stream. Again it is possible to open
a real time waveform widget by pressing the leading header field indicating
the stream code.

Typing a regular expression in the bottom text entry field
results in a stream code filter, only displaying the matching stream codes.
Disabled stations are crossed out. Click on a stream field to view the detailed
QC parameters where stations can also be enabled / disabled.

.. _fig-status-overview:

.. figure:: media/scqcv/Status_overview_of_scqcv.png
   :width: 16cm
   :align: center

   Status overview of scqcv with a stream widget

The compact status overview allows a quick impression of the present status of
all received streams (:ref:`fig-compact-status`). Functionality is equal to the
status overview grouped by network.

Switch between compact view and
network seperated view by clicking the checkbox in the bottom line.

.. _fig-compact-status:

.. figure:: media/scqcv/Compact_status_overview_of_scqcv.png
   :width: 16cm
   :align: center

   Compact status overview of scqcv


.. _scqcv-sec-scoring:

Scoring
-------

The score is formed per station as the sum of the counts for the parameters defined,
e.g., by :confval:`score.default`. The counts are defined per QC parameter by the
`count` parameter of the applicable range, e.g. :confval:`timing.range.$name.count`.


.. _scqcv-sec-setup:

Setup
=====

While some important parameters can be configured using :ref:`scconfig` the configuration
of others is available by examples in the extensive default configuration of scqcv in
*@DEFAULTCONFIGDIR/scqcv.cfg@*

Apply your setup to scqcv.cfg in @SYSTEMCONFIGDIR@ or in @CONFIGDIR@.
If the parameters are not configured, the defaults configuration will be considered.


Message groups and QC parameters
--------------------------------

Select the desired parameters from the list below. "#" disables a parameter.

.. code-block:: sh

   connection.primaryGroup = QC
   connection.subscription = QC, CONFIG

   parameter = 	"latency           : latency",\
   				"delay             : delay",\
   				"timing quality    : timing",\
   				"offset            : offset",\
   				"rms               : rms",\
   				"gaps count        : gap",\
   				"overlaps count    : overlap",\
   				"availability      : availability",\
   				"spikes count      : spike"
   #				"gaps interval     : gapInterval",\
   #				"gaps length       : gapLength",\
   #				"spikes interval   : spikeInterval",\
   #				"spikes amplitude  : spikeAmplitude"
   #				"overlaps interval : overlapInterval",\
   #				"overlaps length   : overlapLength"


Stream selection
----------------

By default all streams configured by the global bindings will be displayed. To limit
the streams or to use a specific list configure :confval:`streams.codes` and
:confval:`streams.cumulative`.

Example configuration or the AM network:

.. code-block:: sh

   # List of channels to display. By default the global configuration is used
   # which can be overwritten with this parameter.
   streams.codes = AM.*.*.*

   # Add new streams from WfQ automatically to the list of stream configured in
   # streams.codes.
   streams.cumulative = false


Properties of QC parameters
---------------------------

Configure intervals, values and format and background colors for QC parameters to
display in :file:`scqcv.cfg`.

In the configuration the QC parameter is referred to by its unique ConfigName. You may
generate structures for each parameter starting with its ConfigName. The structures
contain all configuration parameters. Example for the QC parameter *timing quality*
referred to as *timing*:

.. code-block:: sh

   timing.ranges = sane, inter, bad
   timing.format = int
   timing.expire = 600
   timing.useAbsoluteValue = false

   timing.range.sane = 90.0, 100.0
   timing.range.inter = 50.0, 90.0

   timing.range.bad.count = -100
   timing.range.bad.color = darkred

   timing.range.inter.count = -1
   timing.range.inter.color = yellow

   timing.range.sane.count = 0
   timing.range.sane.color = green


The mapping of parameter names to ConfigName is configurable by
:confval:`parameter` but the default mapping is available in
:file:`@DEFAULTCONFIGDIR@/scqcv.cfg`.

.. csv-table:: Default mapping of parameter names.

   "Parameter name","ConfigName"
   "latency","latency"
   "delay","delay"
   "timing quality","timing"
   "offset","offset"
   "rms","rms"
   "gaps count","gap"
   "overlaps count","overlap"
   "availability","availability"
   "spikes count","spike"
   "gaps interval","gapInterval"
   "gaps length","gapLength"
   "spikes interval","spikeInterval"
   "spikes amplitude","spikeAmplitude"
   "overlaps interval","overlapInterval"
   "overlaps length","overlapLength"

