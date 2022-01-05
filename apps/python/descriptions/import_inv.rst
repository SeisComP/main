import_inv is a wrapper for inventory converters. Inventory converters convert
an input format such as

.. csv-table::
   :widths: 15 15 70
   :header: Format, Converter, Conversion
   :align: left
   :delim: ;

   scml; :ref:`scml2inv`; :ref:`SeisComP inventory XML <concepts_inventory>`, schema: :file:`$SEISCOMP_ROOT/share/xml/`
   sc3; :ref:`sc32inv`; Alias for scml for backwards compatibility to SeisComP3
   arclink; :ref:`arclink2inv`; Arclink inventory XML
   dlsv; :ref:`dlsv2inv`; `dataless SEED <http://www.iris.edu/data/dataless.htm>`_
   fdsnxml; :ref:`fdsnxml2inv`; `FDSN StationXML <http://www.fdsn.org/xml/station/>`_

to SeisComP inventory XML which is read by the trunk config module to
synchronize the local inventory file pool with the central inventory database.

For printing all available formats call

.. code-block:: sh

   $ import_inv help formats

When :program:`import_inv help formats` is called it globs for
:file:`$SEISCOMP_ROOT/bin/*2inv`.
If another format needs to be converted, it is very easy to provide a new
converter.


Converter interface
-------------------

For making a new converter work with import_inv it must implement an interface
on shell level. Furthermore the converter program must be named
:file:`{format}2inv` and must live in :file:`SEISCOMP_ROOT/bin`.

The converter program must take the input location (file, directory, URL, ...)
as first parameter and the output file (SeisComP XML) as second parameter. The
output file must be optional and default to stdout.

To add a new converter for a new format, e.g. Excel, place the new converter
program at :file:`$SEISCOMP_ROOT/bin/excel2inv`.


Examples
--------

* Convert inventory file in FDSN StationXML format (fdsnxml) and copy the content to
  :file:`$SEISCOMP_ROOT/etc/inventoy/inventory.xml`. The call will invoke
  :ref:`fdsnxml2inv` for actually making the conversion:

  .. code-block:: sh

     $ import_inv fdsnxml inventory_fdsn.xml $SEISCOMP_ROOT/etc/inventoy/inventory.xml
