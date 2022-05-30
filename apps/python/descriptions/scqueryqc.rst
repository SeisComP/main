scqueryqc queries a database for waveform quality control (QC) parameters. The
QC parameters must be provided, e.g. by :ref:`scqc`.

.. warning ::

   Writing QC parameters to the database by :ref:`scqc` will result in a rapidly
   growing database and is therefore not recommended in permanent application!

The database query is done from

* One or multiple streams,
* One or multiple QC parameters. All QC parameters can be requested. Defaults
  apply. For reading the defaults use ::

     scqueryqc -h

* A single time window where the begin time must be provided. Current time is
  considered if the end is not give.


Workflow
--------

For minimizing the impact of stored waveform QC parameters on the size of the
database you may:

#. Compute the QC parameters in real time using :ref:`scqc` and save them in the
   |scname| database. Saving the QC parameters in the database requires to
   adjust the scqc module configuration parameters
   ``plugins.*.archive.interval`` for each plugin.
#. Regularly read QC parameters from the database and clean it up, e.g. in a
   cron job:

   #. use scqueryqc for some time span to read the QC parameters from the database.
      Save them in XML files. Example for all QC parameters fround for all
      streams in the inventory beginning at some some time:

      .. code-block:: sh

         scqueryqc -d [host] -b '2021-11-20 00:00:00' --streams-from-inventory -o [XML file]

   #. clean the database from the QC parameters saved in XML files using
      :ref:`scdispatch`. You may need to set the routing table for sending the
      QualityControl parameters to the right message group

      .. code-block:: sh

         scdispatch -H [host] -O remove --routingtable QualityControl:QC -i [XML file]


Examples
--------

* Query rms and delay values for the stream AU.AS18..SHZ,AU.AS19..SHZ from
  '2021-11-20 00:00:00' until current. Write the XML to stdout ::

     scqueryqc -d localhost -b '2021-11-20 00:00:00' -p rms,delay -i AU.AS18..SHZ,AU.AS19..SHZ

* Query all default QC parameter values for all streams found in the inventory
  from '2021-11-20 00:00:00' until current. Write the formatted XML output to
  :file:`/tmp/query.xml` ::

     scqueryqc -d localhost -b '2021-11-20 00:00:00' --streams-from-inventory -f -o /tmp/query.xml
