*scorgls* lists all available origin IDs within a given time range to stdout.
Origins are fetched from database or read from a :term:`SCML` file.

Similarly, use :ref:`scevtls` for listing all event IDs. In extension to
*scorgls* and :ref:`scevtls` :ref:`scquery` can search for parameters based on
complex custom queries.


Examples
========

* Print all origin IDs for the complete year 2012.

  .. code-block:: sh

     scorgls -d mysql://sysop:sysop@localhost/seiscomp \
             --begin "2012-01-01 00:00:00" \
             --end "2013-01-01 00:00:00"

* Print the IDs of all origins provided with the XML file:

  .. code-block:: sh

     scevtls -i origins.xml
