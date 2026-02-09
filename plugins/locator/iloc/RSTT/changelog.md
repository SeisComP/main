Changelog
===========================

v3.2.1 (12 Sep 2025)
------------------------------

This is mainly a bug fix release. Several buffer overflows have been fixed, as
well as a bug that resulted in a segfault when attempting to save an old-type
model file in a new-type format. The random pdu uncertainty flag ∑as also
erroneously set to True in the pdu202009Du.geotess. This has been corrected, and
it is now set to False by default.

Significant backend work has been done towards the goal of making RSTT
thread-safe with a parallelizable `getTravelTime()` method. This includes
getters for a variety of class members, and a large amount of new copy
constructors. This work is ongoing.

Makefiles have been additionally been overhauled, and this version should
compile correctly on Intel and ARM Macs, RHEL 8 and 9, Ubuntu 24.04 LTS, and WSL
equivalents with both Clang and GNU compilers.


v3.2.0 (10 Feb 2021)
------------------------------

The major new feature in this update is a Python interface! **Note that there is
a minimum requirement of Python 3.6.**

The Python interface can be installed using `pip` (see instructions in the
[README](readme.html)), and as it contains the required C++ GeoTess and RSTT
libraries inside of the module package, itself, if you install it via `pip`, you
will not need to set any path variables—it should "just work".

Alternatively, an extracted version of the module is stored in the
`$RSTT_ROOT/lib` directory, so if you do not want to `pip install` the RSTT
module, just set the `$SRTT_ROOT` environment variable, and use the Python code
at the top of `usage_examples/python_example.py` in your own programs to load
the module. The example Python code has been written to recognize `$RSTT_ROOT`,
`$RSTT_HOME`, `$SLBM_ROOT`, and `$SLBM_HOME` as potential search paths.

This update also makes a considerable number of other changes under-the-hood,
mostly in the path-dependent uncertainty RSTT model file format. The new model
format is compatible with the new version of [GeoTess](https://www.sandia.gov/geotess)
(v2.6.1) and will be the permanent new binary format going forward. The format
introduced in v3.1.0 with the `pdu2020012Du` model is no longer supported, and
that model has been converted to the new format and renamed to `pdu202009Du`.
The new versions of *GeoTess*, *GeoTess Builder*, and *GeoTess Explorer*,
contain robust new methods to interact with these new model files, including
resampling the grid and extracting and replacing the PDU data. The old models
(e.g., `rstt201404um`) are still supported for backwards compatibility.

Other miscellaneous changes and fixes:

* Henceforth, path-dependent uncertainty will be referred to as "**PDU**", and
  the old 1D distance-dependent uncertainties (i.e., path-independent
  uncertainty) will be called "**PIU**". This naming convention is concise and
  convenient.

* Improved the path situation. The Java and Python usage examples contain
  well-commented code demonstrating how to load the proper files provided one of
  these environmental variables is set: `$RSTT_ROOT`, `$RSTT_HOME`,
  `$SLBM_ROOT`, and `$SLBM_HOME`.

* Added a `getModelObject` method to the C++ `SlbmInterface` object. Previously
  it was difficult to get access to the `GeoTessModelSLBM` that contained all
  the model information; instead, you had to go through the `Grid` class.

* Added getters for the great circle information in C++ `SlbmInterface`.

* Overloaded literally every C++ method, where possible, that passed or returned
  arrays with new merthods that take vectors, instead. These new methods no
  longer require you to pre-allocate arrays or pass in the existing or
  anticipated size of arrays. Instead, vectors will be expanded as needed, and
  the length of the vectors you pass in is easily determined by code.

* Added an `isEqual` method in the C++ and Python `SlbmInterface` to compare the
  velocity model and/or great circle loaded into an `SlbmInterface` object.

* Overloaded the `==` operator in C++ and Python to call `isEqual`, so two
  `SlbmInterface` objects can be easily compared.

* Added a `modelsEqual` static method to every interface, which takes two model
  pathnames as strings for input and tests whether they are equal. This function
  works on both `*.geotess` files as well as directories containing a
  `geotessmodel` file following the old v3 model format.

* Implemented read/write functionality for the new PDU models into SLBM model
  format v3, callable via `saveVelocityModel`.

* Removed `SLBMVersion.h`. This file was never used.

* Updated internal version of GeoTess to v2.6.1.

* Many small improvements to the files in `usage_examples`. These are intended
  to help new and old users, alike, to base their own programs on.

* Resampled the new `pdu202009Du` model (don't forget that it's just
  `pdu2020012Du` reformatted into the new file format) down to a resolution of
  30° and named it `unittest.geotess`. This is a *very* small model file (a few
  hundred kilobytes) that is useful for testing.

* Replaced all of the tests for all of the interfaces to use the new
  `unittest.geotess` file. The old tests were hard-coded to use `rstt201404um`,
  which is both old and a fairly large file to keep around just for testing. The
  new tests are called "unit tests", but they don't rely on any unit testing
  frameworks. They're just short, simple programs that ensure that the travel
  time and uncertainty computed for a single great circle matches what it is
  supposed to be.

* Since v3.1.0, there have been tens of thousands of changes to the C++
  codebase. These changes include numerous small bugfixes, dozens of convenient
  new overloaded functions in the C++ `SlbmInterface`, and substantially
  rewritten architecture for the new model type.


v3.1.0 (06 May 2020)
------------------------------

This is the first new release since 2016, and there have been a thousands of
lines of code changes under the hood to make this a bump-up to a major release
(v3.0.5 --> v3.1.0).

The most significant change has been the addition of support for path-dependent
travel-time uncertainties when used with an appropriate model. This version of
RSTT is being distributed with a new model, `pdu2020012Du.geotess`, which is
prefixed with "*pdu*" to signify that it's a "*path-dependent uncertainty*"-type
RSTT model. The old model, `rstt201404um`, has not yet been removed because it
is still used for running tests during the build process.

PDU-type models are automatically detected by RSTT, and calls to get travel-time
uncertainty are automatically sent to the new algorithm which computes
path-dependent travel-time uncertainty. If an old-style model is used, calls
fall back to the old 1D travel-time uncertainty computation. If the user wishes
to perform the old 1D travel-time uncertainty computation while using a new
PDU-type model, new function calls have been provided with a "1D" suffix (e.g.,
`getTravelTimeUncertainty()` vs. `getTravelTimeUncertainty1D()`).

While the number of individual code changes is too large to put in a changelog,
a summary of some of the more notable changes are lited below.

* Support for PDU-type (path-dependent uncertainty) models

* Inclusion of a new PDU-type model (`pdu2020012Du.geotess`)

* Simple usage examples in each language (*C++*, *C*, *Java*, *Fortran*)

* A revamped README, viewable in HTML or Markdown

* Completely rewritten Makefiles and build process, eliminating the need for
  setting environment variables such as `LD_LIBRARY_PATH`

* Completely updated *Doxygen* and *Javadoc* documentation

* Updated included version of GeoTess *C++* from 2.2.2 to 2.2.3

* Elimination of all previous compiler warnings

* Slight change to crustal leg slowness computation for Pg/Lg phases to prevent
  travel times returning as zero

* Many bugfixes and improvements


v3.0.5 (22 Jul 2016)
------------------------------

Changes to allow SLBM to be compiled on the latest version of MAC OSX. Also made
model `rstt201404um` the default model delivered with the software.


v3.0.4 (15 Oct 2014)
------------------------------

Improved the computational performance of the natural neighbor interpolation
algorithm in geotess, which speeds up SLBM as well.


v3.0.2 (17 Oct 2013)
------------------------------

Fixed a bug that prevented SLBM from writing models to disk using file format 3
when the appropriate tessellation directory was not present.


v3.0.0 (06 May 2013)
------------------------------

Replaced the grid used in SLBM Version 2 with GeoTess, a general-purpose Earth
model parameterization for 2D and 3D Earth models. For more information about
GeoTess, visit the website at http://www.sandia.gov/geotess.

The change to GeoTess introduced a couple of new model storage formats which are
described in `SLBM_Design.pdf`


v2.8.4 (21 Feb 2012)
------------------------------

Made modifications to the makefile and minor mods to code in order to get code
to compile on Windows (Brian Kraus). Can now build and run the *C++*, *C* and
Fortran interfaces on Windows using Visual Studio compiler. *Java* interface
does not currently work on windows


v2.8.3 (25 Jan 2012)
------------------------------

No code changes. Added new model while in Vienna delivering to the IDC.


v2.8.2 (13 Jan 2012)
------------------------------

Changed compiler flags for Mac builds so that now SLBM can be built on Macs with
the *gcc 4.2.1* compiler, which is the version delivered with Mac Xcode. All
four SLBM interfaces (*C++*, *C*, *Fortran* and *Java*) now run on Mac, Linux,
and Sun.

Changes to the main make file to facilitate building on Sun using gmake. To
build on SunOS computers, users must use gmake, not make.

Renamed the `SLBM_User_Guide` to `SLBM_Installation_Guide`.

This release involved no changes to the SLBM code or to the RSTT model.


v2.8.1 (19 Dec 2011)
------------------------------

Overhauled the makefiles to make them easier to use and more consistent with
each other.

Rewrote the `User_Manual` to bring it up to date. Copied the content of the
`User_Manual`, which is in the doc directory, to a readme.txt file in the main
`SLBM_Root` directory. `Readme.txt` is a simple ascii text file that is very
easy to access from the command line.


v2.8.0 (29 Nov 2011)
------------------------------

Removed parameter 'delta' from createGreatCircle() in all interfaces, which will
break existing code (sorry). This parameter proved to be very confusing to new
users.

Added methods getPathIncrement() and setPathIncrement() which allow applications
to get/set the step size of nodes along the headwave interface used to integrate
travel time. This replaces the 'delta' parameter described above.

Changed the default value of `MAX_DISTANCE` from `180` to `15` degrees and the
default value of `MAX_DEPTH` from `9999` to `200` km. SlbmInterface has getters
and setters for these parameters if users wish to change the default values.

In the *C* and *Fortran* interfaces, but not *C++* or *Java*, there were some
inconsistencies in the method calls for getTravelTimeUncertainty() and
getSlownessUncertainty(). These have been resolved but may cause issues for C
and *fortran* applications. If old code does not work, see the html
documentation for the affected method calls.

Fixed an extremely minor memory leak that occurred only as the program was
terminating execution.

Other minor changes to get the code to compile on Mac computers.

The test applications for *C++*, *C* and *Java*, but not *Fortran*, were
expanded with additional functionality. Formerly, these test applications
performed only one test, a regression test. They now have an option to accept a
model, phase, source position and receiver position and will print out basic
information about the travel time, slowness, derivatives, etc. For more
information, execute SLBM_Root/bin/slbmtestcc with no parameters.


v2.7.1 (10 Sep 2011)
------------------------------

Fixed some inaccuracies in the documentation.

Made minor code changes to address compiler warnings observed when the compiling
the Linux version of the code.


v2.7.0 (23 Aug 2011)
------------------------------

Responded to review by the NEIC. Changed manner in which derivatives of tt with
respect to lat, lon are computed.

Removed all derivatives of slowness from all interfaces.

Fixed bug related to computing derivative of tt with respect to depth that
happened when depth step spanned the moho.

Changed default step size used to compute horizontal derivatives from `.01` to
`.001` radians. Changed default step size used to compute depth derivatives from
`0.01` km to `0.1` km. Added getters and setters for `del_distance` and
`del_depth`, which are the step sizes for calculating horizontal and depth
derivatives.

Added methods:

* `getDistAz()` - distance and azimuth from A to B
* `movePoint()` - from A, move distance azimuth to B
* `getPiercePointSource()`
* `getPiercePointReceiver()`
* `getGreatCircleLocations()` - get points along headwave interface
* `getGreatCirclePoints()` - get nPoints between A and B
* `getGreatCirclePointsOnCenters()` - get nPoints between A and B

In the *c* and *fortran* interfaces, error codes returned by all the methods
have been changed. The methods used to return `0` indicating no error, or `1`
indicating an error occurred. The methods still return `0` when no error, but
now return positive, non-zero error codes for error conditions.

Replaced all the test programs with new ones that perform a simple regression
test. These new test programs are better examples of how to use slbm.


v2.6.2 (03 Oct 2010)
------------------------------

Fixed a bug in *java* interface that caused a very slow memory leak. Only the
*java* interface is affected.


v2.6.0 (02 Feb 2010)
------------------------------

Added methods to return derivatives of slowness with respect to lat, lon and
depth.

Modified `SlbmInterface` methods `setGridData()` and `setActiveNodeData()` so
that they now include depth as an argument. This means that it is now possible
to change the depths of the tops of the interfaces in a model.

In the *fortran* interface, all methods used to be mixed case but have all been
changed to lower case. This was required for the port to Linux.

Modified method `GreatCircle_Xn::mnbrak()` to hopefully prevent infinite loop
condition.


v2.5.5 (07 Oct 2009)
------------------------------

Ported all interfaces to Windows and to Linux.

Code changes to remove the discontinuitites from the derivatives of tt with
respect to source position and spikes from second derivatives of tt with respect
to source position. All derivatives are now smooth.

Sandia now holds the copyright to the code and it has been approved by Sandia
for public release under the BSD open source license. There are now copyright
notices, license agreements and disclaimers at the top of every source code
file.


v2.5.1 (11 Mar 2009)
------------------------------

Made a change to Zhao equations, replacing `1.58e-4` with `1/r`. This
dramatically improved the accuracy of the code for mantle events.

Also fixed a bug that generated segmentation faults when source depth was
exactly equal to moho depth (to double precision).

Added a software disclaimer at the top of every source code file


v2.5.0 (14 Jan 2009)
------------------------------

Added methods to compute derivatives of travel time with respect to source
location. New methods are:

* `get_dtt_dlat(double& dtt_dlat)`
* `get_dtt_dlon(double& dtt_dlon)`
* `get_dtt_ddepth(double& dtt_ddepth)`
* `getSlowness(double& slowness)`
* `getSlownessUncertainty(double& slownessUncertainty)`

Renamed method `getUncertainty()` to `getTravelTimeUncertainty()`.

I also added methods to compute derivatives of slowness with respect to lat, lon
and depth:

* `get_dsh_dlat(double& dsh_dlat)`
* `get_dsh_dlon(double& dsh_dlon)`
* `get_dsh_ddepth(double& dsh_ddepth)`

but then commented them out because they produce slowness derivatives that are
invalid. See file `SLBM_Root\doc\slbm_derivatives.pdf` (the same directory as
this file). I will fix these derivatives in a future version and make these
methods available at that time.

All the files specifying travel time uncertainty as a function of distance were
renamed. In previous versions, these files were named
`Uncertainty_[Pn|Sn|Pg|Lg].txt`. They have been renamed
`Uncertainty_[Pn|Sn|Pg|Lg]_TT.txt` in this release. Files containing slowness
uncertainty were added to the same directories with names
`Uncertainty_[Pn|Sn|Pg|Lg]_Sh.txt`. These files were populated with slowness
uncertainty values suggested by Steve Myers in an email to me (SB) on 1-22-2009.


v2.4.5 (09 Jun 2008)
------------------------------

Corrected equation for computing earth radius as a function of latitude. This
makes only an insignificantly small difference ( > `0.1` msec).


v2.4.4 (01 May 2008)
------------------------------

Fixed bug that caused slbm to occasionally segmentation fault for Pg/Lg
calculations when using `TaupLoc`. What was happening was that a velocity stack
was defined at the receiver with the bottom at the radius of the Moho at the
receiver. The Taup model was constructed with only the velocity stack at the
receiver. Then a source comes along where the Moho at the source position is
deeper than the Moho at the receiver (and hence the bottom of the model). In
addition, the source depth is deeper than the bottom of the taup model, causing
taup to fail. This was fixed by modifying the way the taup model is built. Now
we append a taup layer (in the taup model) that extends from the radius of the
Moho at the receiver down to the center of the earth. Thatlayer is assigned the
velocity of the lower crust at the receiver.

Note: It has been discovered that the compiler option to turn off name mangling
required for use of *C++* libraries within *C* and *FORTRAN* code prevents the
compiler from correctly finding the rand() function in *FORTRAN*. At this point,
this is the only function we have discovered to work this way -but others may be
found in the future. At present, the work-around is to manually "decorate" the
function call within the *FORTRAN* code. For example: `rand()` function calls
would change to `rand_()` and `rand(seed)` function calls would change to
`rand_(seed)`.


v2.4.3 (11 Mar 2008)
------------------------------

Added new target (STR) to the makefile to allow rebuilding a 32-bit stand- alone
version of the `SLBM_Root` from a `SNL_Tool_Root` delivery.

Note: `SNL_Tool_Root` only uses the core SLBM units (not any other language
interfaces) and builds only the `libslbm.so` library in 64-Bit mode.


v2.4.2 (04 Mar 2008)
------------------------------

This version contains bug fix to handle cases when the activenode range spans
the dateline correctly. Also, includes Jim Hipp's changes to the uncertainty
units for compatibility w/`PGL`/`UtilLib`.


04 Mar 2008, 10:57 am
------------------------------

Made `ch_max` (in `SLBMInterface`) a static variable and renamed it to `CH_MAX`
defaulting to .2.

Made the function calls

* `void setCHMax(double chMax)`
* `void getCHMax(double& chMax)`
* `void setMaxDistance(double maxDistance)`
* `void getMaxDistance(double& maxDistance)`
* `void setMaxDepth(double maxDepth)`
* `void getMaxDepth(double& maxDepth) `

all static functions. This was done to allow the locator (`LoCOO`) to be able to
set the control parameters remotely.


13 Feb 2008, 7:25 am
------------------------------

Added defaulted depth dependence (defaults to `0.0` km) for the functions

`double getUncertainty(double distance, double depth = 0.0)`

`double getVariance(double distance, double depth = 0.0)`

Added new privated functions to evaluate uncertainty and variance given a depth
index, distance index and distance weight.

`double getUncertainty(double wdist, int idist, int idepth)`

`double getVariance(double wdist, int idist, int idepth)`

Modified the `getIndex(...)` function to return both the index and the weight
given the input independent parameter position, x, and the vector containing the
independent parameter positions. The input vector can by distance or depth.

`void getIndex(double x, const vector& v, int& index, double& w)`

Added the parameter vector errDepths to contain the depth positions. This
parameter does not require a defintion if the input data is strictly distance
dependent.

Modified the parameter vector errVal to a two dimensional vector defined by
`<vector> errval`. This is so a distance vector can be assigned for each depth
defined in `errDepths`.

Modified the function `readFile(const string& filename)` to look on the first
line for both the distance count and the new depth count. If the new depth count
is not present it defaults to `1`. Modified the remainder of the function to
handle depths if found.

Modified `getBufferSize()`, `serialize(...)`, and `deserialize(...)` functions
to output / input the new depth information.


v2.4.1 (13 Feb 2008)
------------------------------

This version contains bug fixes for segmentation faults for events in which the
source and receiver coincide with each other, and for Pg events where
source-receiver distance is greater than `~13` degrees. The slbmtestc and
slbmtestcc test scripts were enhanced to include a stress test of the Pg
calculations.


v2.4.0 (05 Feb 2008)
------------------------------

The major change in this release is that the travel time calculations for Pg/Lg
phases have been modified to implement the algorithm described by Steve Myers in
a memo which is also available on the secure server in
`Documents/SLBM_PgLgFix_Doc.pdf`.


25 Jan 2008, 7:59 am
------------------------------

Added new functions to Grid including

* `const int getBufferSize()`
* `void loadVelocityModelBinary(util::DataBuffer& buffer)`
* `void saveVelocityModelBinary(util::DataBuffer& buffer)`

The first function retrieves the approximate size required of a `DataBuffer`
object to store the contents of the `Grid` object. The next two functions read a
`Grid` object from an input `DataBuffer` and save the contents of the `Grid`
into an input `DataBuffer` object. These functions are required by `PGL` to be
able to store the SLBM model into an FDB.

Since `PGL` really saves the an `SLBMInterface` object these same functions were
provided in `SlbmInterface` to pass-through the request to the Grid object. In
`SlbmInterface` they are called by the same name. A new function to retreive the
model path variable

`const string& getModelPath()`

was also added to the `SlbmInterface` object.

I modified the `Grid` object file base read function to extract the `DataBuffer`
read/write information into separate functions. This avoided duplicating the
functionality in the new calls above. The calls separate out the input of geo
stacks, layer connectivity and grid connectivity as well as initialization
functionality. They are divided into the 4 functions below:

* `void readGeoStacks(util::DataBuffer& buffer)`
* `void readConnectivity(util::DataBuffer& buffer, int& nNodes, vector& elev, vector& waterThick, vector& stackId)`
* `void readTessellationData(util::DataBuffer& buffer, int nNodes, const vector& elev, const vector& waterThick, const vector& stackId, vector< vector >& triset)`
* `void defineTessAdjacency(int nNodes, const vector< vector >& triset)`

Finally, the Uncertainty class was modified so that it could be read from and
written to by a `DataBuffer` object. This included adding a constructor to
serialize from a `DataBuffer`, a function to retrieve the necessary buffer size,
a function to serialize, and a function to deserialize. These functions are
defined by

* `Uncertainty(util::DataBuffer& buffer)`
* `const int getBufferSize()`
* `void serialize(util::DataBuffer& buffer)`
* `void deserialize(util::DataBuffer& buffer)`

Also, I fixed a bug in the Uncertainty file read class where a pth "/"
descriptor was missing. And I replaced the getUncertainty(...) functionality
with a new implementation that uses a fast binary search to find the bracket
values from then input distance.


v2.3.0 Beta (29 Oct 2007)
------------------------------

Modified the way travel time for Pg/Lg phases is calculated. Previously, Pg/Lg
travel times were computed with the Headwave method. Starting with this release,
Pg/Lg travel times are computed with both the Headwave and TauP methods and the
reported travel time is the lesser of the travel times calculated with the two
methods. See `SLBM_design.pdf` for more details. The `SlbmInterface` method

`getPgLgComponents(double* tTotal, double* tTaup, double* tHeadwave, double* rBottom)`

has been added to all interfaces to facilitate evaluation of this new
capability.

Added following methods to the interface(s):

* `getFractionActive ( double* fractionActive )`
* `setMaxDistance ( const double* maxDistance )`
* `getMaxDistance ( double* maxDistance )`
* `setMaxDepth ( const double* maxDepth )`
* `getMaxDepth ( double* maxDepth )`
* `getPgLgComponents(double* tTotal, double* tTaup, double* tHeadwave, double* rBottom)`

Added a section labeld "`test_2_3_0_BETA_Changes()`" in each testscript
(programming language) to exercise each new method.

Due to function name length limitations in some *FORTRAN* compilers, renamed all
functions calls from "`slbm_shell_xxx()`" to "`slbm_xxx()`" in the *FORTRAN*
interface. Additionally, further renamed functions:

`slbm_getActiveNodeWeightsReceiver()` -> `slbm_getActiveNodeWeightsRcvr()`
`slbm_getActiveNodeWeightsSource()` -> `slbm_getActiveNodeWgtsSrc()`

Modified the way `CrustalProfile` objects are stored. Previously, all
`CrustalProfile` objects were stored in memory so that if they were called for
again they would not have to be recomputed. This was a problem since the number
of objects could grow until all available memory was consumed. Starting with
this release, the number of source `CrustalProfile` objects saved is limited to
10 and the number of receiver `CrustalProfile` objects saved is limited to
`1000`. When these limits are reached, the least-recently used objects are
deleted from memory. These limits are hard-coded in the software but can be
modified in the `Grid` constructor in `Grid.cc`.


09 Oct 2007, 1:02 am
------------------------------

Fixed the constructor in `GreatCircle_Xg.cc` during the `TauPSite`
initialization to set the `TauPSite` planet radius before setting the receiver
depth.


02 Oct 2007, 8:15 am
------------------------------

Modified PGL TauP and SLBM code to utilize identical object definitions. This
included: a) modifications to SLBM to wrap all of its objects into the single
namespace `slbm`; b) modifications to PGL/SLBM to wrap all TauP objects into a
new namespace `taup`; c) modifications to PGL/SLBM to wrap all utility objects
used by both applications into a new namespace `util`; d) renaming of the SLBM
`BaseObject.h` include file in SLBM to `BaseGlobals.h`; e) splitting of the
`BaseGlobals.h` class into a non-namespaced basic definition which keeps the
`BaseGlobals` name and a new `SLBMGlobals.h` file containing the specific
definitions required by SLBM; f) the renaming of the PGL `StandardDefines.h`
file to `PGLGlobals.h`; g) the removal of base definitions from `PGLGlobals.h`
and the inclusion of `BaseGlobals.h` in PGL; h) a new `TauPGlobals.h` file used
by the `taup` namespace; i) and a new UtilGlobals.h used by the `util`
namespace.

Each specific `XXGlobal.h` file contains a windows definition for the DLL
import/export tag along with any specific defintions (beyond `BaseGlobals.h`)
required by the namespace.

The `taup` namespace now includes the objects `TPVelocityModels`,
`TauPSiteFunctionals`, `TauPSite`, and `TauPModel` (used by PGL only). The
`util` namespace now includes `MD50`, `Brents`, and `IntegrateFunction` objects.
Once the `DataBuffer` object in PGL is split into a new `DataBufferDB` object
and the base functionality included in the SLBM `DataBuffer` object, then the
low level `DataBuffer` object will also become part of the `util` namespace.


v2.2.0 Beta (14 Aug 2007)
------------------------------

Converted the model files from ascii to binary format. The binary format is the
one specified by AFTAC/QTSI. This change has resulted in a dramatic reduction in
the time required to load the velocity models into memory.

The ascii format is still supported but support for ascii will be removed in the
next release.

Added setters and getters for `CHMax` and average mantle velocities. Added
`getTessId()` to retrieve tesselation ID of the model in memory. Refer to HTML
documentation for further details.

Sandy Ballard, SNL


v2.1.0 Beta (25 Jul 2007)
------------------------------

Added several methods to all the slbm interfaces to facilitate interaction with
active nodes. Active nodes are nodes that fall within a user-specified latitud,
longitude range. This functionality is really only useful to the tomographers.


v2.0.0 Beta (25 Jun 2007)
------------------------------

Changed the grid from the deformed grid to the global tessellated grid.

Added method `getVersion()` to all the interfaces.

Added the following methods to `SlbmInterface` to support regularization for
tomography applications. See html documentation for more information.

* `void getNodeNeighbors(const int& nid, int neighbors[], int& nNeighbors)`
* `void getNodeNeighbors(const int& nid, vector& neighbors)`
* `void getNodeNeighborInfo(const int& nid, int neighbors[], double distance[], double azimuth[], int& nNeighbors)`
* `void getNodeSeparation(const int& node1, const int& node2, double& distance)`
* `void getNodeAzimuth(const int& node1, const int& node2, double& azimuth)`
* `void initializeActiveNodes(const double& latmin, const double& lonmin, const double& latmax, const double& lonmax)`
* `void nextActiveNode(int& nodeId)`
* `void getNodeHitCount(const int& nodeId, int& hitCount)`


05 Jun 2007
------------------------------

Fixed a bug in the part of the code that computes Pn/Sn travel times for mantle
events. The bug caused the code to hang for events just below the Moho and close
to the receiver. The test events seemed to return very similar travel times for
very small differences in ray parameter. The bug was fixed by simply adding a
counter that caused the loop to quit after a specified number of iterations.

Removed a spurious debug print statement that was inadvertently left in the
*fortran* shell interface.

Added tests for ellipticity corrections and elevation corrections. The results
of these tests had been distributed previously, but are now part of the official
release.


23 May 2007
------------------------------

Modified the method where Pn/Sn travel times are computed for mantle events.
Basically made the convergence behavior more robust and imposed a criterion that
`C * H < 0.2` where `C` is the Zhao `C` parameter and `H` is the depth of the
ray turning point below the Moho.

Removed criterion that mantle gradients had to be `>= 0` and replaced it with
criterion that `C` and `Cm` have to be greater than `1e-6`.

Added 5 accessor methods to `SlbmInterface` (in all 4 languages):

* `getZhaoParameter()` returns several parameters computed during calculation of Pn/Sn travel times, including `C`, `Cm`, `Vm`, `H` and `udSign` (see documentation for definitions of these parameters).
* `getGridLatMin()`, `getGridLatMax()`, `getGridLonMin()` and `getGridLonMax()` return the valid range of the Earth model loaded with the `loadVelocityModel()` call.

Fixed a very subtle bug in `QueryProfile`. It used to be that interpolation was
done on the basis of depth of surrounding grid nodes. Changed so that
interpolation is done on the basis of radius, which makes a small difference
when the GRS80 ellipsoid is used and Earth radius depends on latitude.


18 Apr 2007
------------------------------

(1) Fixed some bugs having to do with the valid range of the Pn travel time for
mantle events.

(2) Changed the algorithm for computing Pg/Lg travel times. Code now returns
Pg/Lg travel times for events at any depth in the crust, even events below the
top of the middle crust. The algorithm implemented is exactly as described in
the requirement document.

(3) Added new `SLBMInterface` constructors that take an earth radius in km. When
the constructors with no parameters are called, the radius of the earth is
assumed to vary as a function of latitude, as described by a GRS80 ellipsoid.
When the earth radius is specified in the `SLBMInterface` constructor, then
earth radius is assumed constant at the specified value. Geographic latitudes
are still converted to geocentric latitudes, using GRS80 ellipsoid.

(4) Modified the `IASPI91_slbm.txt` model file to have `9` layers instead of
`8`.


16 Apr 2007
------------------------------

(1) Changed the model input file (`unified_slbm.txt`) in two ways: (a) the
constraint that gradients cannot be negative has been removed from the model
input file and is now being imposed in the code. There can now be substantial
negative gradients in the input file. (b) There are now 9 layers in the model
where there were previously only `8`. The `9` layers are:

1. `WATER`
2. `SEDIMENT1`
3. `SEDIMENT2`
4. `SEDIMENT3`
5. `UPPER_CRUST`
6. `MIDDLE_CRUST_N`
7. `MIDDLE_CRUST_G`
8. `LOWER_CRUST`
9. `MANTLE`

The change is that there are now two versions of the `MIDDLE_CRUST`.
`MIDDLE_CRUST_N` is the same as the previous `MIDDLE_CRUST`. `MIDDLE_CRUST_G` is
a new layer that has the same depth as `MIDDLE_CRUST_N` but with P wave velocity
`6.2` km/sec and S wave velocity of `3.5` km/sec. There are if statements in the
code that select `MIDDLE_CRUST_N` for phases Pn and Sn and select
`MIDDLE_CRUST_G` for Pg and Lg.

(2) Implemented the LANL modification to the Zhao method that calculates travel
times for mantle events.


14 Feb 2007
------------------------------

Changed InterpolatedProfile and all derived classes so that they no longer have
to call new to create the data arrays. This involved changing from vector to
double[] arrays in many places. Resulted in factor of 3 improvement in execution
speed.

Fixed bug that prevented slbm from exiting gracefully when invalid file
specified in `Grid::loadVelocityModel()`.

Added the following methods to all interfaces:

* `saveVelocityModel()` - save earth model currently in memory to file.
* `getHeadwaveDistanceKm()` - Retrieve horizontal distance traveled by the ray below the headwave interface, in km.
* `getHeadwaveDistance()` - Retrieve angular distance traveled by the ray below the headwave interface.
* `getSourceDistance()` - returns horizontal offset below the source.
* `getReceiverDistance()` - returns horizontal offset below the receiver.
* `getWeightsSource()` - Retrieve the node IDs and the interpolation coefficients for the source `CrustalProfile`.
* `getWeightsReceiver()` - Retrieve the node IDs and the interpolation coefficients for the receiver `CrustalProfile`.

Fixed many issues with the *fortran* interface.

Updated all of the html documentation files.

Added a mode of operation to `SLBM_test_java` wherein two parameters are
specified on the command line. The first is interpreted as the name of the file
containing the earth model and the second is interpreted to be the name of a
file containing a bunch of source and receiver locations. `SLBM_test_java` will
compute the total travel time and spew the results to standard out. The input
file should contain the following information on each record, separated by
spaces:

* `phase` - one of the following strings: Pn, Sn, Pg or Lg
* `sourceLat` - source latitude in degrees.
* `sourceLon` - source longitude in degrees.
* `sourceDepth` - source depth in km.
* `receiverLat` - receiver latitude in degrees.
* `receiverLon` - receiver longitude in degrees.
* `receiverDepth` - receiver depth (negative elevation) in km.
