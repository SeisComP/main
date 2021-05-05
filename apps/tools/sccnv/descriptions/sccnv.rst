sccnv reads input given in a supported format, converts the content to another
format and writes the output. Use the command-line option :confval:`format-list`
for a list of supported formats.


Formats
=======

Different formats are supported for input and output files.

+------------+---------------------------------------------------------------------------------------------+---------+---------+
| Name       | Description                                                                                 | Input   | Output  |
+============+=============================================================================================+=========+=========+
| arclink    | `Arclink XML <https://www.seiscomp.de/seiscomp3/doc/applications/arclink-status-xml.html>`_ |    X    |    X    |
+------------+---------------------------------------------------------------------------------------------+---------+---------+
| bson       |                                                                                             |    X    |    X    |
+------------+---------------------------------------------------------------------------------------------+---------+---------+
| bson-json  |                                                                                             |         |    X    |
+------------+---------------------------------------------------------------------------------------------+---------+---------+
| csv        | comma-separated values                                                                      |         |    X    |
+------------+---------------------------------------------------------------------------------------------+---------+---------+
| hyp71sum2k | Hypo71 format                                                                               |         |    X    |
+------------+---------------------------------------------------------------------------------------------+---------+---------+
| ims10      |                                                                                             |         |    X    |
+------------+---------------------------------------------------------------------------------------------+---------+---------+
| json       | `JSON <https://www.json.org/>`_ format                                                      |    X    |    X    |
+------------+---------------------------------------------------------------------------------------------+---------+---------+
| qml1.2     | :term:`QuakeML` format                                                                      |         |    X    |
+------------+---------------------------------------------------------------------------------------------+---------+---------+
| qml1.2rt   | :term:`QuakeML` real time (RT) format                                                       |         |    X    |
+------------+---------------------------------------------------------------------------------------------+---------+---------+
| scdm0.51   |                                                                                             |    X    |    X    |
+------------+---------------------------------------------------------------------------------------------+---------+---------+
| trunk      | SeisComP XML (:term:`SCML`) - :ref:`SCML API <api-datamodel-python>`                        |    X    |    X    |
+------------+---------------------------------------------------------------------------------------------+---------+---------+


Examples
========

#. Print the list of supported formats:

   .. code-block:: sh

      $ sccnv --format-list

#. Convert an  event parameter file in :term:`SCML` format to :term:`QuakeML` and
   store the content in a file:

   .. code-block:: sh

      $ sccnv -i seiscomp.xml -o qml1.2:quakeml.xml

#. Convert an inventory file in Arclink XML format to :term:`SCML` and store the
   content in a file:

   .. code-block:: sh

      $ sccnv -i arclink:Package_inventory.xml -o inventory.sc.xml

#. Convert an event parameter file in :term:`SCML` format to ims1.0 and store the
   content in a file:

   .. code-block:: sh

      $ sccnv -i trunk:event.xml -o ims10:event.ims
