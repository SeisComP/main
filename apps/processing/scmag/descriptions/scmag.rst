The purpose of scmag is to compute magnitudes from pre-computed amplitudes.
Instead it takes amplitudes and origins as input and produces StationMagnitudes
and (network) Magnitudes as output. It does not access waveforms.
The resulting magnitudes are sent to the "MAGNITUDE" group. scmag doesn’t access
any waveforms. It only uses amplitudes previously calculated.

The purpose of scmag is the decoupling of magnitude computation from amplitude
measurements. This allows several modules to generate amplitudes concurrently,
like :ref:`scautopick` or :ref:`scamp`. As soon as an origin comes in, the amplitudes related
to the picks are taken either from the memory buffer or the database to compute
the magnitudes.


Relationship between amplitudes and origins
-------------------------------------------

scmag makes use of the fact that origins sent by :ref:`scautoloc`, :ref:`scolv`
or other modules include
the complete set of arrivals, which reference picks used for origin computation.
The picks in turn are referenced by a number of amplitudes, some of which are
relevant for magnitude computation.

Read the :ref:`scamp` documentation for more details on amplitude measurements.


.. _scmag-primaryM:

Primary magnitudes
------------------

Primary magnitudes are computed from amplitudes and station-event distances.
Currently the following primary magnitude types are implemented.


Local distances
---------------

:term:`Md <magnitude, duration (Md)>`
   Duration magnitude as described in `HYPOINVERSE`_ .

:term:`Mjma <magnitude, JMA (M_JMA)>`
   Mjma is computed on displacement data using body waves of period < 30s.

:term:`ML <magnitude, local (ML)>`
   Local magnitude calculated on the vertical component using a correction term
   to fit with the standard ML.

:term:`MLh <magnitude, local horizontal (MLh)>`
   Local magnitude calculated on the horizontal components to SED specifications.

:term:`MLv <magnitude, local vertical (MLv)>`
   Local magnitude calculated on the vertical component using a correction term
   to fit with the standard ML.

:term:`MLr <magnitude, local GNS/GEONET (MLr)>`
   Local magnitude calculated from MLv amplitudes based on GNS/GEONET specifications
   for New Zealand.

:term:`MN <magnitude, Nuttli (MN)>`
   Nuttli magnitude for Canada and other Cratonic regions.


Teleseismic distances
---------------------

:term:`mb <magnitude, body-wave (mb)>`
   Narrow band body wave magnitude measured on a WWSSN-SP filtered trace

:term:`mBc <magnitude, cumulative body-wave (mBc)>`
   Cumulative body wave magnitude

:term:`mB <magnitude, broadband body-wave (mB)>`
   Broad band body wave magnitude after `Bormann and Saul`_ (2008)

:term:`Mwp <magnitude, broadband P-wave moment (Mwp)>`
   The body wave magnitude of `Tsuboi`_ et al. (1995)

:term:`Ms_20 <magnitude, surface wave (Ms_20)>`
   Surface-wave magnitude at 20 s period

:term:`Ms(BB) <magnitude, broadband surface wave (Ms(BB))>`
   Broad band surface-wave magnitude


Derived magnitudes
------------------

Additionally, scmag derives the following magnitudes from primary magnitudes:

:term:`Mw(mB) <magnitude, derived mB (Mw(mB))>`
   Estimation of the moment magnitude Mw based on mB using the Mw vs. mB
   regression of `Bormann and Saul`_ (2008)

:term:`Mw(Mwp) <magnitude, derived Mwp (Mw(Mwp))>`
   Estimation of the moment magnitude Mw based on Mwp using the Mw vs. Mwp
   regression of `Whitmore`_ et al. (2002)

:term:`M <magnitude, summary (M)>`
   Summary magnitude, which consists of a weighted average of the individual
   magnitudes and attempts to be a best possible compromise between all magnitudes.
   See below for configuration and also scevent for how to add the summary magnitude
   to the list of possible preferred magnitudes or how to make it always preferred.

   More details are given in the :ref:`section Summary magnitude<scmag-summaryM>`.

:term:`Mw(avg)`
   Estimation of the moment magnitude Mw based on a weighted average of other
   magnitudes, currently MLv, mb and Mw(mB), in future possibly other magnitudes as
   well, especially those suitable for very large events. The purpose of Mw(avg) is
   to have, at any stage during the processing, a “best possible” estimation of the
   magnitude by combining all available magnitudes into a single, weighted average.
   Initially the average will consist of only MLv and/or mb measurements, but as soon
   as Mw(mB) measurements become available, these (and in future other large-event
   magnitudes) become progressively more weight in the average.

If an amplitude is updated, the corresponding magnitude is updated as well.
This allows the computation of preliminary, real-time magnitudes even before
the full length of the P coda is available.


.. _scmag-stationM:

Station magnitudes
==================

Station magnitudes of a :ref:`particular magnitude type <scmag-primaryM>` are
calculated based on amplitude values
considered by this magnitude type and the distance between the event and the station
at which the amplitude was measured. Typically, epicentral distance is used.

.. note::

   Usually station magnitudes use amplitudes of the same type. However, some magnitude
   consider amplitudes of another type. E.g. :term:`MLr <magnitude, local GNS/GEONET (MLr)>`
   uses amplitudes computed for :term:`MLv <magnitude, local vertical (MLv)>`.


.. _scmag-networkM:

Network magnitudes
==================

The network magnitude is a magnitude value summarizing several :ref:`station magnitudes <scmag-stationM>`
values of one :term:`origin`.
Different methods are available for summarizing the station magnitudes:

* mean: the usual mean value
* trimmed mean value:
  To stabilize the network magnitudes the smallest and the largest 12.5% of the
  :term:`station magnitude` values are removed before computing the mean.
* median: the usual median value
* median trimmed mean:
  Removing all station magnitudes with a distance greater than 0.5 (default)
  from the median of all station magnitudes and computing the mean of all
  remaining station magnitudes.

Default values apply for each magnitude type.
In :ref:`scolv` the methods, the stations magnitudes and other parameters can be
selected interactively.


.. _scmag-summaryM:

Summary magnitude
=================

scmag can compute a summary magnitude as a weighted sum from all available
:ref:`network magnitudes <scmag-networkM>`.
This magnitude is typically called **M** as configured in :confval:`summaryMagnitude.type`.

It is computed as a weighted average over the available magnitudes:

.. math::

   M = \frac{\sum w_{i} M_{i}}{\sum w_i}

   w_{i} = a_i stationCount(M_{i}) + b_i

The coefficients a and b can be configured per magnitude type by :confval:`summaryMagnitude.coefficients.a`
and :confval:`summaryMagnitude.coefficients.b`, respectively.
Furthermore each magnitude type can be specifically added to or excluded from the summary magnitude calculation
as defined in :confval:`summaryMagnitude.whitelist` or :confval:`summaryMagnitude.blacklist`,
respectively.

.. note::

   While the magnitudes are computed by scmag the decision about the preferred
   magnitude of an :term:`event` is made by :ref:`scevent`.


Preferred Magnitude
===================

The preferred magnitude of an :term:`event` is set automatically by :ref:`scevent`
or interactively in :ref:`scolv`. It can be any network magnitude or the summary magnitude.


References
==========

.. target-notes::

.. _`HYPOINVERSE` :  https://earthquake.usgs.gov/research/software/#HYPOINVERSE
.. _`Bormann and Saul` : https://pubs.geoscienceworld.org/ssa/srl/article/79/5/698/143470/The-New-IASPEI-Standard-Broadband-Magnitude-mB
.. _`Tsuboi`: https://pubs.geoscienceworld.org/bssa/article-pdf/85/2/606/2708350/BSSA0850020606.pdf
.. _`Whitmore` : https://tsunamisociety.org/STHVol20N4Y2002.pdf
