scdispatch reads an :term:`SCML` file and creates notifier objects for them that
are sent to the :ref:`messaging <concepts_messaging>` and the corresponding
messaging groups (see :option:`--routingtable`). In contrast to :ref:`scdb`
which writes SCML files directly into the :ref:`database <concepts_database>`
scdispatch uses the messaging bus. If :ref:`scmaster` is configured with the
database plugin, messages will end up in the database as well.


Modes
-----

scdispatch can work in two modes applying different
:ref:`operations <scdispatch-operations>`:

* *Without database check:* One of the :ref:`operations <scdispatch-operations>`
  *add*, *update* or *remove* is selected along with the option :option:`-O`. In
  that case all objects in the :term:`SCML` are encapsulated in a notifier with
  that specific operation and sent to the messaging. No check is performed if
  the object is already in the database or not.
* *With database check:* The option :option:`-O` is not given or the
  option is used along with one of the :ref:`operations <scdispatch-operations>`
  *merge* or *merge-without-remove*. scdispatch first tries to load the corresponding
  objects from the database and calculates differences. It will then create the
  corresponding notifiers with operations *add*, *update* or *remove* and sends
  them to the messaging. That mode is quite close to a sync operation with the
  exception that top level objects (such as origin or event) that are not part
  of the input SCML are left untouched in the database. It can be used to
  synchronize event information from one system with another.


.. _scdispatch-operations:

Operations
----------

Different operations can be chosen along with the option :option:`-O`.
If :option:`-O` is not given, *merge* is assumed by default.

* *Add*: All objects are sent trying to be added to the database. If they
  already exist in the database, they will be rejected and not spread through
  the messaging. Modules connected to the messaging will not receive rejected
  objects.
* *Remove*: All sent objects with all their attributes and child objects are
  removed from the database. Modules connected to the messaging will not receive
  any sent object.
* *Update*: All objects are sent trying to be updated to the database along with
  all of their child objects and attributes. Sent objects not existing in the
  database will be ignored and not received by any module connected to the
  messaging. Child objects and attributes existing in the database but not
  included in the sent object will be removed as well.
* *Merge* (default): Applies *Add* and *Update* and requires a database
  connection.
* *Merge-without-remove*: Applies *Add* and *Update* and requires a database
  connection. However, no objects are removed from the database.

.. note::

   All |scname| objects along are listed and described along with their child
   objects and attributes in the :ref:`API documentation <api-datamodel-python>`.


Examples
--------

#. Send different objects from a :term:`SCML` file for merging (adding or
   updating). The option :option:`-O` can be ommitted because the default
   behavior is to merge:

   .. code-block:: sh

      scdispatch -i test.xml -O merge
      scdispatch -i test.xml

#. Send all objects by ignoring events. When :ref:`scevent` receives origins it
   will create new events or associate the origins to existing ones. The ignored
   events may be already existing with different IDs. Hence, event duplication
   is avoided by ignoring them.

   .. code-block:: sh

      scdispatch -i test.xml -e

#. Send new objects to be added:

   .. code-block:: sh

      scdispatch -i test.xml -O add

#. Send an update of objects:

   .. code-block:: sh

      scdispatch -i test.xml -O update

#. Send objects to be removed:

   .. code-block:: sh

      scdispatch -i test.xml -O remove

#. Compare new objects with the database content and send the difference (optionally without removing objects):

   .. code-block:: sh

      scdispatch -i test.xml -O merge
      scdispatch -i test.xml -O merge-without-remove

#. Offline mode: all operations can be performed without the messaging system using xml files:

   .. code-block:: sh

      scdispatch -i test.xml -O operation --create-notifier > notifier.xml

   then:

   .. code-block:: sh

      scdb -i notifier.xml

#. Subsets of SCML Objects

   It can be useful to import a subset of QuakeML objects, e.g. Origins from other
   agencies and then allow :ref:`scevent` to associate them to existing
   events (and possibly prefer them based on the rules in scevent) or create new
   events for the origins. If the event objects from a SCML file are not required
   to be sent to the messaging then either they should be removed (e.g. using XSLT)
   and all the remaining objects in the file added:

   .. code-block:: sh

      scdispatch -i test.xml -O add

   or the **event objects** can be left out of the routing table, e.g.

   .. code-block:: sh

      scdispatch -i test.xml -O add \
                 --routingtable Pick:PICK, \
                                Amplitude:AMPLITUDE, \
                                Origin:LOCATION,StationMagnitude:MAGNITUDE, \
                                Magnitude:MAGNITUDE

   .. hint::

      The option :option:`--no-event` is a wrapper for removing Event:EVENT from
      the routing table. With this option no event objects will be sent which may
      be useful if just the origins with magnitudes, amplitudes, arrivals, picks, etc.
      shall be integrated, e.g. after XML-based playbacks.


#. Testing

   For testing it is useful to watch the results of dispatch with :ref:`scolv` or
   :ref:`scxmldump`. It is also useful to clean the database and logs to remove
   objects from persistent storage to allow repeated reloading of a file.

   .. note::

      The following will clear all events from the database and any other
      other object persistence. Modify the mysql command to suit your db setup.

      .. code-block:: sh

         mysql -u root --password='my$q1' -e "DROP DATABASE IF EXISTS seiscomp; \
           CREATE DATABASE seiscomp CHARACTER SET utf8 COLLATE utf8_bin; \
           GRANT ALL ON seiscomp.* TO 'sysop'@'localhost' IDENTIFIED BY 'sysop'; \
           USE seiscomp;source seiscomp/trunk/share/db/mysql.sql;"

         seiscomp start
