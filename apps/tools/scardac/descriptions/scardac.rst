scardac scans an :term:`SDS waveform archive <SDS>`, e.g.,
created by :ref:`slarchive` or :ref:`scart` for available
:term:`miniSEED <miniSeed>` data. It will collect information about

* ``DataExtents`` -- the earliest and latest times data is available
  for a particular channel,
* ``DataAttributeExtents`` -- the earliest and latest times data is available
  for a particular channel, quality and sampling rate combination,
* ``DataSegments`` -- continuous data segments sharing the same quality and
  sampling rate attributes.

scardac is intended to be executed periodically, e.g., as a cronjob.

The availability data information is stored in the SeisComP database under the
root element :ref:`DataAvailability <api-datamodel-python>`. Access to the
availability data is provided by the :ref:`fdsnws` module via the services:

* :ref:`/fdsnws/station <sec-station>` (extent information only, see
  ``matchtimeseries`` and ``includeavailability`` request parameters).
* :ref:`/fdsnws/ext/availability <sec-avail>` (extent and segment information
  provided in different formats)


.. _scarcac_non-sds:

Non-SDS archives
----------------

scardac can be extended by plugins to scan non-SDS archives. For example the
``daccaps`` plugin provided by :cite:t:`caps` allows scanning archives generated
by a CAPS server. Plugins are added to the global module configuration, e.g.:

.. code-block:: properties

   plugins = ${plugins}, daccaps


.. _scarcac_workflow:

Definitions
-----------

* ``Record`` -- continuous waveform data of same sampling rate and quality bound
  by a start and end time. scardac will only read the record's meta data and not
  the actual samples.
* ``Chunk`` -- container for records, e.g., a :term:`miniSEED <miniSeed>` file,
  with the following properties:

  - overall, theoretical time range of records it may contain
  - contains at least one record, otherwise it must be absent
  - each record of a chunk must fulfill the following conditions:

    - `chunk start <= record start < chunk end`
    - `chunk start < record end < next chunk end`
  - chunks do not overlap, end time of current chunk equals start time of
    successive chunk, otherwise a ``chunk gap`` is declared
  - records may occur unordered within a chunk or across chunk boundaries,
    resulting in `DataSegments` marked as ``outOfOrder``
* ``Jitter`` -- maximum allowed deviation between the end time of the current
  record and the start time of the next record in multiples of the current's
  record sampling rate. E.g., assuming a sampling rate of 100Hz and a jitter
  of 0.5 will allow for a maximum end to start time difference of 50ms. If
  exceeded a new `DataSegment` is created.
* ``Mtime`` -- time the content of a chunk was last modified. It is used to

  - decided whether a chunk needs to be read in a secondary application run
  - calculate the ``updated`` time stamp of a `DataSegment`,
    `DataAttributeExtent` and `DataExtent`
* ``Scan window`` -- time window limiting the synchronization of the archive
  with the database configured via :confval:`filter.time.start` and
  :confval:`filter.time.end` respectively :option:`--start` and :option:`--end`.
  The scan window is useful to

  - reduce the scan time of larger archives. Depending on the size and storage
    type of the archive it may take some time to just list available chunks and
    their mtime.
  - prevent deletion of availability information even though parts of the
    archive have been deleted or moved to a different location
* ``Modification window`` -- the mtime of a chunk is compared with this time
  window to decide whether it needs to be read or not. It is configured via
  :confval:`mtime.start` and :confval:`mtime.end` repectively
  :option:`--modified-since` and :option:`--modified-until`. If no lower bound
  is defined then the ``lastScan`` time stored in the `DataExtent` is used
  instead.  The mtime check may be disabled using :confval:`mtime.ignore` or
  :option:`--deep-scan`.
  **Note:** Chunks in front or right after a chunk gap are read in any case
  regardless of the mtime settings.

Workflow
--------

#. Read existing `DataExtents` from database.
#. Collect a list of available stream IDs either by

   * scanning the archive for available IDs or
   * reading an ID file defined by :confval:`nslcFile`.
#. Identify extents to add, update or remove respecting `scan window`,
   :confval:`filter.nslc.include` and :confval:`filter.nslc.exclude`.
#. Subsequently process the `DataExtents` using :confval:`threads` number of
   parallel threads. For each `DataExtent`:

   #. Collect all available chunks within `scan window`.
   #. If the `DataExtent` is new (no database entry yet), store a new and
      empty `DataExtent` to database, else query existing `DataSegments` from
      the database:

      * count segments outside `scan window`
      * create a database iterator for extents within `scan window`
   #. Create two in-memory segment lists which collect segments to remove and
      segments to add/update
   #. For each chunk

      * determine the `chunk window` and `mtime`
      * decide whether the chunk needs to be read depending on the `mtime`
        and a possible `chunk gap`. If necessary, read the chunk and

        - create chunk segments by analyzing the chunk records for
          gaps/overlaps defined by :confval:`jitter`, sampling rate or quality
          changes
        - merge chunk segments with database segments and update the in-memory
          segment lists.

        If not necessary, advance the database segment iterator to the end
        of the chunk window.

   #. Remove and then add/update the collected segments.
   #. Merge segment information into `DataAttributeExtents`
   #. Merge `DataAttributeExtents` into overall `DataExtent`

Examples
--------

#. Get command line help or execute scardac with default parameters and informative
   debug output:

   .. code-block:: sh

      scardac -h
      scardac --debug

#. Synchronize the availability of waveform data files existing in the standard
   :term:`SDS` archive with the seiscomp database and create an XML file using
   :ref:`scxmldump`:

   .. code-block:: sh

      scardac -d mysql://sysop:sysop@localhost/seiscomp -a $SEISCOMP_ROOT/var/lib/archive --debug
      scxmldump -Yf -d mysql://sysop:sysop@localhost/seiscomp -o availability.xml

#. Synchronize the availability of waveform data files existing in the standard
   :term:`SDS` archive with the seiscomp database. Use :ref:`fdsnws` to fetch a flat file containing a list
   of periods of available data from stations of the CX network sharing the same
   quality and sampling rate attributes:

   .. code-block:: sh

      scardac -d mysql://sysop:sysop@localhost/seiscomp -a $SEISCOMP_ROOT/var/lib/archive
      wget -O availability.txt 'http://localhost:8080/fdsnws/ext/availability/1/query?network=CX'

   .. note::

      The |scname| module :ref:`fdsnws` must be running for executing this
      example.
