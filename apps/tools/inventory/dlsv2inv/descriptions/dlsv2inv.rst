dlsv2inv converts dataless `SEED <http://www.iris.edu/data/dataless.htm>`_ to
SeisComP XML (:term:`SCML`). Due to the limitations of dataless SEED dlsv2inv allows to set
attributes which are not available in dataless such as network type, network
description and so on.

It takes basically two important parameters:

#. input file
#. output file

whereas the output file defaults to stdout if not given.

The SeisComP inventory network and station objects have the attribute archive
which should contain the local datacenter where the information comes from.

While importing the attribute :confval:`datacenterID` is read and written into
the archive attribute of all networks and stations available in the dataless.
The datacenterID can be overridden with the ``--dcid`` command-line option.


Examples
========

#. Convert a given dataless SEED file to SeisComP XML.

   .. code-block:: sh

      dlsv2inv GE.dataless GE.xml

#. Override the datacenterID and leave it blank in the output.

   .. code-block:: sh

      dlsv2inv --dcid "" GE.dataless GE.xml
