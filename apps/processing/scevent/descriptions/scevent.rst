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
* *Optional:* Set the event type of automatic origins based by the plugin
  :ref:`evrc <scevent_regioncheck>` based on the hypocenter location of the
  preferred origin. This requires the configuration of the
  :ref:`evrc <scevent_regioncheck>` plugin and of geographic regions by
  :ref:`polygons in BNA or GeoJSON format <sec-gui_layers>`.


.. _scevent-assocorg:

Origin Association
==================

scevent associates origins to events by searching for the best match of the new
(incoming) origin to other origins for existing events:

* Associate origins belonging to belonging to **the same seismic event**
  to the same event object in |scname|.
* Associate origins belonging to **different seismic events**
  to different event objects in |scname|.

If no match can be found, a new event can be formed.

Origins can be filtered/ignored based on

* ID of agency which has created the origin:
  :confval:`processing.blacklist.agencies`,
  :confval:`processing.whitelist.agencies` (global parameters),
* Hypocenter parameters: :confval:`eventAssociation.region.rect`,
  :confval:`eventAssociation.region.minDepth`,
  :confval:`eventAssociation.region.maxDepth`.


Origin match
------------

The new origin is matched to existing origins by comparing differences in epicenter,
origin time, and :term:`arrivals <arrival>` (associated picks).
The new origin is matched to an existing origin which has the highest rank in
the following three groups (1, 2, 3):

#. Location and Time (lowest)

   The difference in horizontal location is less than
   :confval:`eventAssociation.maximumDistance` (degrees)
   and the difference in origin times is less than
   :confval:`eventAssociation.maximumTimeSpan`.

#. Picks

   The two origins have more than :confval:`eventAssociation.minimumMatchingArrivals`
   matching picks. Picks are matched either by ID or by time depending
   on :confval:`eventAssociation.maximumMatchingArrivalTimeDiff`.

#. Picks and Location and Time (highest)

   This is the best match, for which both the location-and-time and picks
   criteria above are satisfied.

If more than one origin is found in the highest ranking class, then the first
one of them is chosen.

.. note::

   For efficiency events in the cache are scanned first and if no matches are found,
   the database is scanned for the time window :confval:`eventAssociation.eventTimeBefore` -
   :confval:`eventAssociation.eventTimeAfter`
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
    :align: center

    Association of an origin to an event by matching picks.


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

Magnitudes where the evaluation mode is 'rejected' are ignored.


.. _scevent-preffm:

Preferred Focal Mechanism
=========================

The most recent manual focal mechanism or, if no manual ones are unavailable, the
most recent automatic focal mechnisms becomes preferred.


.. _scevent-eventid:

ID of Events
============

The ID of an event or eventID uniquely identifies an event. The ID is derived from
the time of occurrence of the event within a year. As configured by :confval:`eventIDPattern`
it typically consists of a prefix configured by :confval:`eventIDPrefix` and a
string containing the year and a set of characters or numbers defining the time.


.. _scevent-journals:

Journals
========

scevent can be commanded by journals to do a certain action. Journal entries are being
received via the messaging bus to any of the subscribed groups. A journal entry
contains an action, a subject (a publicID of an object) and optional parameters.
Journals can be interactively sent to the messaging by :ref:`scsendjournal`.

If scevent has handled an action, it will send a reply journal entry with
an action formed from the origin action name plus **OK** or **Failed**. The
parameters of the journal entry contain a possible reason.

The following actions are supported by scevent:

.. function:: EvGrabOrg(objectID, parameters)

   Grabs an origin and associates it to the given event. If the origin is
   already associated with another event then its reference to this event
   will be removed.

   :param objectID: The ID of an existing event
   :param parameters: The ID of the origin to be grabbed

.. function:: EvMerge(objectID, parameters)

   Merges an event (source) into another event (target). After successful
   completion the source event will be deleted.

   :param objectID: The ID of an existing event (target)
   :param parameters: The ID of an existing event (source)

.. function:: EvName(objectID, parameters)

   Adds or updates the event description with type "earthquake name".

   :param objectID: The ID of an existing event
   :param parameters: An event name

.. function:: EvNewEvent(objectID, parameters)

   Creates a new event based on a given origin. The origin must not yet be
   associated with another event.

   :param objectID: The origin publicID of the origin which will be used to
                    create the new event.
   :param parameters: Unused

.. function:: EvOpComment(objectID, parameters)

   Adds or updates the event comment text with id "Operator".

   :param objectID: The ID of an existing event
   :param parameters: The comment text

.. function:: EvPrefFocMecID(objectID, parameters)

   Sets the preferred focal mechanism ID of an event. If a focal mechanism ID
   is passed then it will be fixed as preferred solution for this event and
   any subsequent focal mechanism associations will not cause a change of the
   preferred focal mechanism.

   If an empty focal mechanism ID is passed then this is considered as "unfix"
   and scevent will switch back to automatic preferred selection mode.

   :param objectID: The ID of an existing event
   :param parameters: The focal mechanism ID which will become preferred or empty.

.. function:: EvPrefMagType(objectID, parameters)

   Set the preferred magnitude of the event matching the requested magnitude
   type.

   :param objectID: The ID of an existing event
   :param parameters: The desired preferred magnitude type

.. function:: EvPrefMw(objectID, parameters)

   Sets the moment magnitude (Mw) of the preferred focal mechanism as
   preferred magnitude of the event.

   :param objectID: The ID of an existing event
   :param parameters: Boolean flag, either "true" or "false"

.. function:: EvPrefOrgAutomatic(objectID, parameters)

   Releases the fixed origin constraint. This call is equal to :code:`EvPrefOrgID(eventID, '')`.

   :param objectID: The ID of an existing event
   :param parameters: Unused

.. function:: EvPrefOrgEvalMode(objectID, parameters)

   Sets the preferred origin based on an evaluation mode. The configured
   priorities are still valid. If an empty evaluation mode is passed then
   scevent releases this constraint.

   :param objectID: The ID of an existing event
   :param parameters: The evaluation mode ("automatic", "manual") or empty

.. function:: EvPrefOrgID(objectID, parameters)

   Sets the preferred origin ID of an event. If an origin ID is passed then
   it will be fixed as preferred solution for this event and any subsequent
   origin associations will not cause a change of the preferred origin.

   If an empty origin ID is passed then this is considered as "unfix" and
   scevent will switch back to automatic preferred selection mode.

   :param objectID: The ID of an existing event
   :param parameters: The origin ID which will become preferred or empty.

.. function:: EvRefresh(objectID, parameters)

   Refreshes the event information. This operation can be useful if the
   configured fep region files have changed on disc and scevent should
   update the region information. Changed plugin parameters can be another
   reason to refresh the event status.

   :param objectID: The ID of an existing event
   :param parameters: Unused

.. function:: EvSplitOrg(objectID, parameters)

   Remove an origin reference from an event and create a new event for
   this origin.

   :param objectID: The ID of an existing event holding a reference to the
                    given origin ID.
   :param parameters: The ID of the origin to be split

.. function:: EvType(objectID, parameters)

   Sets the event type to the passed value.

   :param objectID: The ID of an existing event
   :param parameters: The event type

.. function:: EvTypeCertainty(objectID, parameters)

   Sets the event type certainty to the passed value.

   :param objectID: The ID of an existing event
   :param parameters: The event type certainty
