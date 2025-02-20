invextr reads and modifies inventory XML provided as file or on stdin:

* Extract or remove networks, stations, sensor locations and channels based on

  * stream IDs,
  * geographic region,
  * time.

* Clean inventories from unreferenced objects such as data loggers, sensors or
  instrument responses when extracting.

  .. note::

     Cleaning inventory may also be achieved by merging inventories with
     :ref:`scinv`.

The important parameters are:

* Stream ID list (:option:`--chans`, :option:`--nslc`). Without a stream ID,
  only unreferenced objects are removed when extracting.
* Input file or stdin
* Output file (:option:`--output`) or stdout
* Region bounding box (:option:`--region`, optional)

where the output XML defaults to stdout and the input file to
stdin if not given.

The optional region box will be used to filter the read inventory based on the
coordinates of sensor locations. Only stations with sensor locations within the
region will be considered. All others will be ignored.

A stream ID is a simple string that is matched against the final stream ID
in the inventory. This final stream ID is constructed by joining the codes of
all stages with a dot where the stages are network, station, location and
channel.

The content of the resulting inventory may be listed using :ref:`scinv`.


Examples
--------

Suppose an inventory with network GE, a station MORC with one sensor locations
and several channels:

.. code-block:: sh

   network GE
     station MORC
       location __
         channel BHZ    ID: GE.MORC..BHZ
         channel BHN    ID: GE.MORC..BHN
         channel BHE    ID: GE.MORC..BHE
         channel LHZ    ID: GE.MORC..LHZ
         channel LHN    ID: GE.MORC..LHN
         channel LHE    ID: GE.MORC..LHE

* Just clean inventory from unreferenced objects such as data loggers, sensors
  or instrument responses.

  .. code-block:: sh

     invextr inventory.xml -o inventory-cleaned.xml

* The IDs are matched against streams passed with :option:`--chans`:

  .. code-block:: sh

     invextr --chans "GE*" inventory.xml

  All streams are passed and nothing is filtered because GE* matches all
  available IDs and region filter is not used. Since :file:`inv.xml` only
  contains stations from the GE network the option :option:`--chans` is not
  useful here at all.

  .. code-block:: sh

     invextr -r 0,-180,90,180 inventory.xml

   All streams located in the northern hemisphere are passed as commanded by the
   region bounding box.

* Nothing is filtered again because *MORC* matches all available IDs.

  .. code-block:: sh

     invextr --chans "*MORC*" inventory.xml

* Everything is filtered because GE.MORC does not match with any ID. To make it
  work, an asterisk needs to be appended: GE.MORC* or GE.MORC.*.

  .. code-block:: sh

     invextr --chans "GE.MORC" inventory.xml


* To extract all vertical components, use:

  .. code-block:: sh

     invextr --chans "*Z" inventory.xml

* To extract BHN and LHZ, use:

  .. code-block:: sh

     invextr --chans "*BHN,*LHZ" inventory.xml

* To remove all HH and SH channels, use:

  .. code-block:: sh

     invextr --rm --chans "*HH?,*SH?" inventory.xml
