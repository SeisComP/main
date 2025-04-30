scxmldump reads SeisComP objects from database or messaging and writes them
in SeisComP XML (:term:`SCML`) to stdout or into a file (:option:`-o`).

* Dumping from a SeisComP database supports various
  :ref:`objects <api-datamodel-python>`:

  * Availability,
  * Config (bindings parameters),
  * Event parameters,
  * Inventory,
  * Journal,
  * Routing.

* Dumping from a SeisComP messaging (:option:`--listen`) considers only event
  parameters.

.. note::

   Waveform quality control (QC) parameters can be read from databases using
   :ref:`scqcquery`.

* Furthermore any object carrying a publicID can be retrieved from the database
  including its hierarchie or not. See :ref:`scxmldump-public-objects`.


Event parameters
----------------

To get event, origin or pick information from the database without using SQL
commands is an important task for the user. :ref:`scxmldump` queries the
database and transforms that information into XML. Events and origins can be
treated further by :ref:`scbulletin` for generating bulletins or conversion
into other formats including KML.

Many processing modules, e.g., :ref:`scevent` support the on-demand processing
of dumped event parameters by the command-line option :option:`--ep`.
Importing event parameters into another database is possible with :ref:`scdb`
and sending to a SeisComP messaging is provided by :ref:`scdispatch`.

.. hint::

   Events, origins and picks are referred to by their public IDs. IDs of events
   and origins can be provided by :ref:`scevtls` and :ref:`scorgls`,
   respectively. Event, origin and pick IDs can also be read from graphical
   tools like :ref:`scolv` or used database queries assisted by :ref:`scquery`.


.. _scxmldump-public-objects:
PublicObjects
-------------

The option :option:`--public-id` defines a list of publicIDs to be retrieved
from the database. As the data model is extendable via plugins and custom code,
scxmldump cannot know all of those object types and how to retrieve them
from the database. If a publicID belongs to a type for which the code resides
in another library or plugin, then scxmldump must load this plugin or library
in order to find the correct database tables. For example, if a strong motion
object should be dumped, then the plugin dmsm must be loaded into scxmldump.

.. code-block:: sh

   scxmldump -d localhost --plugins dbmysql,dmsm --public-id StrongMotionOrigin/123456

This command would only export the StrongMotionOrigin itself without all
child objects. Option :option:`--with-childs` must be passed to export the
full hierarchy:

.. code-block:: sh

   scxmldump -d localhost --plugins dbmysql,dmsm --public-id StrongMotionOrigin/123456 --with-childs


If the extension code resides in a library then LD_PRELOAD can be used to inject
the code into scxmldump:

.. code-block:: sh

   LD_PRELOAD=/home/sysop/seiscomp/lib/libseiscomp_datamodel_sm.so scxmldump -d localhost --public-id StrongMotionOrigin/123456 --with-childs


Format conversion
-----------------

Conversion of :term:`SCML` into other formats is supported by :ref:`sccnv`.
An XSD schema of the XML output can be found under
:file:`$SEISCOMP_ROOT/share/xml/`.


Examples
--------

* Dump inventory

  .. code-block:: sh

     scxmldump -d mysql://sysop:sysop@localhost/seiscomp -fI -o inventory.xml

* Dump config (bindings parameters)

  .. code-block:: sh

     scxmldump -d localhost -fC -o config.xml

* Dump full event data including the relevant journal entries

  .. code-block:: sh

     scxmldump -d localhost -fPAMFJ -E test2012abcd -o test2012abcd.xml

* Dump full event data. Event IDs are provided by :ref:`scevtls` and received
  from stdin

  .. code-block:: sh

     scevtls -d localhost --begin 2025-01-01 |\
     scxmldump -d localhost -fPAMF -E - -o events.xml

* Dump summary event data

  .. code-block:: sh

     scxmldump -d localhost -fap -E test2012abcd -o test2012abcd.xml

* Create bulletin from an event using :ref:`scbulletin`

  .. code-block:: sh

     scxmldump -d localhost -fPAMF -E test2012abcd | scbulletin

* Copy event parameters to another database

  .. code-block:: sh

     scxmldump -d localhost -fPAMF -E test2012abcd |\
     scdb -i - -d mysql://sysop:sysop@archive-db/seiscomp

* Dump the entire journal:

  .. code-block:: sh

     scxmldump -d localhost -fJ -o journal.xml

* Dump events received from messaging on local computer:

  .. code-block:: sh

     scxmldump -H localhost/production --listen
