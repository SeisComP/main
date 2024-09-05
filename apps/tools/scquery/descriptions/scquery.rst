*scquery* reads objects such as event information from a
:ref:`SeisComP database <concepts_database>` using custom queries. The results
are written to stdout. The module extends :ref:`scevtls` and :ref:`scorgls`
which are limited to searching event and origin IDs, respectively, by time.

scquery takes into account and requires :ref:`query profiles <scquery_queries>`
for querying the database. The profiles are defined in

* :file:`@SYSTEMCONFIGDIR@/queries.cfg` or
* :file:`@CONFIGDIR@/queries.cfg`

while parameters in the latter take priority. The are no default query profile,
hence they must be created first.


Module Setup
============

.. _scquery_config:

#. Create the query profiles in :file:`queries.cfg` in :file:`@SYSTEMCONFIGDIR@`
   or :file:`@CONFIGDIR@`. The file contains your database queries. Examples for
   MariaDB/MySQL and PostgreSQL are found in the section :ref:`scquery_queries`.

#. **Optional:** Add the database connection parameter to the configuration file
   :file:`scquery.cfg` or :file:`global.cfg` in @CONFIGDIR@ or to @SYSTEMCONFIGDIR@:

   .. code-block:: properties

      database = mysql://sysop:sysop@localhost/seiscomp

   .. hint ::

      If the database connection is configured, the database option
      :confval:`-d <database>` in the section :ref:`Examples<scquery_examples>`
      can be omitted or used to override the configuration.


.. _scquery_examples:

Examples
========

Choose any query profile defined in the :ref:`queries.cfg<scquery_queries>`.
Provide the required parameters in the same order as in the database request.
The required parameters are indicated by hashes, e.g. ##latMin##.

#. List all available query profiles using the command-line option
   :confval:`showqueries`:

   .. code-block:: sh

      scquery --showqueries

#. Profile **event_filter**: Fetch all event IDs and event parameters for events
   with magnitude ranging from 2.5 to 5 in central Germany between 2014 and 2017:

   .. code-block:: sh

      scquery -d localhost/seiscomp eventFilter 50 52 11.5 12.5 2.5 5 2014-01-01 2018-01-01 > events_vogtland.txt

#. Profile **eventByAuthor**: Fetch all event IDs where the preferred origin was
   provided by a specific author for events 2.5 to 5 with 6 to 20 phases in central
   Germany between 2014 and 2017:

   .. code-block:: sh

      scquery -d localhost/seiscomp eventByAuthor 50 52 11.5 12.5 6 20 2.5 5 2014-01-01 2018-01-01 scautoloc > events_vogtland.txt

#. Profile **eventType**: Fetch all event IDs and event times from events
   with the given event type and within the provided time interval:

   .. code-block:: sh

      scquery -d localhost/seiscomp eventType explosion '2017-11-01 00:00:00' '2018-11-01 00:00:00'


.. _scquery_queries:

Queries
=======

Example queries for :ref:`scquery_mariadb` and :ref:`scquery_psql` are given
below.


.. _scquery_mariadb:

MariaDB/MySQL
-------------

**General event/origin queries**

.. code-block:: properties

   queries = eventFilter, eventUncertainty, eventByAuthor, eventWithStationCount, eventType, originByAuthor

   query.eventFilter.description = "Returns all events (lat, lon, mag, time) that fall into a certain region and a magnitude range"
   query.eventFilter = "SELECT PEvent.publicID, Origin.time_value AS OT, Origin.latitude_value,Origin.longitude_value, Origin.depth_value, Magnitude.magnitude_value, Magnitude.type FROM Origin,PublicObject as POrigin, Event, PublicObject AS PEvent, Magnitude, PublicObject as PMagnitude WHERE Event._oid = PEvent._oid AND Origin._oid = POrigin._oid AND Magnitude._oid = PMagnitude._oid AND PMagnitude.publicID=Event.preferredMagnitudeID AND POrigin.publicID = Event.preferredOriginID AND Origin.latitude_value >= ##latMin## AND Origin.latitude_value <= ##latMax## AND Origin.longitude_value >= ##lonMin## AND Origin.longitude_value <= ##lonMax## AND Magnitude.magnitude_value >= ##minMag## AND Magnitude.magnitude_value <= ##maxMag## AND Origin.time_value >= '##startTime##' AND Origin.time_value <= '##endTime##';"

   query.eventUncertainty.description = "Returns all events (eventsIDs, time, lat, lat error, lon, lon error, depth, depth error, magnitude, region name) in the form of an event catalog"
   query.eventUncertainty = "SELECT PEvent.publicID, Origin.time_value AS OT, ROUND(Origin.latitude_value, 3), ROUND(Origin.latitude_uncertainty, 3), ROUND(Origin.longitude_value, 3), ROUND(Origin.longitude_uncertainty, 3), ROUND(Origin.depth_value, 3), ROUND(Origin.depth_uncertainty, 3), ROUND(Magnitude.magnitude_value, 1), EventDescription.text FROM Event, PublicObject AS PEvent, EventDescription, Origin, PublicObject AS POrigin, Magnitude, PublicObject AS PMagnitude WHERE Event._oid = PEvent._oid AND Origin._oid = POrigin._oid AND Magnitude._oid = PMagnitude._oid AND Event.preferredOriginID = POrigin.publicID AND Event.preferredMagnitudeID = PMagnitude.publicID AND Event._oid = EventDescription._parent_oid AND EventDescription.type = 'region name' AND Event.type = '##type##' AND Origin.time_value >= '##startTime##' AND Origin.time_value <= '##endTime##';"

   query.eventByAuthor.description = "Get events by preferred origin author etc"
   query.eventByAuthor = "SELECT PEvent.publicID, Origin.time_value AS OT, Origin.latitude_value AS lat,Origin.longitude_value AS lon, Origin.depth_value AS dep, Magnitude.magnitude_value AS mag, Magnitude.type AS mtype, Origin.quality_usedPhaseCount AS phases, Event.type AS type, Event.typeCertainty AS certainty, Origin.creationInfo_author FROM Origin, PublicObject AS POrigin, Event, PublicObject AS PEvent, Magnitude, PublicObject AS PMagnitude WHERE Event._oid = PEvent._oid AND Origin._oid = POrigin._oid AND Magnitude._oid = PMagnitude._oid AND PMagnitude.publicID = Event.preferredMagnitudeID AND POrigin.publicID = Event.preferredOriginID AND Origin.latitude_value >= ##latMin## AND Origin.latitude_value <= ##latMax## AND Origin.longitude_value >= ##lonMin## AND Origin.longitude_value <= ##lonMax## AND Origin.quality_usedPhaseCount >= ##minPhases## AND Origin.quality_usedPhaseCount <= ##maxPhases## AND Magnitude.magnitude_value >= ##minMag## AND Magnitude.magnitude_value <= ##maxMag## AND Origin.time_value >= '##startTime##' AND Origin.time_value <= '##endTime##' AND Origin.creationInfo_author like '##author##';"

   query.eventWithStationCount.description = "Get events by preferred origin author etc"
   query.eventWithStationCount = "SELECT PEvent.publicID, Origin.time_value AS OT, Origin.latitude_value AS lat, Origin.longitude_value AS lon, Origin.depth_value AS dep, Magnitude.magnitude_value AS mag, Magnitude.type AS mtype, Origin.quality_usedStationCount AS stations, Event.type AS type, Event.typeCertainty AS certainty, Origin.creationInfo_author FROM Origin, PublicObject AS POrigin, Event, PublicObject AS PEvent, Magnitude, PublicObject AS PMagnitude WHERE Event._oid = PEvent._oid AND Origin._oid = POrigin._oid AND Magnitude._oid = PMagnitude._oid AND PMagnitude.publicID = Event.preferredMagnitudeID AND POrigin.publicID = Event.preferredOriginID AND Origin.time_value >= '##startTime##' AND Origin.time_value <= '##endTime##';"

   query.eventType.description = "Returns all eventIDs FROM event WHERE the type is flagged AS 'event type'"
   query.eventType = "SELECT pe.publicID, o.time_value AS OT FROM PublicObject pe, PublicObject po, Event e, Origin o WHERE pe._oid = e._oid AND po._oid = o._oid AND e.preferredOriginID = po.publicID AND e.type = '##type##' AND o.time_value >= '##startTime##' AND o.time_value <= '##endTime##';"

   query.originByAuthor.description = "Get origins by author"
   query.originByAuthor = "SELECT po.publicID, o.time_value AS OT, o.creationInfo_author FROM PublicObject po JOIN Origin o ON po._oid = o._oid WHERE o.creationInfo_author like '##author##' AND o.time_value >= '##startTime##' AND o.time_value <= '##endTime##';"

**More examples and statistics**

.. code-block:: properties

   queries = phaseCountPerAuthor, time, mag_time, space_time, all, space_mag_time, event, fm_space_time, picks, stationPicks, assoc_picks, pref_assoc_picks, sta_net_mag, sta_net_mag_type, delta_sta_net_mag, delta_sta_net_mag_type

   query.phaseCountPerAuthor.description = "Get phase count per origin author FROM event #EventID#"
   query.phaseCountPerAuthor = "SELECT PEvent.publicID, Origin.creationInfo_author, MAX(Origin.quality_usedPhaseCount) FROM Origin, PublicObject AS POrigin, Event, PublicObject AS PEvent, OriginReference WHERE Origin._oid = POrigin._oid AND Event._oid = PEvent._oid AND OriginReference._parent_oid = Event._oid AND OriginReference.originID = POrigin.publicID AND PEvent.publicID = '##EventID##' group by Origin.creationInfo_author;"

   query.time.description = "Events in time range"
   query.time = "SELECT PEvent.publicID, Origin.time_value, ROUND(Origin.latitude_value, 4), ROUND(Origin.longitude_value, 4), ROUND(Origin.depth_value, 1), ROUND(Magnitude.magnitude_value, 1), Magnitude.type, Origin.quality_usedPhaseCount, Origin.quality_usedStationCount, Event.typeCertainty, Event.type, Origin.creationInfo_author FROM Origin, PublicObject AS POrigin, Event, PublicObject AS PEvent, Magnitude, PublicObject AS PMagnitude WHERE Event._oid = PEvent._oid AND Origin._oid = POrigin._oid AND Magnitude._oid = PMagnitude._oid AND PMagnitude.publicID = Event.preferredMagnitudeID AND POrigin.publicID = Event.preferredOriginID AND Origin.time_value >= '##startTime##' AND Origin.time_value <= '##endTime##';"

   query.mag_time.description = "Events in magnitude-time range"
   query.mag_time = "SELECT PEvent.publicID, Origin.time_value, ROUND(Origin.latitude_value, 4), ROUND(Origin.longitude_value, 4), ROUND(Origin.depth_value, 1), ROUND(Magnitude.magnitude_value, 1), Magnitude.type, Origin.quality_usedPhaseCount, Origin.quality_usedStationCount, Event.typeCertainty, Event.type, Origin.creationInfo_author FROM Origin, PublicObject AS POrigin, Event, PublicObject AS PEvent, Magnitude, PublicObject AS PMagnitude WHERE Event._oid = PEvent._oid AND Origin._oid = POrigin._oid AND Magnitude._oid = PMagnitude._oid AND PMagnitude.publicID = Event.preferredMagnitudeID AND POrigin.publicID = Event.preferredOriginID AND Magnitude.magnitude_value >= ##minMag## AND Magnitude.magnitude_value <= ##maxMag## AND Origin.time_value >= '##startTime##' AND Origin.time_value <= '##endTime##';"

   query.space_time.description = "Events in space-time range"
   query.space_time = "SELECT PEvent.publicID, Origin.time_value, ROUND(Origin.latitude_value, 4), ROUND(Origin.longitude_value, 4), ROUND(Origin.depth_value, 1), ROUND(Magnitude.magnitude_value, 1), Magnitude.type, Origin.quality_usedPhaseCount, Origin.quality_usedStationCount, Event.typeCertainty, Event.type, Origin.creationInfo_author FROM Origin, PublicObject AS POrigin, Event, PublicObject AS PEvent, Magnitude, PublicObject AS PMagnitude WHERE Event._oid = PEvent._oid AND Origin._oid = POrigin._oid AND Magnitude._oid = PMagnitude._oid AND PMagnitude.publicID = Event.preferredMagnitudeID AND POrigin.publicID = Event.preferredOriginID AND Origin.latitude_value >= ##latMin## AND Origin.latitude_value <= ##latMax## AND Origin.longitude_value >= ##lonMin## AND Origin.longitude_value <= ##lonMax## AND Origin.time_value >= '##startTime##' AND Origin.time_value <= '##endTime##';"

   query.all.description = "Events in space-magnitude-time-quality range by author"
   query.all = "SELECT PEvent.publicID, Origin.time_value, ROUND(Origin.latitude_value, 4), ROUND(Origin.longitude_value, 4), ROUND(Origin.depth_value, 1), ROUND(Magnitude.magnitude_value, 1), Magnitude.type, Origin.quality_usedPhaseCount, Origin.quality_usedStationCount, Event.typeCertainty, Event.type, Origin.creationInfo_author FROM Origin, PublicObject AS POrigin, Event, PublicObject AS PEvent, Magnitude, PublicObject AS PMagnitude WHERE Event._oid = PEvent._oid AND Origin._oid = POrigin._oid AND Magnitude._oid = PMagnitude._oid AND PMagnitude.publicID = Event.preferredMagnitudeID AND POrigin.publicID = Event.preferredOriginID AND Origin.latitude_value >= ##latMin## AND Origin.latitude_value <= ##latMax## AND Origin.longitude_value >= ##lonMin## AND Origin.longitude_value <= ##lonMax## AND Origin.quality_usedPhaseCount >= ##minPhases## AND Origin.quality_usedPhaseCount <= ##maxPhases## AND Magnitude.magnitude_value >= ##minMag## AND Magnitude.magnitude_value <= ##maxMag## AND Origin.time_value >= '##startTime##' AND Origin.time_value <= '##endTime##' AND Origin.creationInfo_author like '##author##%';"

   query.space_mag_time.description = "Events in space-magnitude-time range"
   query.space_mag_time = "SELECT PEvent.publicID, Origin.time_value, ROUND(Origin.latitude_value, 4), ROUND(Origin.longitude_value, 4), ROUND(Origin.depth_value, 1), ROUND(Magnitude.magnitude_value, 1), Magnitude.type, Origin.quality_usedPhaseCount, Origin.quality_usedStationCount, Event.typeCertainty, Event.type, Origin.creationInfo_author FROM Origin, PublicObject AS POrigin, Event, PublicObject AS PEvent, Magnitude, PublicObject AS PMagnitude WHERE Event._oid = PEvent._oid AND Origin._oid = POrigin._oid AND Magnitude._oid = PMagnitude._oid AND PMagnitude.publicID = Event.preferredMagnitudeID AND POrigin.publicID = Event.preferredOriginID AND Origin.latitude_value >= ##latMin## AND Origin.latitude_value <= ##latMax## AND Origin.longitude_value >= ##lonMin## AND Origin.longitude_value <= ##lonMax## AND Magnitude.magnitude_value >= ##minMag## AND Magnitude.magnitude_value <= ##maxMag## AND Origin.time_value >= '##startTime##' AND Origin.time_value <= '##endTime##';"

   query.fm_space_time.description = "Events with focal mechanisms in space-time range"
   query.fm_space_time = "SELECT PEvent.publicID, Origin.time_value, ROUND(Origin.latitude_value, 4), ROUND(Origin.longitude_value, 4), ROUND(Origin.depth_value, 1), ROUND(Magnitude.magnitude_value, 1), Magnitude.type, MomentTensor.doubleCouple, MomentTensor.variance, Event.typeCertainty, Event.type, Origin.creationInfo_author FROM Origin, PublicObject AS POrigin, Event, PublicObject AS PEvent, Magnitude, PublicObject AS PMagnitude, FocalMechanism, PublicObject AS PFocalMechanism, MomentTensor WHERE Event._oid = PEvent._oid AND Origin._oid = POrigin._oid AND Magnitude._oid = PMagnitude._oid AND PMagnitude.publicID = Event.preferredMagnitudeID AND FocalMechanism._oid = PFocalMechanism._oid AND PFocalMechanism.publicID = Event.preferredFocalMechanismID AND MomentTensor._parent_oid = FocalMechanism._oid AND POrigin.publicID = Event.preferredOriginID AND Origin.latitude_value >= ##latMin## AND Origin.latitude_value <= ##latMax## AND Origin.longitude_value >= ##lonMin## AND Origin.longitude_value <= ##lonMax## AND Origin.time_value >= '##startTime##' AND Origin.time_value <= '##endTime##';"

   query.event.description = "List authors and number of origins for event"
   query.event = "SELECT PEvent.publicID, Origin.creationInfo_author, MAX(Origin.quality_usedPhaseCount) FROM Origin, PublicObject AS POrigin, Event, PublicObject AS PEvent, OriginReference WHERE Origin._oid = POrigin._oid AND Event._oid = PEvent._oid AND OriginReference._parent_oid = Event._oid AND OriginReference.originID = POrigin.publicID AND PEvent.publicID = '##EventID##' group by Origin.creationInfo_author;"

   query.picks.description = "List number of picks per station in a certain timespan"
   query.picks = "SELECT waveformID_networkCode AS Network, waveformID_stationCode AS Station, COUNT(_oid) AS Picks, MIN(time_value) AS Start, MAX(time_value) AS End FROM Pick WHERE time_value >= '##startTime##' AND time_value <= '##endTime##' GROUP BY waveformID_networkCode, waveformID_stationCode;"

   query.stationPicks.description = "List the picks and phase hints per station in a certain timespan"
   query.stationPicks = "SELECT PPick.publicID, Pick.phaseHint_code FROM Pick, PublicObject AS PPick WHERE Pick._oid = PPick._oid AND waveformID_networkCode = '##netCode##' AND waveformID_stationCode = '##staCode##' AND time_value >= '##startTime##' AND time_value <= '##endTime##';"

   query.assoc_picks.description = "List number of associated picks per station in a certain time span"
   query.assoc_picks = "SELECT Pick.waveformID_networkCode AS Network, Pick.waveformID_stationCode AS Station, COUNT(DISTINCT(Pick._oid)) AS Picks, MIN(Pick.time_value) AS Start, MAX(Pick.time_value) AS End FROM Pick, PublicObject PPick, Arrival WHERE Pick._oid = PPick._oid AND PPick.publicID = Arrival.pickID AND Pick.time_value >= '##startTime##' AND Pick.time_value <= '##endTime##' GROUP BY Pick.waveformID_networkCode, Pick.waveformID_stationCode;"

   query.pref_assoc_picks.description = "List number of associated picks of preferred origins per station for certain time span"
   query.pref_assoc_picks = "SELECT Pick.waveformID_networkCode AS Network, Pick.waveformID_stationCode AS Station, COUNT(DISTINCT(Pick._oid)) AS Picks, MIN(Pick.time_value) AS Start, MAX(Pick.time_value) AS End FROM Pick, PublicObject PPick, Arrival, Origin, PublicObject POrigin, Event WHERE Event.preferredOriginID = POrigin.publicID AND Origin._oid = POrigin._oid AND Origin._oid = Arrival._parent_oid AND Pick._oid = PPick._oid AND PPick.publicID = Arrival.pickID AND Pick.time_value >= '##startTime##' AND Pick.time_value <= '##endTime##' GROUP BY Pick.waveformID_networkCode, Pick.waveformID_stationCode;"

   query.sta_net_mag.description = "Compares station magnitudes of a particular station with the network magnitude in a certain time span"
   query.sta_net_mag = "SELECT StationMagnitude.waveformID_networkCode AS Network, StationMagnitude.waveformID_stationCode AS Station, StationMagnitude.magnitude_value AS StaMag, Magnitude.magnitude_value AS NetMag, Magnitude.type AS NetMagType, StationMagnitude.creationInfo_creationTime AS CreationTime FROM StationMagnitude, PublicObject PStationMagnitude, StationMagnitudeContribution, Magnitude WHERE StationMagnitude._oid = PStationMagnitude._oid AND StationMagnitudeContribution.stationMagnitudeID = PStationMagnitude.publicID AND StationMagnitudeContribution._parent_oid = Magnitude._oid AND StationMagnitude.waveformID_networkCode = '##netCode##' AND StationMagnitude.waveformID_stationCode = '##staCode##' AND StationMagnitude.creationInfo_creationTime >= '##startTime##' AND StationMagnitude.creationInfo_creationTime <= '##endTime##' ORDER BY StationMagnitude.creationInfo_creationTime;"

   query.sta_net_mag_type.description = "Compares station magnitudes of a particular station with the network magnitude of specific type in a certain time span"
   query.sta_net_mag_type = "SELECT StationMagnitude.waveformID_networkCode AS Network, StationMagnitude.waveformID_stationCode AS Station, StationMagnitude.magnitude_value AS StaMag, Magnitude.magnitude_value AS NetMag, Magnitude.type AS NetMagType, StationMagnitude.creationInfo_creationTime AS CreationTime FROM StationMagnitude, PublicObject PStationMagnitude, StationMagnitudeContribution, Magnitude WHERE StationMagnitude._oid = PStationMagnitude._oid AND StationMagnitudeContribution.stationMagnitudeID = PStationMagnitude.publicID AND StationMagnitudeContribution._parent_oid = Magnitude._oid AND StationMagnitude.waveformID_networkCode = '##netCode##' AND StationMagnitude.waveformID_stationCode = '##staCode##' AND StationMagnitude.creationInfo_creationTime >= '##startTime##' AND StationMagnitude.creationInfo_creationTime <= '##endTime##' AND Magnitude.type = '##magType##' ORDER BY StationMagnitude.creationInfo_creationTime;"

   query.delta_sta_net_mag.description = "Calculates delta values of station and network magnitudes for all stations in a certain time span"
   query.delta_sta_net_mag = "SELECT StationMagnitude.waveformID_networkCode AS Network, StationMagnitude.waveformID_stationCode AS Station, AVG(StationMagnitude.magnitude_value - Magnitude.magnitude_value) AS DeltaAvg, MIN(StationMagnitude.magnitude_value - Magnitude.magnitude_value) AS DeltaMin, MAX(StationMagnitude.magnitude_value - Magnitude.magnitude_value) AS DeltaMax, MIN(StationMagnitude.creationInfo_creationTime) AS Start, MAX(StationMagnitude.creationInfo_creationTime) AS End FROM StationMagnitude, PublicObject PStationMagnitude, StationMagnitudeContribution, Magnitude WHERE StationMagnitude._oid = PStationMagnitude._oidStationMagnitudeContribution.stationMagnitudeID = PStationMagnitude.publicIDStationMagnitudeContribution._parent_oid = Magnitude._oidStationMagnitude.creationInfo_creationTime >= '##startTime##'StationMagnitude.creationInfo_creationTime <= '##endTime##' GROUP BY StationMagnitude.waveformID_networkCode, StationMagnitude.waveformID_stationCode;"

   query.delta_sta_net_mag_type.description = "Calculates delta values of station and network magnitudes for all stations and all magnitude types in a certain time span"
   query.delta_sta_net_mag_type = "SELECT StationMagnitude.waveformID_networkCode AS Network, StationMagnitude.waveformID_stationCode AS Station, AVG(StationMagnitude.magnitude_value - Magnitude.magnitude_value) AS DeltaAvg, MIN(StationMagnitude.magnitude_value - Magnitude.magnitude_value) AS DeltaMin, MAX(StationMagnitude.magnitude_value - Magnitude.magnitude_value) AS DeltaMax, Magnitude.type AS NetMagType, MIN(StationMagnitude.creationInfo_creationTime) AS Start, MAX(StationMagnitude.creationInfo_creationTime) AS End FROM StationMagnitude, PublicObject PStationMagnitude, StationMagnitudeContribution, Magnitude WHERE StationMagnitude._oid = PStationMagnitude._oid AND StationMagnitudeContribution.stationMagnitudeID = PStationMagnitude.publicID AND StationMagnitudeContribution._parent_oid = Magnitude._oid AND StationMagnitude.creationInfo_creationTime >= '##startTime##' AND StationMagnitude.creationInfo_creationTime <= '##endTime##' GROUP BY StationMagnitude.waveformID_networkCode, StationMagnitude.waveformID_stationCode, Magnitude.type;"


.. _scquery_psql:

PostgreSQL
----------

In contrast to queries for objects in :ref:`MariaDB/MySQL <scquery_mariadb>` the
string ``m_`` must be added to the value and publicID database columns as shown
below for the query "eventFilter".

.. code-block:: properties

   queries = eventFilter

   query.eventFilter.description = "Returns all events (lat, lon, mag, time) that fall into a certain region and a magnitude range"
   query.eventFilter = "SELECT PEvent.m_publicID, Origin.m_time_value AS OT, Origin.m_latitude_value, Origin.m_longitude_value, Origin.m_depth_value, Magnitude.m_magnitude_value, Magnitude.m_type FROM Origin, PublicObject AS POrigin, Event, PublicObject AS PEvent, Magnitude, PublicObject AS PMagnitude WHERE Event._oid = PEvent._oid AND Origin._oid = POrigin._oid AND Magnitude._oid = PMagnitude._oid AND PMagnitude.m_publicID = Event.m_preferredMagnitudeID AND POrigin.m_publicID = Event.m_preferredOriginID AND Origin.m_latitude_value >= ##latMin## AND Origin.m_latitude_value <= ##latMax## AND Origin.m_longitude_value >= ##lonMin## AND Origin.m_longitude_value <= ##lonMax## AND Magnitude.m_magnitude_value >= ##minMag## AND Magnitude.m_magnitude_value <= ##maxMag## AND Origin.m_time_value >= '##startTime##' AND Origin.m_time_value <= '##endTime##';"
