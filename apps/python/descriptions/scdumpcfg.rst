scdumpcfg reads and prints the :ref:`module or bindings configuration <concepts_configuration>`
for a specific module or for global. This command-line utility is useful for
debugging of configuration parameters.

Related to :program:`scdumpcfg` is :ref:`bindings2cfg` which dumps the bindings
configuration to :term:`SCML`.

Examples
========

#. Dump the global bindings configuration for all stations which have global bindings.

   .. code-block:: sh

      scdumpcfg global -d mysql://sysop:sysop@localhost/seiscomp -B

#. Dump the bindings configuration for all stations which have bindings to a
   :ref:`scautopick` profile. Additionally use *-G* as :ref:`scautopick` inherits global bindings.

   .. code-block:: sh

      scdumpcfg scautopick -d mysql://sysop:sysop@localhost/seiscomp -GB

#. Dump the module global module configuration specifcally searching for the map
   zoom sensitivity and output the result in the format of the |scname| module
   configuration.

   .. code-block:: sh

      scdumpcfg global -d mysql://sysop:sysop@localhost/seiscomp --cfg -P map.zoom.sensitivity

#. Dump the module configuration of scautopick and output in the format of the
   |scname| module configuration.

   .. code-block:: sh

      scdumpcfg scautopick -d mysql://sysop:sysop@localhost/seiscomp --cfg
