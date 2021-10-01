*scorgls* lists all available origin IDs within a given time range to stdout.

Use :ref:`scevtls` for listing all event IDs. In extension to :ref:`scevts` and scorgls
:ref:`scquery` can search for parameters based on complex custom queries.


Example
=======

Print all origin IDs for the complete year 2012.

.. code-block:: sh

   scorgls -d mysql://sysop:sysop@localhost/seiscomp \
           --begin "2012-01-01 00:00:00" \
           --end "2013-01-01 00:00:00"
