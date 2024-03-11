scmssort reads unsorted (and possibly multiplexed) MiniSEED files and sorts
the individual records by time. This is useful e.g. for simulating data
acquisition and playbacks. Removing of duplicate data and trimming of time
window is available.

scmssort reads single files and output to the command line. Cat many files
to read them at the same time. In this way huge amount of data can be processed
efficiently.

Applications to miniSEED records:

* Sort records by time, e.g., for playbacks.
* Remove duplicate records from files and clean waveform archives.
* Filter data records, i.e. keep or remove them, based on

  * time windows,
  * stream lists where each line has the format NET.STA.LOC.CHA including regular
    expressions. Such stream lists can be generated, e.g., using :ref:`scinv`.

.. hint::

   * Combine with :ref:`scart` or :ref:`msrtsimul` to archive data or to make
     playbacks with real-time simulations.
   * Filter data by stream IDs using NSLC lists which can be generated using
     :ref:`scinv`.


Examples
========

#. Read a single miniSEED data file. The records are sorted by endtime and
   duplicates are removed.

   .. code-block:: sh

      scmssort -vuE unsorted.mseed > sorted.mseed

#. Read all files ending with ".mseed" at the same time. The data are trimmed
   to a time window and duplicated or empty records are ignored.

   .. code-block:: sh

      cat *.mseed | scmssort -vuiE -t 2020-03-28T15:48~2020-03-28T16:18 > sorted.mseed

#. Remove streams listed by stream code and sort records by end time. Also ignore
   duplicated or empty records. Stream lists can be generated, e.g., by :ref:`scinv`.

   .. code-block:: sh

      scmssort -vuiE --rm -l stream-list.txt test.mseed > sorted.mseed

#. Extract streams by time and stream code and sort records by end time. Also ignore
   duplicated or empty records.

   .. code-block:: sh

      echo CX.PB01..BH? | scmssort -vuE -t 2007-03-28T15:48~2007-03-28T16:18 -l - test.mseed > sorted.mseed
      scmssort -vuiE -t 2007-03-28T15:48~2007-03-28T16:18 -l stream-list.txt test.mseed > sorted.mseed
