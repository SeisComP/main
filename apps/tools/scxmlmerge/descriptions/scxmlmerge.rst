scxmlmerge reads all |scname| elements from one or more XML files in :term:`SCML`
format. It merges the content and prints the result to standard output. The
input can contain and :ref:`SeisComP element<api-datamodel-python>` and the
content can be filtered to print only some elements such as EventParameters.
The output can be redirected into one single file and used by other applications.

The supported :ref:`SeisComP elements<api-datamodel-python>` are:

* Config
* DataAvailability
* EventParameters
* Inventory
* Journaling
* QualityControl
* Routing

By default all supported elements will be parsed and merged. Duplicates are
removed. Use options to restrict the element types.

There are alternative modules for processing inventory XML files:

* :ref:`scinv`: Merge inventory XML files, extract inventory information.
* :ref:`invextr`: Extract and filter inventory information.


Examples
========

#. Merge the all SeisComP elements from 2 XML files into a single XML file:

   .. code-block:: sh

      scxmlmerge file1.xml file2.xml > file.xml

#. Merge the all EventParameters and all Config elements from 2 XML files into a
   single XML file. Other element types will be ignored:

   .. code-block:: sh

      scxmlmerge -E -C file1.xml file2.xml > file.xml
