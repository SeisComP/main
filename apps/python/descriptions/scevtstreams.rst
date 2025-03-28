scevtstreams reads all picks of an event or solitary picks determining the time
window between the first pick and the last pick.
In addition symmetric asymmetric time margins are added to this time window.
It writes the streams that are picked including the determined
time window for the event to stdout. This tool gives appropriate input
information for :ref:`scart`, :ref:`fdsnws` and :cite:t:`capstool` for
:cite:t:`caps` server (Common Acquisition Protocol Server by gempa GmbH) to dump
waveforms from archives based on event data.

Events with origins and picks can be read from database or XML file. Solitary
picks can only be read from XML file. The XML files can be generated using
:ref:`scxmldump`.


Output Format
=============

The generated list contains start and end time as well as stream information.

Generic:

.. code-block:: properties

   starttime;endtime;stream

Example:

.. code-block:: properties

   2019-07-17 02:00:00;2019-07-17 02:10:00;GR.CLL..BHZ


Examples
========

#. Get the time windows for an event in the database:

   .. code-block:: sh

      scevtstreams -E gfz2012abcd -d mysql://sysop:sysop@localhost/seiscomp

#. Get the time windows for one specific event or all events in a XML file.
   The time windows start 120 s before the first pick and ends 500 s after the
   last pick:

   .. code-block:: sh

      scevtstreams -i event.xml -E gfz2012abcd
      scevtstreams -i event.xml

#. Get the time windows from all picks in a XML file which does not contain
   events or origins:

   .. code-block:: sh

      scevtstreams -i picks.xml

#. Combine with :ref:`scart` for creating a :term:`miniSEED` data file from one
   event. The time window starts and ends 5 minutes before the first and after
   the last pick, respectively.
   The data is read from :term:`SDS` archive and sorted by end time:

   .. code-block:: sh

      scevtstreams -E gfz2012abcd -d mysql://sysop:sysop@localhost/seiscomp -m 300 |\
      scart -dsvE --list - ~/seiscomp/acquisition/archive > gfz2012abcd-sorted.mseed

#. Download waveforms from FDSN and import into local archive. Include
   all stations from the contributing networks:

   .. code-block:: sh

      scevtstreams -E gfz2012abcd -d mysql://sysop:sysop@localhost/seiscomp -m 300 -R --all-stations |\
      scart --list - -I fdsnws://geofon.gfz.de ./my-archive

#. Create lists compatible with :ref:`fdsnws` POST format or :cite:t:`capstool`:

   .. code-block:: sh

      scevtstreams -E gfz2012abcd -i event.xml -m 120,500 --fdsnws
      scevtstreams -E gfz2012abcd -i event.xml -m 120,500 --caps
