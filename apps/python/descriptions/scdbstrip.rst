|scname|'s :ref:`scmaster` is continuously writing to the database. This causes
the database to grow and to occupy much space on the harddisc. scdbstrip taggles
this problem and removes processed objects from the database older than a
configurable time span. The time comparison considers the object time, not the
time of their creation.

The parameters which scdbstrip removes are

* Event parameters including events, origins, magnitudes, amplitudes, arrivals, picks,
  focal mechanisms, moment tensors
* Waveform quality control (QC) parameters.

scdbstrip will remove all events with an origin time and QC parameters older or
younger than specified. Default is 'older'. It will also remove all associated
objects such as picks, origins, arrivals, amplitudes and so on.

scdbstrip does not run as a daemon. To remove old objects continuously scdbstrip
should be added to the list of cronjobs running every e.g. 30 minutes. The more
often it runs the less objects it has to remove and the faster it will unlock
the database again. The timing and the parameters to be removed is controlled
by module configuration or command-line options.

.. hint::

   * For removing specific parameters and not all in a time range, use
     :ref:`scdispatch` along with XML files created by :ref:`scxmldump` and
     :ref:`scqueryqc` for event parameters and waveform QC parameters,
     respectively.
   * For removing data availability parameters use :ref:`scardac`.


Known Issues
============

When running scdbstrip for the first time on a large database it can happen
that it aborts in case of MYSQL with the following error message:


.. code-block:: sh

   [  3%] Delete origin references of old events...08:48:22 [error]
   execute("delete Object from Object, OriginReference, old_events where
   Object._oid=OriginReference._oid and
   OriginReference._parent_oid=old_events._oid") = 1206 (The total number
   of locks exceeds the lock table size)

   Exception: ERROR: command 'delete Object from Object, OriginReference,
   old_events where Object._oid=OriginReference._oid and
   OriginReference._parent_oid=old_events._oid' failed

That means your MYSQL server cannot hold enough data required for deletion.
There are two solutions to this:

#. Increase the memory pool used by MYSQL by changing the configuration. The
   minimum is 64 MBytes but modern system typically have a larger default:

   .. code-block:: sh

      innodb_buffer_pool_size = 64M

   The size of the new buffer depends on the size of the database that should
   be cleaned up. Read also the section :ref:`database_configuration`. It
   provides more options for optimizing your database server.

#. Run scdbstrip on smaller batches for the first time:

   .. code-block:: sh

      $ scdbstrip -d mysql://sysop:sysop@localhost/seiscomp --days 1000
      $ scdbstrip -d mysql://sysop:sysop@localhost/seiscomp --days 900
      ...
      $ scdbstrip -d mysql://sysop:sysop@localhost/seiscomp --days 100

.. hint::

   In the examples, database connection parameters correspond to default values.
   You may thus replace ``-d mysql://sysop:sysop@localhost/seiscomp`` by
   ``-d localhost`` or ``-d mysql://``.


Examples
========

* Remove event and waveform quality parameters older than 30 days

  .. code-block:: sh

     scdbstrip -d mysql://sysop:sysop@localhost/seiscomp --days 30

* Remove event and waveform quality parameters newer than 30 days

  .. code-block:: sh

     scdbstrip -d mysql://sysop:sysop@localhost/seiscomp --days 30 -i

* Only remove waveform QC parameters older than 30 days but no others

  .. code-block:: sh

     scdbstrip -d mysql://sysop:sysop@localhost/seiscomp --days 30 --qc-only

* Remove event and waveform quality parameters before 2000-01-01 12:00:00

  .. code-block:: sh

     scdbstrip -d mysql://sysop:sysop@localhost/seiscomp --datetime 2000-01-01T12:00:00

* Remove event and waveform quality parameters after 2000-01-01 12:00:00

  .. code-block:: sh

     scdbstrip -d mysql://sysop:sysop@localhost/seiscomp --datetime 2000-01-01T12:00:00 -i

* Remove event and waveform quality parameters between 2000-01-01 12:00:00 ~ 2000-01-01 14:00:00

  .. code-block:: sh

     scdbstrip -d mysql://sysop:sysop@localhost/seiscomp --time-window 2000-01-01T12:00:00~2000-01-01T14:00:00

* Remove event and waveform quality parameters before 2000-01-01 12:00:00 and after 2000-01-01 14:00:00

  .. code-block:: sh

     scdbstrip -d mysql://sysop:sysop@localhost/seiscomp --time-window 2000-01-01T12:00:00~2000-01-01T14:00:00 -i


