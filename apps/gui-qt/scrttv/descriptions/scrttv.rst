scrttv visualizes waveform data (see :ref:`Figure below <fig-scrttv-overview>`)
in real-time, from archives or files of a defined window length (default: 30
minutes) and of defined streams/stations (default: streams defined by global
bindings). scrttv dynamically switches between two modi.
In the normal mode the trace order is given by the configuration file.
In the event mode the traces are sorted by epicentral distance to the
latest origin received from the messaging. In addition to waveforms
information about gaps or overlaps, picks and the time of incoming origins are
displayed.


.. _fig-scrttv-overview:

.. figure:: media/scrttv/scrttv.png
   :width: 16cm
   :align: center

   scrttv overview

   An example of scrttv and the dialog window to assoicate picks to new origins.
   Tabs: Enable/Disable; Amplitude: mean and maximum;
   Stream: station, network, sensor location and channel code;
   Filter: filter applied traces; Status = connection status to messaging.

scrttv shows two tabs: the Enabled and the disabled tab
(see :ref:`fig-scrttv-overview`). Stations listed in the disabled tab
are excluded from automatic processing (e.g. phase picking). To move a station
from one tab to another just drag and drop the trace to the new tab. An alternative solution is
to double click on the trace label to disable a trace. Read the section
:ref:`scrttv-waveform-qc` for the details.

Normally, the raw data are displayed. Pressing :kbd:`f` the predefined bandpass filter
of third order from 0.5 Hz to 8 Hz, :ref:`BW(3,0.5,8) <filter-bw>` is applied
to the traces. Also zoom functions for the time and amplitude axis are provided.
Read the sections :ref:`<scrttv-filtering>` and  :ref:`scrttv-visualization` for
more details.

Among the configurable parameters are:

* Global :term:`bindings <binding>`:

  * default definition of traces to show (:confval:`detecStream` and :confval:`detecLocid`).

* :term:`Module <module>` configuration:

  * network, stations, locations and streams to show extending or overriding the
    default definition (:confval:`streams.codes`),
  * :ref:`data filters <scrttv-filtering>`,
  * buffer size controlling the lenght of loaded data (:confval:`bufferSize`),
  * sorting of traces upon arrival of new origins (:confval:`resortAutomatically`),
  * reference coordinate for sorting traces by default (:confval:`streams.sort.*`),
  * region filters (:confval:`streams.region.*`),
  * :ref:`grouping of streams <scrttv-grouping>` with different properties,
  * number of traces to show with fixed height (:confval:`streams.rows`).

* Scheme parameters in global :term:`module` configuration:

  * trace properties and trace background colors,
  * font and general GUI parameters.

More parameters are available on the command-line:

.. code-block:: sh

   scrttv -h


.. _scrttv-modes:

Modes of Operation
==================

scrttv can be started in message mode or in offline mode.

* Message mode: scrttv is started normally and connects to the messaging,
  :term:`picks <picks>`, :term:`origins <origin>` and inventory are read from
  the database and received in real time from the messaging. Data received from
  :term:`recordstream`.
* Offline mode: scrttv is started without connection to the messaging,
  :term:`picks <picks>` and :term:`origins <origin>` are not received in real
  time from the messaging. However, they can be loaded from XML files using the
  *File* menu. The offline mode is invoked when using the option
  :option:`--offline` or when passing a file name to scrttv at startup, e.g.,

  .. code-block:: sh

     scrttv file.mseed


.. _scrttv-visualization:

Waveform Visualization
======================


Stream selection
----------------

Withouth further configuration scrttv displays waveforms for streams defined
in global bindings. The selection can be refined by configuring
:confval:`streams.codes`.

Streams with :ref:`data latency <scqc>` < :confval:`maxDelay` are hidden but
shown again when applicable. By default this parameter is inactive. For listing
streams hidden from one tab press :kbd:`h`.


.. _scrttv-time-windows:

Time Windows
------------

The reading waveforms from RecordsStream, the data is displayed for a time
window which by default ends at current time or as given by the command-line
option :option:`--end-time`. Initially, the time window takes the length defined
in :confval:`bufferSize` or by the option :option:`--buffer-size`. When reading data
directly from file in offline mode, the time window is set
from the time limits of the waveforms.

* The **length of visible time windows** can be adjusted by
  :ref:`zooming <scrttv-zooming>`.
* The **end time of the data** progresses in continuously in real time (UTC)
  with the time of the computer clock unless fixed (:kbd:`F8`). The end time is
  fixed during startup when applying :option:`--end-time`.
* For **progressing or rewinding by 30 minutes** press :kbd:`Alt right` or
  :kbd:`Alt left`, respectively. Data will be loaded immediately.
* You may also **freely zoom** into any time window. Data and picks will be loaded
  when pressing :kbd:`Ctrl + r`
* **Return to default real-time processing** by pressing :kbd:`Ctrl + Shift + r`
  or :kbd:`N`.

.. hint::

   Gaps and overlaps in waveforms are indicated by yellow and purple areas,
   respectively. The colors are configurable.


.. _scrttv-zooming:

Zooming
-------

Waveforms can be zoomed in and out interactively in amplitude and time. Use the
*View* menu or refer to the section :ref:`scrttv-hot-keys` for options.

In addition to the actions available from the View menu, zooming is supported
mouse actions:

* Zooming in in time: Right-click on time axis, drag to the right. A green bar appears
  which is the new time window. Dragging up or down (gray bar) disables zooming.
* Zooming out in time: Right-click on time axis, drag to the left. A red bar appears. The
  longer the bar, the more you zoom out.  Dragging up or down (gray bar)
  disables zooming.
* Zooming in time and amplitude: Mouse over a trace of interest, use
  :kbd:`Ctrl + mouse wheel` for zooming in or out.
* Zooming around a selected area: Press :kbd:`z` and drag an area with while
  pressing the left mouse button. Press :kbd:`z` again for leaving the zoom
  mode.


.. _scrttv-grouping:

Stream Grouping
---------------

scrttv allows grouping of stations and even streams with different properties,
e.g. colors or color gradients.

.. _scrttv-fig-group-filter:

.. figure:: media/scrttv/groups.png
   :width: 16cm
   :align: center

   Stations with 2 groups and different line color gradients. Ungrouped stations
   are visible with default line properties. The applied filter
   is shown in the lower left corner. The tooltip on top of station CX.PB19
   is derived from :confval:`streams.group.$name.title`.


**Configuration**

Adjust the scrttv module configuration (:file:`scrttv.cfg`).

#. Define the groups:

   * add a new group profile to :confval:`streams.group`.
   * set the properties for this group profile. :term:`Colors <color>` and color
     gradients are defined by hexadecimal values or by
     :term:`color keyword name`.
     When choosing gradients the colors of the traces within one group will be
     varied in alphabetic order of the streams.
   * set a group title in :confval:`streams.group.$name.title`.

#. Register the groups in :confval:`streams.groups`.


**Viewing groups**

#. Open :program:`scrttv` to view the data.
#. Select *Sort by group* in the *Interaction* menu or use the hotkey :kbd:`5`
   to sort the traces by their groups.
#. Mouse over a station belonging to a group. The tooltips shows the group title.
#. For maintaining the sorting by groups adjust the :program:`scrttv` module
   configuration (:file:`scrttv.cfg`): ::

      resortAutomatically = false


.. _scrttv-picks:

Phase Picks and Arrivals
------------------------

Previous versions of scrttv (< 5.4) only displayed :term:`picks <pick>` with the
colors indicating the pick evaluation mode along with the phase hint of the
pick:

* red: automatic,
* green: manual.

This hasn't really changed in later versions but additionally scrttv determines
an additional state of a pick called :term:`arrival`. In scrttv a pick is
considered an arrival if it is associated to an valid origin. An origin is
called valid if its evaluation status is not REJECTED. When scrttv loads all
picks from the database for the currently visible time span it also checks if
each pick is associated with a valid origin and declares the arrival state if
the check yields true. The visibility of picks and arrivals can be toggled by
pressing :kbd:`Ctrl + p` and :kbd:`Ctrl + a`, respectively. :kbd:`c` removes all
markers. The configuration paramter :confval:`showPicks` controls the default
visibility.

Picks and arrivals can be differentiated visually by their colours. When
configured in global module configuration, the same colours are being used
consistently as in any other GUI displaying both types, namely

* :confval:`scheme.colors.picks.automatic`
* :confval:`scheme.colors.picks.manual`
* :confval:`scheme.colors.picks.undefined`
* :confval:`scheme.colors.arrivals.automatic`
* :confval:`scheme.colors.arrivals.manual`
* :confval:`scheme.colors.arrivals.undefined`

That visual difference should support the operator in finding clusters of picks
and creating new location missed by the automatic system.

The next sections will only use the :term:`pick` which can be used
interchangeable for pick or arrival.


.. _scrttv-record-borders:

Record Borders
--------------

The borders of records are toggled by using the hotkey :kbd:`b`.

.. figure:: media/scrttv/borders.png
   :width: 16cm
   :align: center

   Record borders in box mode on top of waveforms.

Border properties can be adjusted and signed records can be visualized by colors
configured in the scheme parameters in :file:`global.cfg` or :file:`scrttv.cfg`:

* :confval:`scheme.records.borders.drawMode`: Define where to draw borders, e.g. on top, bottom or as boxes.
* :confval:`scheme.colors.records.borders.*`: Define pen and brush properties.


.. _scrttv-waveform-qc:

Waveform Quality Control
========================

Use scrttv for regular visual waveform inspection and for enabling or disabling
of stations. Disabled stations will not be used for automatic phase detections
and can be excluded from manual processing in :ref:`scolv`. They will also be
highlighted in :ref:`scmv` and :ref:`scqc`.

To enable or disable a station for automatic data processing in |scname| select
a station code with the mouse and drag the stations to the disable / enable tab
or simply double-click on the station code.


Stream Processing
=================


.. _scrttv-filtering:

Filtering
---------

scrttv allows filtering of waveforms.
The Filter selection dropdown menu  (see :ref:`Figure above <fig-scrttv-overview>`)
and the hotkey :kbd:`f` can be used to toggle the list of filters pre-defined in
:confval:`filter` or in :confval:`filters`.  The applied filter is named in the
lower left corner. To show filtered and raw data together use the hotkey :kbd:`r`.

.. note::

   The list of filters defined in :confval:`filters` overwrites :confval:`filter`.
   Activate :confval:`autoApplyFilter` to filter all traces at start-up of scrttv
   with the first filter defined in :confval:`filters`.



Gain correction
---------------

The stream gain is applied to waveforms and amplitude values are given in the
physical units of the stream by default. For showing amplitudes in counts,
deactivate the option *Apply gain* in the Interaction menu.


Interactive signal detection
============================

Beside visual inspection of waveforms for quality control, scrttv can also be
used for interactive signal detection in real time or for selected time windows
in the past.


.. _scrttv-artificial-origins:

Artificial Origins
------------------

.. figure:: media/scrttv/artificial-origin.png
   :width: 16cm
   :align: center

   Artifical origin.

In case the operator recognizes several seismic signals which shall be processed
further, e.g. in :ref:`scolv`, an artificial/preliminary origin can be set by
either pressing the middle mouse
button on a trace or by opening the context menu (right mouse button) on a trace
and selecting "Create artificial origin". The following pop-up window shows the
coordinates of the selected station and the time the click was made on the
trace. Both are used to generate the new artificial origin without any arrivals.
Pressing "Create" sends this origin to the LOCATION group. This artificial
origin is received e.g., by :ref:`scolv` and enables an immediate manual analysis
of the closest traces.

In order to send receive articifial origins and receive them in other GUIs
:confval:`commands.target` of the global module configuration must be set and
must be in line with :confval:`connection.username` of the receiving GUI module.

Alternatively picks can be selected and origins can be located as preliminary
solution to be sent to the system as regular origin objects, see section
:ref:`scrttv-origin-association`.


.. _scrttv-origin-association:

Origin Association
------------------

scrttv comes with a minimal version of a phase associator and manual locator
(Fig. :ref:`fig-scrttv-overview`). Picks can be selected, relocated and
committed to the messaging system as manual preliminary location.
In contrast to the artificial origin operation which requires an immediate
intervention with, e.g. :ref:`scolv`, this operation allows to store all those
detected origins and work on them later because they will be stored in the
database.

.. note::

   More detailed waveform and event analysis can be made in :ref:`scolv`.


Pick Selection
~~~~~~~~~~~~~~

In order to select picks, the pick selection mode must be entered. Then dragging
a box (rubber band) around the picks in question will add them to the "cart".
The "cart" refers to the list of picks of the manual associated widget used to
attempt to locate an origin. Simply dragging a box will remove all previously
selected picks. Further options are:

* :kbd:`Shift + drag`: Add selected picks while keeping the previous selection.
* :kbd:`Ctrl + drag`: Remove selected picks while keeping the previous selection.

If at least one pick has been added to the cart, the manual associator will
open as a dock widget.

.. note::

   A dock widget is a special kind of window which can be docked to any border
   of the application or even displayed floated as kind of overlay window. The
   position of the dock widget will be persistent across application restarts.

At any change of the pick cart, the associator attempts a relocation and will
display the result in the details or an error message at the top.

To add more picks to the cart, shift has to be pressed while dragging the
selection box. To remove picks from the cart, ctrl has to be pressed while
dragging the selection box. Picks can also be removed individually from the
cart by clicking the close icon of each pick item.

Picks being part of the cart are also highlighted in the traces.


Location Setup
~~~~~~~~~~~~~~

The associator adds all available locators in the system and presents them
in a dropdown list at the bottom. The locator which should be selected as default
can be controlled with :confval:`associator.defaultLocator`. The profile which
is selected as default can be controlled with
:confval:`associator.defaultLocatorProfile`.

Whenever the operator changes any of the values, a new location attempt is being
made which can succeed or fail. A successful attempt will update the details,
a failed attempt will reset the details and print an error message at the top
of the window.

Each locator can be configured locally by clicking the wrench icon. This
configuration is not persistent across application restarts. It can be used
to tune and test various settings. Global locator configurations in the
configuration files are of course being considered by scrttv.

In addition to the locator and its profile a fixed depth can be set. By default
the depth is free and it is up to the locator implementation to assign a depth
to the origin. The depth dropdown list allows to set a predefined depth. The
list of depth values can be controlled with :confval:`associator.fixedDepths`.


Committing a solution
~~~~~~~~~~~~~~~~~~~~~

Once a solution is accepted by the operator it can be committed to the system
as regular origin as emitted by, e.g. `scautoloc`. Those origins will be sent to
the message group defined by :confval:`messaging.location` and grabbed by
connected modules, e.g., :ref:`scevent` and possibly associated to an
:term:`event`.

Alternatively, the button "Show Details" can be used to just send the origin to
the GUI group and let :ref:`scolv` or other GUIs pick it up and show it. This
will not store the origin in the database and works the same way as creating an
artificial origin.

.. _scrttv-hot-keys:

Hotkeys
=======

=======================  =======================================
Hotkey                   Description
=======================  =======================================
:kbd:`F1`                Open |scname| documentation
:kbd:`Shift+F1`          Open scrttv documentation
:kbd:`F2`                Setup connection dialog
:kbd:`F11`               Toggle fullscreen
:kbd:`ESC`               Set standard selection mode and deselect all traces
:kbd:`c`                 Clear picker  markers
:kbd:`b`                 Toggle record borders
:kbd:`h`                 List hidden streams
:kbd:`Ctrl+a`            Toggle showing arrivals
:kbd:`Ctrl+p`            Toggle showing picks
:kbd:`n`                 Restore default display
:kbd:`o`                 Align by origin time
:kbd:`p`                 Enable pick selection mode
:kbd:`Alt+left`          Reverse the data time window by buffer size
:kbd:`Alt+right`         Advance the data time window by buffer size
-----------------------  ---------------------------------------
**Filtering**
-----------------------  ---------------------------------------
:kbd:`f`                 Toggle filtering
:kbd:`d`                 Switch to previous filter in list if filtering is enabled.
:kbd:`g`                 Switch to next filter in list if filtering is enabled.
:kbd:`r`                 Toggle showing all records
-----------------------  ---------------------------------------
**Navigation**
-----------------------  ---------------------------------------
:kbd:`Ctrl+f`            Search traces
:kbd:`up`                Line up
:kbd:`down`              Line down
:kbd:`PgUp`              Page up
:kbd:`PgDn`              Page down
:kbd:`Alt+PgUp`          To top
:kbd:`Alt+PgDn`          To bottom
:kbd:`left`              Scroll left
:kbd:`right`             Scroll right
:kbd:`Ctrl+left`         Align left
:kbd:`Ctrl+right`        Align right
-----------------------  ---------------------------------------
**Navigation and data**
-----------------------  ---------------------------------------
:kbd:`Alt+left`          Rewind time window by 30' and load data
:kbd:`Alt+right`         Progress time window by 30' and load data
:kbd:`Ctrl+r`            (Re)load data in current visible time range
:kbd:`Ctrl+Shift+r`      Switch to real-time with configured buffer size
-----------------------  ---------------------------------------
**Sorting**
-----------------------  ---------------------------------------
:kbd:`1`                 Restore configuration order of traces
:kbd:`2`                 Sort traces by distance
:kbd:`3`                 Sort traces by station code
:kbd:`4`                 Sort traces by network-station code
:kbd:`5`                 Sort traces by group
-----------------------  ---------------------------------------
**Zooming**
-----------------------  ---------------------------------------
:kbd:`<`                 Horizontal zoom-in
:kbd:`>`                 Horizontal zoom-out
:kbd:`y`                 Vertical zoom-out
:kbd:`Shift+y`           Vertical zoom-in
:kbd:`s`                 Toggle amplitude normalization
:kbd:`Ctrl+mouse wheel`  Vertical and horizontal zooming
:kbd:`z`                 Enable/disable zooming: Drag window with left mouse button
=======================  =======================================
