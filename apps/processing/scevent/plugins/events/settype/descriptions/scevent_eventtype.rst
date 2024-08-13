The *evtype* pluging sets the type of an event based on comments of picks which
are associated to the preferred origin of this event. The IDs of the comments
from which the comment values are considered must be configured by
:confval:`eventType.pickCommentIDs` and the name of the plugin, *evtype*, must
be added to the :confval:`plugins` parameter. The text of the considered
comments must contain a supported event type.

.. note::

   Other criteria for setting the event type may be added later to this plugin.

Example of a pick comment:

.. code-block:: xml

   <pick publicID="20160914.075944.29-deepc-AB.XYZ..HHZ">
     ...
     <comment>
       <text>earthquake</text>
       <id>deepc:eventTypeHint</id>
     </comment>
     ...
   </pick>

Example configuration (:file:`scevent.cfg`):

.. code-block:: properties

   plugins = evtype

   eventType.setEventType = true
   eventType.pickCommentIDs = scrttv:eventTypeHint, deepc:eventTypeHint
