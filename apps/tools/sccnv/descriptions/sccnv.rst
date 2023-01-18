sccnv reads input given in a supported format, converts the content to another
format and writes the output. Use the command-line option :confval:`format-list`
for a list of supported formats.


Formats
=======

Different formats are supported for input and output files.

.. csv-table::
   :widths: 10, 60, 10, 10
   :header: Name, Description, Input, Output
   :align: left

   arclink    , `Arclink XML <https://www.seiscomp.de/seiscomp3/doc/applications/arclink-status-xml.html>`_ ,    X    ,    X
   bson       ,                                                                                             ,    X    ,    X
   bson-json  ,                                                                                             ,         ,    X
   csv        , comma-separated values                                                                      ,         ,    X
   hyp71sum2k , Hypo71 format                                                                               ,         ,    X
   ims10      ,                                                                                             ,         ,    X
   json       , `JSON <https://www.json.org/>`_ format                                                      ,    X    ,    X
   qml1.2     , :term:`QuakeML` format                                                                      ,   \*    ,    X
   qml1.2rt   , :term:`QuakeML` real time (RT) format                                                       ,   \*    ,    X
   scdm0.51   ,                                                                                             ,    X    ,    X
   trunk      , SeisComP XML (:term:`SCML`) - :ref:`SCML API <api-datamodel-python>`                        ,    X    ,    X

**\***: The conversion from files in QuakeML format is not supported by sccnv
but can be realized by system tools. Read section :ref:`sec-sccnv-quakeml` for
details and instructions.


.. _sec-sccnv-quakeml:

QuakeML
-------

:term:`QuakeML` is used in a variety of flavors involving, e.g.,

* Using non-standard objects,
* PublicID references which are not globally unique,
* Missing references to parent objects,
* Missing creationInfo parameters.

The ability to convert from QuakeML to :term:`SCML` is thus limited and it
depends on the parameters provided with the input QuakeML file.

However, XSLT stylesheets are provided for mapping the parameters. The files
are located in :file:`@DATADIR@/xml/[version]/` for different |scname| data schema
versions. The stylesheet files provide information on the mapping and on
limitations as well as examples on their application.

.. note::

   You may find out about the |scname| data schema version using modules along
   with the command-line option `-V`, e.g.,

   .. code-block:: sh

      $ sccnv -V

The style sheets can be used along with other stylesheet converter tools provided
by your system, e.g., :program:`xalan` or :program:`xsltproc`. Examples are given
in section :ref:`sec-sccnv-examples`.


.. _sec-sccnv-examples:

Examples
========

* Print the list of supported formats:

  .. code-block:: sh

     $ sccnv --format-list

* Convert an  event parameter file in :term:`SCML` format to :term:`QuakeML` and
  store the content in a file:

  .. code-block:: sh

     $ sccnv -i seiscomp.xml -o qml1.2:quakeml.xml

* Convert an inventory file in Arclink XML format to :term:`SCML` and store the
  content in a file:

  .. code-block:: sh

     $ sccnv -i arclink:Package_inventory.xml -o inventory.sc.xml

* Convert an event parameter file in :term:`SCML` format to ims1.0 and store the
  content in a file:

  .. code-block:: sh

     $ sccnv -i trunk:event.xml -o ims10:event.ims

* Convert QuakeML in version 1.2 to SCML in data schema version 0.12:

  .. code-block:: sh

     $ xsltproc $SEISCOMP_ROOT/share/xml/0.12/quakeml_1.2__sc3ml_0.12.xsl file.quakeml > file_sc.xml
