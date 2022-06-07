scinv merges inventory XML files to a single inventory, synchronises an inventory
with another (most common use is with database), creates initial key files and
much more ...

scinv is used by :file:`etc/init/scinv.py` to synchronise the inventory from
:file:`etc/inventory` with the database.

.. code-block:: sh

   seiscomp update-config inventory


Commands
========

scinv works with different commands:

- :ref:`scinv_apply`: Read and apply notifiers,
- :ref:`scinv_check`: Check the consistency of inventories,
- :ref:`scinv_keys`: Merge inventory files and generate key files,
- :ref:`scinv_ls`: List the content of inventories,
- :ref:`scinv_merge`: Merge inventory files,
- :ref:`scinv_sync`: Synchronize inventories in files and writing sending to the
  messaging for saving to the database.



The command **must** be given as **1st**
parameter to the application. All others parameters must follow.

.. code-block:: sh

   scinv $command [options] [files]


.. _scinv_sync:

sync
----

Synchronises an applications inventory with a given source given as file(s).
The applications inventory is either read from the database or given with
*--inventory-db*. As a result all information in the source is written to target
and target does not contain any additional information. The source must hold all
information. This works different to merge. If an output file is specified with
*-o* no notifiers are generated and sent via messaging.

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

Merges two or more inventories into one inventory. This command
is useful to merge existing subtrees into a final inventory before
synchronization.

.. code-block:: sh

   scinv merge net1.xml net2.xml -o inv.xml

.. _note ::

    Merging inventory XML files is also supported by :ref:`scxmlmerge`.


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
corresponding station key file in :file:`etc/key` exists. If not an empty
station key file is created. If a station key file without a corresponding
station in the merged inventory is found, it is deleted.


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
*sta*, *chan* and *resp*. The output level is controlled by ``--level``.

For checking the available networks and stations in the inventory pool, calling

.. code-block:: sh

   scinv ls --level sta

is enough.


.. _scinv_check:

check
-----

Checks consistency of passed inventory files or a complete filebase. In the
first step the inventory is merged from all files. In the second step several
consistency checks are applied such as:

- Overlapping epochs on each level (network, station, ...),
- Valid epochs (start < end),
- Defined gain in a stream,
- Set gainUnit,
- Distance of the sensor location to the station location,
- "Invalid" location 0/0.

When inconsistencies are found, alerts are printed:

- **!**: Error, user should take an action,
- **C**: Conflict, user should take an action,
- **W**: Warning, user should check if an action is required,
- **I**: Information,
- **D**: Debug,
- **R**: Unresolvable,
- **?**: Question.

.. csv-table::
   :widths: 10, 30, 5, 65
   :header: Object, Check description, Alert, Comments
   :align: left

   network       , start time after end time        , !,
                 , network without station          , W,
   sensorLocation, coordinates far away from station, W, ``--distance`` overrides defaults threshold (10 km)


In future further checks will be added to make this tool a real help for
correct meta data creation.
