scdumpcfg reads and prints the
:ref:`module or bindings configuration <concepts_configuration>`
for a specific module or for global. It even prints the global bindings for modules
which do not have module bindings, such as :ref:`scmv`.

This command-line utility is therefore useful for debugging configuration parameters.

Instead of printing parameters and values for stations, the option :option:`--nlsc`
allows printing a list of the channel considering bindings. The output may be
used, e.g., for

* filtering inventory by :ref:`invextr`
* miniSEED records by :ref:`scart` or :ref:`scmssort`
* event information by :ref:`scevtstreams`.

Related to :program:`scdumpcfg` is :ref:`bindings2cfg` which dumps the bindings
configuration to :term:`SCML`.


Examples
========

#. Dump the global bindings configuration for all stations which have global bindings:

   .. code-block:: sh

      scdumpcfg global -d mysql://sysop:sysop@localhost/seiscomp -B

#. Dump the bindings configuration for all stations which have bindings to a
   :ref:`scautopick` profile. Additionally use *-G* as :ref:`scautopick` inherits global
   bindings:

   .. code-block:: sh

      scdumpcfg scautopick -d localhost -GB

#. Dump the module global module configuration specifcally searching for the map
   zoom sensitivity and output the result in the format of the |scname| module
   configuration:

   .. code-block:: sh

      scdumpcfg global -d localhost --cfg -P map.zoom.sensitivity

#. Dump the module configuration of scautopick and output in the format of the
   |scname| module configuration:

   .. code-block:: sh

      scdumpcfg scautopick -d localhost --cfg

#. Dump global bindings configuration considerd by scmv:

   .. code-block:: sh

      scdumpcfg scmv -d localhost -BG

#. Dump the channel codes defined by scautopick binding as a list of NET.STA.LOC.CHA:

   .. code-block:: sh

      scdumpcfg scautopick -d localhost -B --nslc
