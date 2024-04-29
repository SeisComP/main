scqueryqc queries a database for waveform quality control (QC) parameters. The
QC parameters can be provided and written to the database, e.g., by :ref:`scqc`.

.. warning ::

   Writing QC parameters to the database by :ref:`scqc` will result in a rapidly
   growing database and is therefore not recommended in permanent application without
   regularly stripping these parameters from the  database!

The database query is done for

* One or multiple streams,
* One or multiple QC parameters. All QC parameters can be requested. Defaults
  apply. For reading the defaults use

  .. code-block:: sh

     scqueryqc -h

* A single time window where the begin time must be provided. Current time is
  considered if the end is not give.


Workflow
--------

You should minimize the impact of stored waveform QC parameters on the size of the
database.

#. Compute the QC parameters in real time using :ref:`scqc` and save them in the
   |scname| database. Saving the QC parameters in the database requires to
   adjust the scqc module configuration parameters
   :confval:`plugins.$name.archive.interval` for each plugin.
#. Regularly use scqueryqc for some time span to read the QC parameters from the
   database. Save them in a XML files.

   Example for all QC parameters found for all streams in the inventory before
   end time:

   .. code-block:: sh

      scqueryqc -d [host] -e '[end time]' --streams-from-inventory -o [XML file]


#. Clean the database from QC parameters.

   * Either use :ref:`scdispatch` with the parameters saved in XML. You may need
     to set the routing table for sending the QualityControl parameters to the
     right message group, e.g., QC:

     .. code-block:: sh

        scdispatch -H [host] -O remove --routingtable QualityControl:QC -i [XML file]

   * Alternatively, use :ref:`scdbstrip` with the command-line option
     :option:`--qc-only` and remove **all** QC parameters in the time span. Use the same
     period for which the QC parameters were retrieved:

     .. code-block:: sh

        scdbstrip -d [database] -Q --date-time '[end time]'

     .. note ::

        Considering an end time by :option:`--date-time` has the advantage that no QC
        parameters are removed which were measured after scqueryqc was applied with the
        same end time value.

Examples
--------

* Query rms and delay values for the stream AU.AS18..SHZ,AU.AS19..SHZ before
  '2021-11-20 00:00:00'. Write the XML to stdout

  .. code-block:: sh

     scqueryqc -d localhost -e '2021-11-20 00:00:00' -p rms,delay -i AU.AS18..SHZ,AU.AS19..SHZ

* Query all default QC parameter values for all streams found in the inventory
  from '2021-11-20 00:00:00' until current. Write the formatted XML output to
  :file:`/tmp/query.xml`

  .. code-block:: sh

     scqueryqc -d localhost -b '2021-11-20 00:00:00' --streams-from-inventory -f -o /tmp/query.xml
