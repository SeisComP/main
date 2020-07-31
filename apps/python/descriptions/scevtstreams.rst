scevtstreams reads all picks of an event and determines the time window between
the first pick and the last pick. In addition a symmetric or an asymmetric time
margin is added to this
time window. It writes the streams that are picked including the determined
time window for the event to stdout. This tool gives appropriate input
information for :ref:`scart`, :ref:`fdsnws` and
`caps <https://docs.gempa.de/caps/current/apps/capstool.html>`_
(Common Acquisition Protocol Server by gempa GmbH) to dump waveforms from archives
based on event data.

Output Format
=============

The generated list contains start and end time as well as stream information.

Generic: ::

   starttime;endtime;streamID

Example: ::

   2019-07-17 02:00:00;2019-07-17 02:10:00;GR.CLL..BHZ


Examples
========

#. Get the time windows for an event in the database:

   .. code-block:: sh

      scevtstreams -E gfz2012abcd -d mysql://sysop:sysop@localhost/seiscomp

#. Get the asymmetric time windows for an event in an XML file. The time window
   starts 120 s before the first pick and ends 500 s after the last pick:

   .. code-block:: sh

      scevtstreams -E gfz2012abcd -i event.xml -m 120,500

#. Create a playback of an event with a time window of 5 minutes data and sort the records by end time:

   .. code-block:: sh

      scevtstreams -E gfz2012abcd -d mysql://sysop:sysop@localhost/seiscomp -m 300 |\
      scart -dsvE --list - ~/seiscomp/acquisition/archive > gfz2012abcd-sorted.mseed

#. Download waveforms from Arclink and import into local archive. Include all stations from the contributing networks:

   .. code-block:: sh

      scevtstreams -E gfz2012abcd -d mysql://sysop:sysop@localhost/seiscomp -m 300 -R --all-stations |\
      scart --list - ./my-archive

#. Create lists compatible with :ref:`fdsnws` or `caps <https://docs.gempa.de/caps/current/apps/capstool.html>`_: ::

      scevtstreams -E gfz2012abcd -i event.xml -m 120,500 --fdsnws
      scevtstreams -E gfz2012abcd -i event.xml -m 120,500 --caps
