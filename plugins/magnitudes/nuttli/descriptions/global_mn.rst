MN is the Nuttli magnitude :cite:p:`nuttli-1973` for Canada and other Cratonic
regions. It is implemented by the *nuttli* plugin according to the
Geological Survey of Canada (NRCan).

For measuring AMN amplitudes and for computing MN magnitudes |scname| provides
regionalization.


Amplitude
---------

Amplitude unit in |scname|: **meter/second** (m/s)


Settings
========

Add the *nuttli* plugin to the list of loaded plugins e.g. in the global module configuration:

.. code-block:: sh

   plugins = ${plugins},nuttli

Adjust MN-specific global bindings parameters in the amplitude section and set the
region-specific calibration parameters in the global module configuration.


scamp
-----

Add the Nuttli amplitude type, **AMN**, to the range of magnitudes for which the amplitudes are
to be calculated by :ref:`scamp`, e.g.:

.. code-block:: sh

   amplitudes = AMN

Adjust MN-specific global bindings parameters in the amplitude section and set the
region-specific calibration parameters in the global module configuration
(amplitude section).

.. note::

   Provide *AMN* for computing Nuttli-type amplitudes.


scmag
-----

Add the Nuttli magnitude type, **MN**, to the range of magnitudes to be calculated by
:ref:`scmag`, e.g.:

.. code-block:: sh

   magnitudes = MN

Adjust MN-specific global bindings parameters in the magnitude section and define
the region polygons in the global module configuration (magnitude section).
