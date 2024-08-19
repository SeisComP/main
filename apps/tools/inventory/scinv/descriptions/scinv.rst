scinv merges and tests inventory XML files to a single inventory, synchronises
an inventory with another (most common use is with database), creates initial
key files and much more ...

scinv is used by :file:`$SEISCOMP_ROOT/etc/init/scinv.py` to synchronise the
inventory from :file:`$SEISCOMP_ROOT/etc/inventory` with the database.

.. code-block:: sh

   seiscomp update-config inventory

.. hint::

   Inventory files in :term:`SCML` format may be generated or modified by
   :cite:t:`smp` or :ref:`invextr`. For conversion from FDSN station XML and
   dataless SEED volume to :term:`SCML` use :ref:`fdsnxml2inv` and
   :ref:`dlsv2inv`, respectively.


Commands
========

scinv works with different commands:

- :ref:`scinv_ls`: List the content of inventories in XML files,
- :ref:`scinv_check`: Merge and test inventories, check the completeness and
  consistency of parameters, report any issue,
- :ref:`scinv_merge`: Merge and test inventory files,
- :ref:`scinv_keys`: Merge and test inventories, generate key files or
  remove key files without coorresponding inventory,
- :ref:`scinv_sync`: Merge and test inventory files, generate or remove key
  files, synchronise the inventory with the database and send updates by
  notifiers to the messaging for saving to the database,
- :ref:`scinv_apply`: Read and apply notifiers.

The command **must** be given as **1st**
parameter to the application. All others parameters must follow.

.. code-block:: sh

   scinv $command [options] [files]


.. _scinv_sync:

sync
----

Synchronises an applications inventory with a given source given as file(s).
It checks the consistency of the inventory using :ref:`scinv_check` before
synchronization.
The applications inventory is either read from the database or given with
:option:`--inventory-db`. As a result all information in the source is written
to target and target does not contain any additional information. The source
must hold all information. This works different to merge. If an output file is
specified with :option:`-o` no notifiers are generated and sent via messaging.

This command is used by :file:`etc/init/scinv.py` as follows:

.. code-block:: sh

   scinv sync --console=1 -H localhost:$p --filebase "$fb" \
              --rc-dir "$rc" --key-dir "$kd"

where

.. code-block:: sh

   $p = configured messaging port
   $fb = $SEISCOMP_ROOT/etc/inventory
   $rc = $SEISCOMP_ROOT/var/lib/rc
   $kd = $SEISCOMP_ROOT/etc/key


.. _scinv_merge:

merge
-----

Merges two or more inventories into one inventory checking the consistency
of the inventory by using :ref:`scinv_check`. This command is useful to merge
existing subtrees into a final inventory before synchronization.

.. code-block:: sh

   scinv merge net1.xml net2.xml -o inv.xml

.. note::

   Merging inventory XML files is also supported by :ref:`scxmlmerge` but
   without the full :ref:`consistency checks <scinv_check>`.


.. _scinv_apply:

apply
-----

Applies stored notifiers created with **sync** and option ``--create-notifier``
which is saved in a file (``-o``). Source is the applications inventory read
from the database or given with ``--inventory-db``.
If ``-o`` is passed, no messages are sent but the result is stored in a file.
Useful to test/debug or prepare an inventory for offline processing.


.. code-block:: sh

   # Synchronise inventory and save the notifiers locally. No messages are sent.
   scinv sync -d mysql://sysop:sysop@localhost/seiscomp \
         --create-notifier -o sync_patch.xml

   # Sent the notifiers to the target system
   scinv apply -H localhost sync_patch.xml

This operation can be useful to save differences in synchronization for
validation or debugging problems.


.. _scinv_keys:

keys
----

Synchronise station key files with current inventory pool. This command merges
all XML files in the inventory pool (or the given files) and checks if a
corresponding station key file in :file:`$SEISCOMP_ROOT/etc/key` exists. If not,
an empty station key file is created. If a station key file without a
corresponding station in the merged inventory is found, it is deleted.


.. _scinv_ls:

ls
--

List contained items up to response level. This command is useful to inspect
an XML file or the complete inventory pool.

.. code-block:: sh

   $ scinv ls SK.KOLS.xml

     network SK       Slovak National Network of Seismic Stations
       epoch 1980-01-01
       station KOLS   Kolonicke sedlo, Slovakia
         epoch 2004-09-01
         location __
           epoch 2004-09-01
           channel BHE
             epoch 2006-04-25 12:00:00 - 2010-03-24
           channel BHN
             epoch 2006-04-25 12:00:00 - 2010-03-24
           channel BHZ
             epoch 2006-04-25 12:00:00 - 2010-03-24
           channel EHE
             epoch 2004-09-01 - 2006-04-25 10:00:00
           channel EHN
             epoch 2004-09-01 - 2006-04-25 10:00:00
           channel EHZ
             epoch 2004-09-01 - 2006-04-25 10:00:00
           channel HHE
             epoch 2006-04-25 12:00:00 - 2010-03-24
           channel HHE
             epoch 2010-03-25
           channel HHN
             epoch 2006-04-25 12:00:00 - 2010-03-24
           channel HHN
             epoch 2010-03-25
           channel HHZ
             epoch 2006-04-25 12:00:00 - 2010-03-24
           channel HHZ
             epoch 2010-03-25

The default level of information printed is *chan*. Available levels are *net*,
*sta*, *chan* and *resp*. The output level is controlled by :option:`--level`.

For checking the available networks and stations in the inventory pool, calling

.. code-block:: sh

   scinv ls --level sta

is enough.

.. hint::

   Stream lists in NSLC format (NET.STA.LOC.CHA) may be generated when combining
   with :option:`--nslc`. Such lists can be used as input for filtering
   waveforms, e.g., to :ref:`scmssort` or :ref:`scart`.

   .. code-block:: sh

      $ scinv ls --nslc inventory.xml

        IU.WVT.00.BHZ 2017-11-16
        IU.XMAS.00.BH1 2018-07-06 20:00:00


.. _scinv_check:

check
-----

Checks consistency of passed inventory files or a complete filebase. In the
first step the inventory is merged from all files. In the second step several
consistency checks are applied such as:

- Overlapping epochs on each level (network, station, ...),
- Valid epochs (start < end),
- Defined gain in a stream,
- Set gain unit,
- Distance of the sensor location to the station location,
- "Invalid" location 0/0.

When inconsistencies or other relevant information are found, alerts are printed:

- **!**: Error, user must take an action,
- **C**: Conflict, user should take an action,
- **W**: Warning, user should check if an action is required,
- **I**: Information,
- **D**: Debug,
- **R**: Unresolvable, user should check if an action is required,
- **?**: Question.

.. note::

   * Default test tolerances are adopted from typical values for global
     networks. Consider adjusting :confval:`check.maxDistance`,
     :confval:`check.maxElevationDifference` and :confval:`check.maxSensorDepth`
     by configuration or command-line options.
   * Errors must but conflicts and warnings should be resolved for maintaining a
     correct inventory.
   * :ref:`Merging <scinv_merge>` and :ref:`sychronization <scinv_sync>` stop
     when finding errors.

The following table lists checks of objects for deficiencies and the test
results.

* This test matrix may be incomplete. Consider adding more tests and results.
* Please report inventory issues not caught by tests to the SeisComP
  development team, e.g. on :cite:t:`seiscomp-github`.

.. csv-table::
   :widths: 10, 30, 5, 65
   :header: Object, Check description, Alert, Comments
   :align: left
   :delim: ;

   network       ; start time after end time        ; !;
                 ; network without station          ; W;
                 ; empty start time                 ;  ; handled by SeisComP inventory reader: network is ignored
                 ; empty station                    ; W;
                 ; empty code                       ; W;

   station       ; start time after end time        ; !;
                 ; empty or no start time           ; W; station is ignored
                 ; start time after end time        ; !;
                 ; empty code                       ; W;
                 ; empty latitude                   ; W;
                 ; empty longitude                  ; W;
                 ; empty elevation                  ; W;
                 ; elevation >   8900               ; !;
                 ; elevation < -12000               ; !;
                 ; has no sensor location           ; W;

   sensorLocation; coordinates far away from station; W; :option:`--distance` and :confval:`check.maxDistance` override default threshold (10 km)
                 ; elevation far away from station  ; W; :option:`--max-elevation-difference` and :confval:`check.maxElevationDifference` override default threshold (500 m)
                 ; epoch outside network epochs     ; C;
                 ; epoch outside station epochs     ; C;
                 ; empty or no start time           ; W; sensorLocation is ignored
                 ; empty latitude                   ; W;
                 ; empty longitude                  ; W;
                 ; elevation >   8900               ; !;
                 ; elevation < -12000               ; !;
                 ; empty or no elevation            ; W;
                 ; has no channel/stream            ; W;

   stream        ; empty or no start time           ;  ; handled by SeisComP inventory reader: stream is ignored
                 ; empty azimuth                    ; C;
                 ; epoch outside sensorLocation     ; C;
                 ; epoch outside station            ; C;
                 ; epoch outside network            ; C;
                 ; start time after end time        ; C;
                 ; missing gain value               ; W; empty value is handled by SeisComP inventory reader
                 ; gain value = 0                   ; W;
                 ; gain < 0 and dip > 0             ; W; may result in unexpected behavior, consider positive gain and negative dip
                 ; missing gain unit                ; W; empty value is handled by SeisComP inventory reader
                 ; missing gain frequency           ;  ; empty value is handled by SeisComP inventory reader
                 ; missing sampling rate            ;  ; empty value is handled by SeisComP inventory reader
                 ; missing depth                    ; W; empty value is handled by SeisComP inventory reader
                 ; missing azimuth                  ; W;
                 ; missing dip                      ; W;
                 ; empty azimuth                    ;  ; handled by SeisComP inventory reader
                 ; empty dip                        ;  ; handled by SeisComP inventory reader
                 ; large depth                      ; W; :option:`--max-sensor-depth` and :confval:`check.maxSensorDepth` override default threshold (500 m)
                 ; empty sensor ID                  ; I;
                 ; sensor is unavailable            ; R;
                 ; empty data logger ID             ; I;
                 ; data logger is unavailable       ; R;
                 ; 2 or more than 3 streams exist   ; I;
                 ; 3C streams are not orthogonal    ; W; differences <= 5 degree are tolerated, applies to seismic sensors with codes G, H, L, N

   sensor        ; referenced response not available; R;

   data logger   ; referenced response not available; R;
