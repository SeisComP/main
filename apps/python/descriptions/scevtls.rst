*scevtls* lists all available event IDs within a given time range to stdout.

Use :ref:`scorgls` for listing all origin IDs. In extension to scevtls and :ref:`scorgls`
:ref:`scquery` can search for parameters based on complex custom queries.


Example
=======

Print all event IDs for the complete year 2012.

.. code-block:: sh

   scevtls -d mysql://sysop:sysop@localhost/seiscomp \
           --begin "2012-01-01 00:00:00" \
           --end "2013-01-01 00:00:00"
