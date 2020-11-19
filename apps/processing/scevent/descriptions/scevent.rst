As a consequence of a real-time system the |scname| modules creates several
:term:`origins <origin>` (results of localization processes) for one earthquake
or other seismic events because as time
goes by more seismic phases are available. scevent receives these origins and
associates the origins to :term:`events <event>`. It is also possible to import
and associate origins from other agencies.

The main tasks of scevent are:

* :ref:`Associate origins <scevent-assocorg>` to events.
* Set the :ref:`ID of events <scevent-eventid>`.
* Set the :ref:`preferred origin <scevent-preforg>` from all available origins.
* Set the :ref:`preferred magnitude <scevent-prefmag>` from all available magnitudes.
* Set the :ref:`preferred focal mechanism <scevent-preffm>` from all available focal mechanisms.
* *Optional:* Set the event type of automatic origins based on the plugin
  :ref:`evrc <scevent_regioncheck>` and the hypocenter location of the preferred
  origin. This requires the configuration of the :ref:`evrc <scevent_regioncheck>`
  plugin and of regions by :ref:`BNA polygons <sec-gui_layers>`.


.. _scevent-assocorg:

Origin Association
==================

scevent associates origins to events by searching for the best match of the new
(incoming) origin to other origins for existing events:

* Associate origins belonging to belonging to **the same seismic event**
  to the same event object in |scname|.
* Associate origins belonging to **different seismic events**
  to different event objects in |scname|.

If a match is not found, a new event can be formed.

Origin match
------------

The new origin is matched to existing origins by comparing differences in epicenter,
origin time, and :term:`arrivals <arrival>` (associated picks).
The new origin is matched to an existing origin which has the highest rank in
the following three groups (1, 2, 3):

1. Location and Time (lowest)

   The difference in horizontal location is less than
   :confval:`eventAssociation.maximumDistance` (degrees)
   and the difference in origin times is less than
   :confval:`eventAssociation.maximumTimeSpan`.

2. Picks

   The two origins have more than :confval:`eventAssociation.minimumMatchingArrivals`
   matching picks. Picks are matched either by ID or by time depending
   on :confval:`eventAssociation.maximumMatchingArrivalTimeDiff`.

3. Picks and Location and Time (highest)

   This is the best match, for which both the location-and-time and picks
   criteria above are satisfied.

If more than one origin is found in the highest ranking class, then the first
one of them is chosen.

.. note::

   For efficiency events in the cache are scanned first and if no matches are found,
   the database is scanned for the time window :confval:`eventAssociation.eventTimeBefore` - :confval:`eventAssociation.eventTimeAfter`
   around the incoming Origin time. The cached events are ordered by eventID and
   thus in time.

No origin match
---------------

If no event with an origin that matches the incoming origin is found, then a
new event is formed and the origin is associated to that event. The following
criteria are applied to allow the creation of the new event:

* The agency for the origin is not black listed (:confval:`processing.blacklist.agencies`).
* The origin is automatic and it has more than :confval:`eventAssociation.minimumDefiningPhases`
  :term:`arrivals <arrival>` (associated picks).

.. figure:: media/scevent/Association_of_an_origin_by_matching_picks.jpg
    :scale: 50 %
    :alt: alternate association of an origin by matching picks.

    Associations of an origin to an event by matching picks.


.. _scevent-preforg:

Preferred Origin
================

The preferred origin is set by ranking of all associated origins. The ranking
is controlled by :confval:`eventAssociation.priorities` and related configuration
parameters.


.. _scevent-prefmag:

Preferred Magnitude
===================

The preferred magnitude is set by ranking of the
:ref:`summary magnitude <scmag-summaryM>` and all :ref:`network magnitudes <scmag-networkM>`
of the preferred origin. The ranking is mainly controlled by
:confval:`eventAssociation.magTypes` and :confval:`eventAssociation.minimumMagnitudes`
and related configuration parameters.


.. _scevent-preffm:

Preferred Focal Mechanism
=========================

The most recent manual focal mechanism or, if no manual ones are unavailable, the
most recent automatic focal mechnisms becomes preferred.


.. _scevent-eventid:

ID of events
============

The ID of an event or eventID uniquely identifies an event. The ID is derived from
the time of occurrence of the event within a year. As configured by :confval:`eventIDPattern`
it typically consists of a prefix configured by :confval:`eventIDPrefix` and a
string containing the year and a set of characters or numbers defining the time.
