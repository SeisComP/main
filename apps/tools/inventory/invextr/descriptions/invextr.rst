invextr reads and modifies inventory XML provided as file or on stdin:

* Extract or remove networks, stations and channels based on

  * channel IDs
  * geographic region
  * time

* Clean inventories from unused objects such as data loggers, sensors or
  instrument responses.

The important parameters are:

* Channel ID list (required)
* Input file or stdin
* Output file or stdout
* Region bounding box (optional)

whereas the output file defaults to stdout and the input file to
stdin if not given.

The optional region box will be used to filter the read inventory based on the
coordinates of sensor locations. Only stations with sensor locations within the
region will be considered. All others will be ignored.

A channel ID is a simple string that is matched against the final channel ID
in the inventory. This final channel ID is constructed by joining the codes of
all stages with a dot where the stages are network, station, location and
channel.

The content of the resulting inventory may be listed using :ref:`scinv`.


Examples
--------

Suppose an inventory with network GE, a station MORC and several channels:

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

* The IDs are matched against streams passed with --chans.

  .. code-block:: sh

     invextr --chans "GE*" inv.xml

  All streams are passed and nothing is filtered because GE* matches all
  available IDs and region filter is not used. Since :file:`inv.xml` only
  contains stations from the GE network the option :option:`--chans` is not
  useful here at all.

  .. code-block:: sh

     invextr -r 0,-180,90,180 inv.xml

   All streams located in the northern hemisphere are passed as commanded by the
   region bounding box.

* Nothing is filtered again because *MORC* matches all available IDs.

  .. code-block:: sh

     invextr --chans "*MORC*" inv.xml

* Everything is filtered because GE.MORC does not match with any ID. To make it
  work, an asterisk needs to be appended: GE.MORC* or GE.MORC.*.

  .. code-block:: sh

     invextr --chans "GE.MORC" inv.xml


* To extract all vertical components, use:

  .. code-block:: sh

     invextr --chans "*Z" inv.xml

* To extract BHN and LHZ, use:

.. code-block:: sh

   invextr --chans "*BHN,*LHZ" inv.xml

* To remove all HH and SH channels, use:

  .. code-block:: sh

     invextr --rm --chans "*HH?,*SH?" inv.xml
