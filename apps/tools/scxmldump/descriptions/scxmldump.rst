scxmldump reads various parameters from a SeisComP database:

* Availability,
* Config (bindings parameters),
* Event parameters,
* Inventory,
* Journal,
* Routing.

The parameters are sent to stdout or written into an XML (:term:`SCML`) file.

.. note::

   Waveform quality control (QC) parameters can be read from databases using
   :ref:`scqcquery`.


Event parameters
----------------

To get event, origin or pick information from the database without using SQL
commands is an important task for the user. :ref:`scxmldump` queries the
database and transforms that information into XML. Events and origins can be
treated further by :ref:`scbulletin` for generating bulletins or conversion
into KML.

Many processing modules, e.g., :ref:`scevent` support the on-demand processing
of dumped event parameters by the command-line option :option:`--ep`.
Importing event parameters into another database is possible with :ref:`scdb`
and sending to a SeisComP messaging is provided by :ref:`scdispatch`.

.. hint::

   Events, origins and picks are referred to by their public IDs. IDs of events
   and origins can be provided by :ref:`scevtls` and :ref:`scorgls`,
   respectively. Event, origin and pick IDs can also be read from graphical
   tools like :ref:`scolv` or used database queries assisted by :ref:`scquery`.


Format conversion
-----------------

Conversion of :term:`SCML` into other formats is supported by :ref:`sccnv`.
An XSD schema of the XML output can be found under
:file:`$SEISCOMP_ROOT/share/xml/`.


Examples
--------

Dump inventory

.. code-block:: sh

   scxmldump -fI -o inventory.xml -d mysql://sysop:sysop@localhost/seiscomp

Dump config (bindings parameters)

.. code-block:: sh

   scxmldump -fC -o config.xml -d mysql://sysop:sysop@localhost/seiscomp

Dump full event data incl. the relevant journal entries

.. code-block:: sh

   scxmldump -fPAMFJ -E test2012abcd -o test2012abcd.xml \
             -d mysql://sysop:sysop@localhost/seiscomp


Dump summary event data

.. code-block:: sh

   scxmldump -fap -E test2012abcd -o test2012abcd.xml \
             -d mysql://sysop:sysop@localhost/seiscomp


Create bulletin from an event

.. code-block:: sh

   scxmldump -fPAMF -E test2012abcd
             -d mysql://sysop:sysop@localhost/seiscomp | \
   scbulletin


Copy event parameters to another database

.. code-block:: sh

   scxmldump -fPAMF -E test2012abcd \
             -d mysql://sysop:sysop@localhost/seiscomp | \
   scdb -i - -d mysql://sysop:sysop@archive-db/seiscomp


Export the entire journal:

.. code-block:: sh

   scxmldump -fJ -o journal.xml \
             -d mysql://sysop:sysop@localhost/seiscomp
