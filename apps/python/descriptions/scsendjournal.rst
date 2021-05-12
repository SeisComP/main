scsendjournal sends journals to the \scname messaging system.
Currently, journals can be used to command :ref:`scevent`.
The journals command :ref:`scevent` to manipulate event parameters according to
the :ref:`journal actions <scsendjournal-actions>` which must be known to
:ref:`scevent`.

The actions allow to:

* Create new events,
* Modify event parameters,
* Control the association of origins to events.


Synopsis
========

scsendjournal [opts] {objectID} {action} [parameters]


.. _scsendjournal-actions:

Actions
=======

There are specific journal actions for handling non-events and events. The documentation
of :ref:`scevent` contains a :ref:`complete list of journals known to scevent <scevent-journals>`.


None-event specific actions
---------------------------

* **EvNewEvent**: Create a new event from origin with the provided origin ID.
  The origin must be known to :ref:`scevent`.

  Example: Create a new event from the
  origin with given originID. Apply the action in the message system on *localhost*: ::

     scsendjournal -H localhost Origin#20170505130954.736019.318 EvNewEvent


Origin association
------------------

* **EvGrabOrg**: Grab origin and move the origin to the event with the given eventID.
  If the origins is already associated to another event, remove this reference
  in the other event.
* **EvMerge**: Merge events into one event.

  Example: Merge all origins from the source event with eventID *eventS* into the
  target event with eventID *eventT*. Remove event *eventS*. Apply the action in
  the message system on *host*: ::

     scsendjournal -H {host} {eventT} EvMerge {eventS}

* **EvSplitOrg**: Split origins to 2 events.


Event parameters
----------------

* **EvName**: Set *EventDescription* of type *earthquake name*.

  Example, setting the name of the event with
  eventID *gempa2021abcd* to *Petrinja* ::

     scsendjournal -H localhost gempa2020abcd EvName "Petrinja"

* **EvOpComment**: Set event operator's comment.
* **EvPrefFocMecID**: Set event preferred focal mechanism.
* **EvPrefMagTypev:** Set preferred magnitude type.
* **EvPrefMw**: Set Mw from focal mechanism as preferred magnitude.
* **EvPrefOrgAutomatic**: Set the preferred mode to *automatic* corresponding to *unfix* in scolv.
* **EvPrefOrgEvalMode**: Set preferred origin by evaluation mode.
* **EvPrefOrgID**: Set preferred origin by ID.
* **EvRefresh**: Select the preferred origin, the preferred magnitude, update
  the region. Call processors loaded with plugins, e.g. the
  :ref:`evrc <scevent_regioncheck>` plugin for scevent.

  Example: ::

     scsendjournal -H localhost gempa2021abcd EvRefresh

* **EvType**: Set event type.

  Example: Set the type of the event with eventID *gempa2021abcd* to *nuclear explosion*. ::

     scsendjournal -H localhost gempa2021abcd EvType "nuclear explosion"

* **EvTypeCertainty**: set event type certainty.
