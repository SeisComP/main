scmssort reads unsorted (and possibly multiplexed) MiniSEED files and sorts
the individual records by time. This is useful e.g. for simulating data
acquisition and playbacks. Removing of duplicate data and trimming of time window is available.

scmssort reads single files and output to the command line. Cat many files
to read them at the same time. In this way huge amount of data can be processed efficiently.


.. hint::

   Combine with :ref:`scart` or :ref:`msrtsimul` to archive data or to make playbacks
   with real-time simulations.

Examples
========

#. Read a single file. The records are sorted by endtime. Duplicated records are
   removed.

   .. code-block:: sh

      scmssort -u -E -v unsorted.mseed > sorted.mseed

#. Read all files ending with ".mseed" at the same time. The data are trimmed to a time window and duplicated
   data are removed

   .. code-block:: sh

      cat *.mseed | scmssort -u -E -v -t '2020-03-28 15:48~2020-03-28 16:18' > sorted.mseed
