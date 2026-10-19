scproclat listens to the messaging and logs the time when picks, amplitudes,
origins, magnitudes and events are received. For picks, amplitudes and origins
it also logs the latency, the difference between the reception time and the
time of the object, e.g. the pick time. The latencies show the delay of the
entire processing chain from the data acquisition to the messaging, e.g. to
monitor the performance of :ref:`scmaster` or to find stations whose picks
arrive late.

The latencies are written to stdout and to log files in the storage directory
configured by :confval:`directory` or the command-line option ``-o``.
scproclat does not connect to the database.


Log files
=========

The log files are stored in sub-directories named by year, month and day,
e.g. :file:`2026/10/19/`. Each entry is written to two files:

* :file:`*.rcv`: The entry is stored in the directory of the day of the
  reception. The time of the entry is the reception time.
* :file:`*.trg`: The entry is stored in the directory of the day of the
  object time, e.g. the pick time. The time of the entry is the object time.
  Magnitudes, origin references and events do not have an object time and are
  not written to these files.

Picks and amplitudes are written per stream to
:file:`NET.STA.LOC.CHA.rcv` and :file:`NET.STA.LOC.CHA.trg`. Origins,
magnitudes, origin references and events are written to :file:`objects.rcv`
and :file:`objects.trg`.

The times are given as time of the day, e.g. ``12:41:43.282679``. The date is
given by the directory. Latencies are given as
``[-]DD:HH:MM:SS.ffffff`` (days, hours, minutes, seconds and microseconds).


Output format
=============

Each entry is a line with fields separated by semicolons. Entries of picks and
amplitudes start with:

.. code-block:: none

   time;latency;messageLatency;mode;...

Entries of origins, magnitudes, origin references and events start with:

.. code-block:: none

   time;latency;mode;...

with

* **time**: The reception time, or the object time in :file:`*.trg` files.
* **latency**: The reception time minus the object time. Empty for magnitudes,
  origin references and events.
* **messageLatency**: The reception time minus the creation time of the
  object. Only set if :confval:`logMsgLatency` is active.
* **mode**: ``A`` for a new object, ``U`` for an updated object.

The following fields depend on the object type:

.. csv-table::
   :header: "Object", "Fields", "Object time"
   :widths: 15 55 30

   "Pick", "``publicID;P;phaseHint``", "Pick time"
   "Amplitude", "``publicID;A;type;value``", "Reference time of the time window"
   "Origin", "``publicID;O;evaluationStatus;latitude;longitude;depth``, depth in km", "Origin time"
   "Magnitude", "``publicID;M;type;value;stationCount``", "-"
   "Origin reference", "``eventID;OR;originID``", "-"
   "Event", "``publicID;E;preferredOriginID;preferredMagnitudeID``", "-"

On stdout, the entries of picks and amplitudes additionally end with the stream
ID, ``NET.STA.LOC.CHA``. In the log files the stream ID is given by the file
name.


Examples
========

Show the latencies of all objects while they arrive and store the log files in
:file:`/tmp/latencies`:

.. code-block:: sh

   scproclat -H localhost -o /tmp/latencies

Example output of a pick which arrived two hours after the pick time:

.. code-block:: none

   12:41:43.441033;00:02:00:00.500239;;A;Pick/20261019124143.440859.1;P;P;AU.ARMA.00.BHZ

Find the picks and amplitudes which were received more than one hour after
their object time on a day, together with the file name containing the stream
ID:

.. code-block:: sh

   cd /tmp/latencies
   awk -F';' '$2 >= "00:01:00:00" {print FILENAME ": " $0}' 2026/10/19/*.*.*.*.rcv
