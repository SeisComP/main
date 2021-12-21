:program:`scheli` visualizes waveforms from a single stream or multiple stations
mimicking a drum-recorder plot (see :ref:`fig-scheli`):

* :program:`scheli` plots one configurable trace in helicorder style in the
  :term:`GUI` (:ref:`GUI mode <scheli-show>`).
* Configurable GUI: trace colors, visualized time spans, number of rows, data filtering,
  amplitude ranges and much more.
* Automatic image capturing: Capture helicorder images at configurable time intervals
  of one trace in :ref:`GUI mode<scheli-show>` or a set of multiple channels in
  :ref:`capture mode<scheli-capture>`.
  The images can be used, e.g. for showing data images on web sites.

.. _fig-scheli:

.. figure:: media/scheli.png
   :width: 16cm
   :align: center

   scheli in GUI mode


Examples
========

.. _scheli-show:

1. **GUI mode - Simple helicorder window:**

   * Learn about the plenty command-line options for :program:`scheli`: ::

        scheli -h

   * Start :program:`scheli` with the configured values and informative debug output: ::

        scheli --debug

   * Let :program:`scheli` show data from the CX station PB01 for the previous 5 hours
     overriding configuration by command-line paramaters:

     .. code-block:: sh

        scheli --stream CX.PB01..HHZ --rows 10

   * Define the data request window by end time and duration; scale traces to the
     maximum amplitude per row: ::

        scheli --stream IU.TSUM.00.BHZ --end-time "2021-04-22 14:00:00" --time-span 600 --amp-scaling row

.. _scheli-capture:

2. **Capture mode - Image capturing:**

   Capture the helicorder plot for 3 stations in intervals of 10 seconds.
   The data is retrieved using seedlink and the plots are stored as PNG images.
   The image files are named according to network, station, stream and location codes
   of the requested stations. Command-line parameters override the module configuration.

   .. code-block:: sh

      scheli capture --stream CX.PB01..HHZ --stream CX.PB02..HHZ --stream CX.PB04..HHZ --interval 10 -o "/tmp/heli_%N_%S_%L_%C.png" -H localhost -I slink://localhost

   The output file names will be generated based on network code (%N), station code (%S),
   location code (%L) and stream code (%C): ::

      /tmp/CX.PB01..HHZ.png
      /tmp/CX.PB02..HHZ.png
      /tmp/CX.PB04..HHZ.png

Setup
=====

Specifc :program:`scheli` parameters are adjusted in the :ref:`module configuration <scheli_configuration>`.
Colors of traces etc. can be adjusted by setting the *scheme* parameters in
the global configuration of scheli. For alternating colors between the traces
set the parameters scheme.colors.records.foreground and
scheme.colors.records.alternateForeground in :file:`scheli.cfg`:

.. code-block:: sh

   # The general color of records/traces.
   scheme.colors.records.foreground = 4286F4

   # A general trace color of the alternate trace (eg scheli).
   scheme.colors.records.alternateForeground = B72D0E
