scstation enables or disables stations from the command line. The state is the
same as shown in the *Enabled* and *Disabled* tabs of :ref:`scrttv`, and it is
also used by :ref:`scolv`, :ref:`scmv`, :ref:`scqcv` and other modules.

scstation sets the *enabled* flag of the station configuration (*ConfigStation*)
in the config module, which is *trunk* by default (:option:`--config-module`).
If a station has no station configuration yet, one is created, as
:ref:`scrttv` does. The change is sent as notifiers to the *CONFIG* messaging
group. :ref:`scmaster` stores it in the database, and running modules apply it
immediately. scstation never writes to the database directly.

The enabled state of existing station configurations is kept when running
:program:`seiscomp update-config`. Stations without a station configuration are
considered enabled.


Commands
========

* **disable** NET.STA [NET.STA ...]: Disable the given stations.
* **enable** NET.STA [NET.STA ...]: Enable the given stations.
* **list** [enabled|disabled] [NET.STA ...]: List the stations of the config
  module and their state, optionally filtered by state or station ID.

Station IDs may contain wildcards (``*``, ``?``, ``[...]``). Wildcards only
match stations which already have a station configuration in the config
module. Station IDs can also be read from file using :option:`-i`.

.. note::

   ``enable`` and ``disable`` require a running :ref:`scmaster`.
   ``list`` and :option:`--dry-run` only read from the database, so
   messaging is not used when a database is given with :option:`-d`.


Examples
========

* Disable two stations: ::

     scstation -H localhost disable GE.APE GE.MORC

* Enable all stations of a network which have a station configuration: ::

     scstation -H localhost enable 'GE.*'

* List all disabled stations: ::

     scstation -H localhost list disabled

* List the state of all stations reading directly from the database: ::

     scstation -d mysql://sysop:sysop@localhost/seiscomp list

* Show what would be disabled for a list of stations read from file, without
  sending anything: ::

     scstation -H localhost --dry-run disable -i stations.txt
