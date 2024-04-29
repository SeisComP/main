*evrc* (event region check) is a :term:`plugin` for :ref:`scevent` setting the
event type by comparing the location of the preferred origin with
:ref:`defined regions <sec-evrc-regions>`.

.. note::

   Events for which the mode of the preferred origin is "manual" are by default
   not considered.


.. _sec-evrc-regions:

Definition of regions
---------------------

The regions are defined by closed polygons provided in
:ref:`GeoJSON or BNA files <sec-gui_layers>`. Configure :confval:`rc.regions` to
consider a region defined by its region name. The name is given either

* As a property of the polygon when given in GeoJSON format,
* Or in the header when given in BNA format.

There exist **positive and negative regions**:

* **Positive region:** All events within the area enclosed by the polygon are
  flagged positive, all events not enclosed by the polygon are flagged negative.
* **Negative region:** All events within the area enclosed by the polygon are
  flagged negative, all events not enclosed by the polygon are flagged positive.

Regions are negative if the :confval:`name <rc.regions>` of the enclosing polygon
starts with **!** (exclamation mark. Otherwise the region is positive.

If a list of region names is defined, the last matching region in the list takes
priority when treating events.

.. note::

   * When regions are defined or configured multiple times by polygons or
     :confval:`rc.regions`, respectively, the region is not unique and the
     region check is entirely inactive.
   * When a region is not defined but configured in :confval:`rc.regions`, the
     region check remains active but the region is ignored.

   In both cases, error log message are printed.


Treatment of events
-------------------

When the *evrc* plugin is loaded and configured, the location of the preferred
origin of an events is compared with the defined regions.
Events within a positive and a negative region are flagged positive and
negative, respectively. By default it sets the event type to "outside of network
interest" if the event is flagged negative.

#. When activating :confval:`rc.readEventTypeFromBNA` the type of positive
   events is set according to the eventType defined in
   :ref:`polygon <sec-evrc-polygon>`.
   The type of negative events is set according to :confval:`rc.eventTypeNegative`.
   Prepend 'accept' to the list of polygons to unset the type of negative events.
#. When :confval:`rc.readEventTypeFromBNA` is inactive, the event type is set
   based on :confval:`rc.eventTypePositive` and :confval:`rc.eventTypeNegative`:

   #. by default the type of all negative events (events within negative regions)
      is set to "outside of network interest".
      Prepend **accept** to :confval:`rc.regions` to unset the event type for
      negative events.

   #. **positive:** The event type of positive events is set to
      :confval:`rc.eventTypePositive`. For empty :confval:`rc.eventTypePositive`
      the type is unset.

   #. **negative:** The event type of negative events is set to
      :confval:`rc.eventTypeNegative`. The default type for negative events is
      "outside of network interest".

Evaluation is made based on the order of the regions names defined in
:confval:`rc.regions`. The last matching criteria applies.
In this way disjunct and overlapping regions with different behavior can be
defined. If events ARE NOT within positive regions their type is set to
"outside of network interest".


.. _fig-evrc-region:

.. figure:: media/regions.png
  :align: center
  :width: 10cm

  Disjunct and overlapping regions in front of a default.


Event types
-----------

The event types are either set based the types configured in
:confval:`rc.eventTypePositive` and :confval:`rc.eventTypeNegative`
or based on the type provided in the polygon files if
:confval:`rc.readEventTypeFromBNA` is active.


Type definition
~~~~~~~~~~~~~~~

For defining the event type, any value defined in :cite:t:`uml`.
The list of valid values can also be found in the Event tab of :ref:`scolv`: Type.

Examples for valid event types:

* earthquake
* quarry blast
* nuclear explosion
* not existing
* ...

Invalid values result in errors or debug messages which are reported depending
on the verbosity level of :ref:`scevent` as given :confval:`logging.level` or
:option:`--verbosity`/:option:`-v`.


.. _sec-evrc-polygon:

Event type from polygon
~~~~~~~~~~~~~~~~~~~~~~~

If :confval:`rc.readEventTypeFromBNA` is active, the event type is read from the
polygon defining a region. Use a key-value pair in double quotes to specify the
type where the key is "eventType" and the value is the event type. The
formatting depends on the file format.

The depth of the event can be tested, as well. For events within a region but
with depth outside a depth range the type is not set. The limits of the depth
range can be added to the polygons using the key words *minDepth* and
*maxDepth*. For considering a polygon, the depth *d* of the preferred
:term:`origin` of an :term:`event` must be within the range

.. math::

   minDepth \le d \le maxDepth

The origin depth is only tested if minDepth or maxDepth or both are set and if
:confval:`rc.readEventTypeFromBNA` is active.

.. warning::

   * The names of polygons, e.g. coal, are case sensitive and must not contain
     commas.
   * A hierarchy applies to the reading of GeoJSON/BNA files. Read the section
     :ref:`sec-gui_layers-vector` for the details.

**Example polygon in BNA format:**

.. code-block:: properties

   "coal","rank 1","eventType: mining explosion, minDepth: -5, maxDepth: 10",6
   13.392,50.3002
   13.2244,50.4106
   13.4744,50.5347
   13.6886,50.4945
   13.6089,50.358
   13.6089,50.358

where the name of the polygon / region is "coal" and the considered event type
is "mining explosion". The name and the rank are mandatory fields. All key-value
pairs for eventType, minDepth and maxDepth are written within one single field
enclosed by double quotes.

**Example polygon in GeoJSON format:**

* Single Feature

  For a single Feature and Poylgon, eventType, minDepth are maxDepth are added as
  key-value pair to the properities of the feature:

  .. code-block:: properties

     {
         "type": "Feature",
         "geometry": {
             "type": "Polygon",
             "coordinates": [
                 [
                     [-77.075, -37.7108], [-76.2196, -21.2587], [-69.0919, -7.10994]
                 ]
             ]
         },
         "properties": {
             "name": "mines",
             "rank": 1,
             "eventType": "mining explosion",
             "minDepth": -5,
             "maxDepth": 10
         }
     }

* Single Feature and MultiPoylgon

  For a single Feature and a MultiPoylgon, eventType, minDepth are maxDepth are
  added as key-value pair to the properities of the MultiPoylgon:

  .. code-block:: properties

     {
       "type": "Feature"
       "properties": {
           "name": "mines",
           "rank" : 1,
           "eventType": "mining explosion",
           "minDepth": -5,
           "maxDepth": 10
       },
       "geometry": {
           "type": "MultiPolygon",
           "coordinates": [
                [
                     [
                       [ 10.0, -25.0 ],
                       [ 13.0, -25.0 ],
                       [ 13.0, -22.0 ],
                       [ 10.0, -25.0 ]
                     ]
                ], [
                     [
                       [ 20.0, -25.0 ],
                       [ 23.0, -25.0 ],
                       [ 23.0, -22.0 ],
                       [ 20.0, -25.0 ]
                     ]
                ]
           ]
       }
     }

* FeatureCollection

  For a FeatureCollection, the key-value pairs may be added to the properties of
  each individual feature:


  .. code-block:: properties

     {
         "type": "FeatureCollection",
         "features": [
             { "type": "Feature",
               "properties": {
                   "name": "Krakatau",
                   "rank": 1,
                   "eventType": "mining explosion",
                   "minDepth": -5,
                   "maxDepth": 10
               },
               "geometry": {
                   "type": "Polygon",
                   "coordinates": [ ... ]
                 }
             },
             { "type": "Feature",
               "properties": {
                   "name": "Batu Tara",
                   "rank": 1,
                   "eventType": "mining explosion",
                   "minDepth": -5,
                   "maxDepth": 10
               },
               "geometry": {
                   "type": "Polygon",
                   "coordinates": [ ... ]
                 }
             },
         }
     }


Setting up the Plugin
======================

Load the *evrc* plugin: Add to the global configuration or to the
global configuration of :ref:`scevent`  in the order of priority:

.. code-block:: sh

   plugins = ${plugins},evrc

Add BNA polygons by defining :confval:`rc.regions`.
Use the region name to define positive and negative regions. Names with
leading *!* define negative regions.

.. code-block:: sh

   rc.regions = accept,area

.. note::

   :ref:`scevent` stops
   if the *evrc* plugin is loaded but :confval:`rc.regions` is not defined.

Activate :confval:`rc.readEventTypeFromBNA` and add the eventType key-value pair
to the :ref:`polygons <sec-evrc-polygon>` if the event type
shall be read from GeoJSON or BNA polygon.


**Examples:**

Set type of events within the positive polygon **germany** but do not change the
type outside:

.. code-block:: sh

   rc.regions = accept,germany

Accept all events without setting the type but set the type for all events within
the positive polygon **germany** but consider negative within the polygon
**quarries**:

.. code-block:: sh

   rc.regions = accept,germany,!quarries

Accept all events without setting the type but consider events within the
negative polygon **germany** and events within the positive polygon **saxony**:

.. code-block:: sh

   rc.regions = accept,!germany,saxony
