This module executes custom scripts upon arrival of objects or updates.
It provides as template for custom modification and is not a replacement for :ref:`scvoice`.

There are four possible trigger mechanisms for calling scripts:

* Event creation/update,
* Amplitude creation,
* Origin creation (with status = preliminary),
* Pick creation with filter for phase hint.

.. note ::

   People started modifying :ref:`scvoice` to send emails or
   other alert messages. Then, the name *scvoice* is then just wrong.
   If you want to customize :ref:`scvoice`, use scalert instead.
