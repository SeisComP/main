scquery_qc queries a database for waveform quality control (QC) parameters. The
QC parameters must be provided, e.g. by :ref:`scqc`.

.. warning ::

   Writing QC parameters to the database by :ref:`scqc` will result in a rapidly
   growing database and is therefore not recommended in permanent application!

The database query is done from

* One or multiple streams,
* One or multiple QC parameters. All QC parameters can be requested. Defaults
  apply. For reading the defaults use ::

     scquery_qc -h

* A single time window where the begin time must be provided. Current time is
  considered if the end is not give.


Workflow
--------

For minimizing the impact of stored waveform QC parameters on the size of the
database you may:

#. Compute the QC parameters in real time using scqc and save them in the
   |scname| database.
#. Regularly read the QC parameters from the database and clean up the database,
   e.g. in a cron job:

   #. use scquery_qc for some time span to read the QC parameters from the database.
      Save them in XML files.
   #. clean the database from the QC parameters saved in XML files using
      :ref:`scdispatch`, e.g. ::

         scdispatch -H [host] -O remove -i [XML file]


Examples
--------

* Query rms and delay values for the stream AU.AS19..SHZ' from
  '2021-11-20 00:00:00' until current. Write the XML to stdout ::

     scquery_qc -d localhost -b '2021-11-20 00:00:00' -p rms,delay -i AU.AS18..SHZ,AU.AS19..SHZ

* Query all default QC parameter values for streams 'AU.AS18..SHZ' and 'AU.AS19..SHZ'
  from '2021-11-20 00:00:00' until current. Write the formatted XML output to
  :file:`/tmp/query.xml` ::

     scquery_qc -d localhost -b '2021-11-20 00:00:00' -i AU.AS18..SHZ,AU.AS19..SHZ -f -o /tmp/query.xml
