scbulletin transforms the parameters of an event or an origin either to autoloc1
or to autoloc3 format.


Input Modes
-----------

Two modes of parameter input are possible:

1. Either one can fetch all necessary information from database directly
#. or one can provide a representation of the origin as XML file, to transform it
   to a standard format.

The first mode is the dump-mode the second is the input-mode. For dumping either
choose eventID or the originID. If the eventID is choosen the preferred origin
will be used.


Output Modes
------------

Different output formats are available by command-line options:

#. ``-1`` for **autoloc1**: Print one bulletin per event.
#. ``-3`` for **autoloc3**: Print one bulletin per event.
#. ``--fdsnws`` for FDSNWS event text: Print one line per event. Useful for
   generating event catalogs from XML file containing several events. This
   option offers an alternative to generating event catalogs by
   :ref:`fdsnws-event <sec-event>`.
#. ``-3 -x`` for **extended autoloc3**.
#. ``-1 -e`` or ``-3 -e`` for **enhanced** output at high-precision. All times
   and distances are in units of milliseconds and meters, respectively.

If called with an event or origin ID a database connection is necessary to
fetch the corresponding object. Otherwise scbulletin will read the input source
(defaults to stdin), grab the first found event or origin and dump it.


Examples
========

#. Create bulletin from one event in database

   .. code-block:: sh

      scbulletin -d mysql://sysop:sysop@localhost/seiscomp -E gfz2012abcd

#. Convert XML file to bulletin

   .. code-block:: sh

      scbulletin < gfz2012abcd.xml

   .. code-block:: sh

      scbulletin -i gfz2012abcd.xml


#. Convert XML file to bulletin but filter by event ID

   .. code-block:: sh

      scbulletin -i gfz2012abcd.xml -E gempa2022abcd

.. note::

   When considering a single event XML file containing many events, the
   bulletins of all events will be generated unless ``--first-only`` is used.
