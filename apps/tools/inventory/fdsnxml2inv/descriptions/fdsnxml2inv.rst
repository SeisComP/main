fdsnws2inv is an inventory converter. It converts station meta data from
FDSN StationXML format to SeisComP XML (:term:`SCML`) and back writing the
output to a file, if given, or the command line (stdout).


Examples
========

#. Convert an inventory file in FDSN StationXML format to SCML with formatted XML.
   Redirect the output to a new file:

   .. code-block:: sh

      fdsnxml2inv -f inventory_fdsn.xml inventory_sc.xml

#. Convert an inventory file in SCML format to FDSN StationXML with formatted XML.
   Redirect the output to a new file:

   .. code-block:: sh

      fdsnxml2inv --to-staxml -f inventory_sc.xml inventory_fdns.xml
