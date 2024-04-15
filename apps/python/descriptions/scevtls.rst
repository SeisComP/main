*scevtls* lists the event IDs of all events available in a database or
:term:`SCML` file within a given time span. The list may be filtered by
event type. The IDs are printed to stdout.


Similarly, use :ref:`scorgls` for listing all origin IDs. In extension to
*scevtls* and :ref:`scorgls`, :ref:`scquery` can search for parameters based on
complex custom queries.


Examples
========

* Print all event IDs for the complete year 2012:

  .. code-block:: sh

     scevtls -d mysql://sysop:sysop@localhost/seiscomp \
             --begin "2012-01-01 00:00:00" \
             --end "2013-01-01 00:00:00"

* Print all event IDs with event type *quarry blast*:

  .. code-block:: sh

     scevtls -d mysql://sysop:sysop@localhost/seiscomp \
             --event-type "quarry blast"

* Print the IDs of all events provided with the XML file:

  .. code-block:: sh

     scevtls -i events.xml

* Print all event IDs along with the ID of the preferred origin:

  .. code-block:: sh

     scevtls -d localhost -p
