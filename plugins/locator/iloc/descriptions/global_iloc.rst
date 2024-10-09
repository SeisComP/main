iLoc is a locator developed by István Bondár which has been integrated into
|scname| by :cite:t:`gempa`. It is invoked by the wrapper plugin *lociloc* - the
interface between |scname| and iLoc.
Read the sections :ref:`iloc-setup` and :ref:`iloc-application` for
configuring and using iLoc in |scname|.


Background
----------

iLoc is a locator tool for locating seismic, hydroacoustic and
infrasound sources
based on :term:`phase picks <pick>`. iLoc is based on the location
algorithm developed by :cite:t:`bondár-2009a` and implemented at the
International Seismological Center, (:cite:t:`isc`, :cite:t:`bondár-2018`)
with numerous new features added (:cite:t:`bondár-2018`).
The stand-alone iLoc code can be downloaded from the :cite:t:`iloc-github`
software repository.

Among the major advantages of using iLoc is that it can

* Use any phases with valid travel-time predictions;
* Use seismic, hydroacoustic and infrasound arrival time, slowness and azimuth
  observations in location;
* Use travel-time predictions from a global 3D upper mantle velocity model;
* Use a local 1D velocity model;
* Account for the correlated travel-time prediction error structure due to
  unmodeled 3D velocity heterogeneities;
* Check if the data has sufficient resolution to determine the
  hypocenter depth;
* Identify ground truth (GT5) candidate events.


History
-------

* Originally developed for U.S. Air Force Research Laboratory, today the standard
  at the International Seismological Centre (ISC) replacing previous routines
* Open source, download website: :cite:t:`iloc-github`
* Integrated first in SeisComP3 in 2019
* Basis of the EMSC crowd-source locator, CsLoc since 2019
* EMSC standard as of 2022


iLoc in a nutshell
------------------

* Accounts for correlated travel-time prediction errors
* Initial hypocenter guess from Neighborhood Algorithm search
* Linearised inversion using a priori estimate of the full data covariance matrix
  Attempts for free-depth solution only if there is depth resolution
* Default depth is derived from historical seismicity
* Seismic, hydroacoustic and infrasound observations
* Arrival time, slowness and azimuth measurements
* Uses most ak135 or iasp91 Earth model phases in locating
* Integrated RSTT travel-time predictions
* RSTT is default for Pn/Sn and Pg/Lg
* Local velocity model and local phase TT predictions for Pg/Sg/Lg, Pb/Sb, Pn/Sn.


Algorithms
----------

This section describes some of the principles. The full description of the applied
algorithms can be found in the iLoc documentation provided along with the package
on the :cite:t:`iloc-github` website.


Neighbourhood algorithm
~~~~~~~~~~~~~~~~~~~~~~~

Linearized inversion algorithms are quite sensitive to the initial guess. In order
to find an initial hypocenter guess for the linearized inversion the Neigbourhood
Algorithm (:cite:t:`sambridge-1999`; :cite:t:`sambridge-2001`) is performed
around the starting hypocentre if :confval:`iLoc.profile.$name.DoGridSearch` is active.

During the NA search, we identify the phases with respect to each trial hypocenter
and calculate the misfit of the trial hypocenter. The misfit is defined as the sum
of the :confval:`iLoc.profile.$name.NAlpNorm` residual and a penalty factor that
penalizes against freakish local minima provided by just a few phases. In the first
iteration :confval:`iLoc.profile.$name.NAinitialSample` hypocenter hypotheses are tested,
while the subsequent iterations consider the best :confval:`iLoc.profile.$name.NAcells`
solutions and resample the search space around them with
:confval:`iLoc.profile.$name.NAnextSample` hypocenter hypotheses. The solution with
the lowest misfit after :confval:`iLoc.profile.$name.NAiterMax` iteration is taken
as the initial hypocenter for the linearized least squares inversion.


A grid search can be performed to obtain a better initial hypocenter
guess. The search is performed around the starting hypocenter.
For a very exhaustive search one can increase :confval:`iLoc.profile.$name.NAinitialSample`,
:confval:`iLoc.profile.$name.NAnextSample` and :confval:`iLoc.profile.$name.NAcells`
values. Note that the maximum value for :confval:`iLoc.profile.$name.NAinitialSample`
is around 3500 before hitting memory limits.

An exhaustive search will
considerably slow iLoc down, especially when RSTT predictions are
enabled (:confval:`iLoc.profile.$name.UseRSTT`, :confval:`iLoc.profile.$name.UseRSTTPnSn`,
:confval:`iLoc.profile.$name.UseRSTTPgLg`).


Depth resolution
~~~~~~~~~~~~~~~~

Depth resolution can be provided by a local network, depth phases, core reflections
and to a lesser extent near-regional secondary phases. iLoc attempts for a free-depth
solution if the set of :term:arrivals meets at least one of the following conditions:

* Number of pairs of defining P and depth phases
  :math:`\le` :confval:`iLoc.profile.$name.MinDepthPhases`
* Number of pairs of defining P and core phases
  :math:`\le` :confval:`iLoc.profile.$name.MinCorePhases`
* Number of pairs of defining P and S phases
  :math:`\le` :confval:`iLoc.profile.$name.MinSPpairs`
  within a regional distance of :confval:`iLoc.profile.$name.MaxLocalDistDeg`
  degree
* Number of defining P phases
  :math:`\le` :confval:`iLoc.profile.$name.MinLocalStations`
  within a local distance of :confval:`iLoc.profile.$name.MinLocalStations`
  degree.

If there is insufficient depth resolution provided by the data, or the depth uncertainty
for a free-depth solution exceeds a threshold, the hypocentre depth is set to the depth
from the default depth grid if a grid point for the epicentre location exists; otherwise
it is set to a depth :cite:t:`bolton-2006` assigned to
the corresponding Flinn-Engdahl geographic
region (:cite:t:`young-1996`). The default depth grid (:cite:t:`bondár-2011`)
is defined on a 0.5º x 0.5º grid as the median of all depths in the cell, provided
that there were at least five events in the cell, and the 75–25 percent quartile
range was less than 100 km. The latter constraint is imposed to avoid regions with
both shallow and deep seismicity. Anthropogenic events are fixed to the surface.
Finally, the user can fix the depth to the initial depth.

iLoc reports back how the depth was determined in the FixedDepthType parameter:

* 0 - free depth solution
* 1 - airquake/deepquake, depth fixed to surface/MaxHypocenterDepth
* 2 - depth fixed to depth reported by an agency (not used in |scname|)
* 3 - depth fixed to depth-phase depth
* 4 - anthropogenic event, depth fixed to surface
* 5 - depth fixed to default depth grid depth
* 6 - no default depth grid point exists, fixed to median reported depth
* 7 - no default depth grid point exists, fixed to GRN-dependent depth
* 8 - depth fixed by user provided value


Linearized inversion
~~~~~~~~~~~~~~~~~~~~

Once the Neighbourhood search get close to the global optimum, iloc switches
to an iterative linearized least-squares inversion of travel-time, azimuth and
slowness observations (:cite:t:`bondár-2009b`; :cite:t:`bondár-2011`) to obtain the final solution
for the hypocenter.

The convergence test after (:cite:t:`paige-1982`) is
applied after every iteration. Once a convergent solution is obtained, the location
uncertainty is defined by the a posteriori model covariance matrix. The model
covariance matrix yields the four-dimensional error ellipsoid whose projections
provide the two-dimensional error ellipse and one-dimensional errors for depth
and origin time. These uncertainties are scaled to the 90% confidence level
(:cite:t:`jordan-1981`).

The final hypocentre is tested against the
ground truth selection criteria (:cite:t:`bondár-2009a`),
and it is reported as
a GT5candidate if the solution meets the GT5 criteria.

Some important parameters are:

* :confval:`iLoc.profile.$name.SigmaThreshold`: Residuals that exceed
  :math:`abs(Sigmathreshold * PriorMeasError)` are made non-defining.
* :confval:`iLoc.profile.$name.MinNdefPhases`: Minimum number of observations
  required to attempt for a solution.

If the number of defining arrival times exceed
:confval:`iLoc.profile.$name.MinNdefPhases`, then slowness observations will not
be used in the location.


Integration into |scname|
-------------------------

* Integration of iLoc into |scname| is provided by an external library of
  routines (:cite:t:`iloc-github`).
* |scname| modules call iLoc routines by passing the objects via the plugin
  *lociloc* installed in :file:`@DATADIR@/plugins/lociloc.so`.
* iLoc returns objects to |scname| for integration.
* The iLoc implementation in |scname| retains all original iLoc functionalities.

Read the section :ref:`iloc-setup` for the installation of the iLoc library and
the configuration in |scname|.


Velocity models
---------------

iLoc ships with the global models *iasp91* and *ak135* as well as with regional
seismic travel-time tables, RSTT, which, if activated by configuration, replaces
the global models in areas where they are defined.


.. _iloc-velocity_global:

Global models
~~~~~~~~~~~~~

The global models *iasp91* and *ak135* and RSTT are available by default without
further configuration.


.. _iloc-velocity_rstt:

RSTT
~~~~

RSTT are available in :file:`@DATADIR@/iloc/RSTTmodels/pdu202009Du.geotess`.
Custom RSTT can be integrated into iLoc and provided to |scname|.
For adding custom RSTT to iLoc read the original iLoc documentation from the
:cite:t:`iloc-github` software repository.

The usage of RSTT is controlled per iLoc profile by global configuration
parameters

* :confval:`iLoc.profile.$name.UseRSTT`
* :confval:`iLoc.profile.$name.UseRSTTPnSn`
* :confval:`iLoc.profile.$name.UseRSTTPgLg`


.. _iloc-velocity_local:

Local velocity models
~~~~~~~~~~~~~~~~~~~~~

Custom local velocity models can be provided by a file in
:file:`@DATADIR@/iloc/localmodels`. Example file
:file:`@DATADIR@/iloc/localmodels/test.localmodel.dat`:

.. code-block:: properties

   #
   # test
   #
   # number of layers
   4
        0.000    5.8000    3.4600 x
       20.000    6.5000    3.8500 CONRAD
       45.000    8.0400    4.4800 MOHO
       77.500    8.0400    4.4800 x

Once added, the velocity can be configured in |scname| as set out in section
:ref:`iloc-setup`.


Station elevation
-----------------

iLoc considers station elevation. It calculates the elevation correction,
*elevationCorrection*, for a station as

.. math::

   elevationCorrection = \frac{\sqrt{1 - (surfVel * p)^2} * elev}{surfVel}

where

* *elev*: elevation of the station
* *p*: the ray parameter (horizontal slowness)
* *surfVel*: layer P or S velocity of at the surface depending on the last lag
  of the phase name.


.. note ::

   iLoc does not allow airquakes or source locations above datum (0 km). If the
   depth of an origin becomes negative, iLoc
   fixes the depth to 0 km and the depth type of the origin will be "operator
   assigned".


.. _sec-iloc-references:

Resources
---------

iLoc has taken advantage of many publications or has been cited therein.
Read the section :ref:`sec-references` for a list.


.. _iloc-setup:

Setup in |scname|
=================

#. Add the plugin *lociloc* to the global configuration, e.g. in
   :file:`@SYSTEMCONFIGDIR@/global.cfg`:

   .. code-block:: properties

      plugins = ${plugins}, lociloc

#. Install the dependencies missing for iLoc. For download, the system variable
   *SEISCOMP_ROOT* must be defined which you may wish to test first:

   .. code-block:: sh

      echo $SEISCOMP_ROOT

   In case the variable is undefined, follow the instructions in section
   :ref:`getting-started-variables`.

   After *$SEISCOMP_ROOT* is defined you may install the software dependencies
   for iLoc using the :ref:`install scripts <software_dependencies>` or simply
   the :ref:`seiscomp` script:

   .. code-block:: sh

      seiscomp install-deps iloc

   The install scripts will fetch auxiliary files from :cite:t:`iloc-github`
   and install them in :file:`@DATADIR@/iloc/iLocAuxDir`. For manual download and
   installation read the install scripts located in
   :file:`@DATADIR@/deps/[os]/[version]/install-iloc.sh`.

   .. note ::

      * Check the :cite:t:`iloc-github` website for updates before downloading
        the file since the version number, hence the name of the download file
        may change.
      * Instead of generating the :file:`SEISCOMP_ROOT/share/iloc/iLocAuxDir`
        directory, you can also manually install the dependencies somewhere else,
        create a symbolic link and maintain always the same iLoc version in
        |scname| and externally.

#. Add and configure iLoc profiles for the velocity models. The global models
   *iasp91* and *ak135* are considered by default with default configuration
   parameters even without setting up *iasp91*/*ak135* profiles. You may,
   however, create these profiles for their customization.

   Create new profiles or consider existing ones for adjusting their
   configuration:

   * :confval:`iLoc.profile.$name.globalModel`: The name of the
     :ref:`global model <iloc-velocity_global>`, e.g. *iasp91* or *ak135*.
   * Consider the :ref:`RSTT parameters <iloc-velocity_rstt>`.
   * :confval:`iLoc.profile.$name.LocalVmodel`, :confval:`iLoc.profile.$name.UseLocalTT`
     and :confval:`iLoc.profile.$name.MaxLocalTTDelta`: The definition of a
     :ref:`local velocity model <iloc-velocity_local>`: model file, default
     usability, distance range.
   * :confval:`iLoc.profile.$name.DoNotRenamePhases`: Renaming seismic phases
     automatically
     impacts the usability of the origins with other locators and locator profiles.
     Activate the parameter to avoid phase renaming.
   * Consider the remaining parameters.

   .. note ::

      Creating the profiles allows using the same global velocity model along
      with different local models or RSTT settings in separate profiles.

#. Test the locator using :ref:`scolv` or configure with :ref:`screloc` or other
   locator modules.


.. _iloc-application:

Application in |scname|
=======================

Once the *lociloc* plugin is configured, the iLoc locator can be applied

* Automatically e.g. in :ref:`screloc` or
* Interactively in :ref:`scolv`.

For using iLoc in :ref:`scolv` select it in the locator menu of the Location tab

.. figure:: media/scolv-iloc-locator.png
   :align: center

   Select iLoc locator

along with a profile:

.. figure:: media/scolv-iloc-profile.png
   :align: center

   Select iLoc profile

The parameters for iLoc can be adjusted by pressing the wrench button next to the
locator selection combo box

.. figure:: media/scolv-iloc-change.png
   :align: center

   Start the settings dialog

which opens the iLoc settings dialog:

.. figure:: media/scolv-iloc-settings.png
   :align: center

   Adjust the settings and click OK to confirm

.. warning ::

   By default, automatic phase renaming by iLoc is active. The renaming may
   change the phase names, e.g. from P to Pn.

   Renaming seismic phases automatically will later impact the usability of
   the new origins with other locators and locator
   profiles. Deactivate *DoNotRenamePhases* to avoid phase renaming.

   However,
   when deactivating, iLoc may not provide results if the initial phases do not
   exist in the phase table for the given source depth and epicentral distance.
   Example: For great source depth and small epicentral distance, the first arrival
   phase is p or Pn and not P but |scname| provides P.

After relocating, the iLoc locator and the selected profile are shown in the
:ref:`scolv` Location tab as Method and Earth model, respectively:

.. figure:: media/scolv-iloc-info.png
   :align: center

   Information in scolv Locator tab
