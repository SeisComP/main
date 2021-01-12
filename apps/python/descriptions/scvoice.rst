This tool runs an external script whenever an event has been created or updated.
It can also run a script in case an amplitude of a particular type or a
preliminary origin (heads-up message) has been sent. The common purpose for
this tool is to play a sound or to convert a message to speech using external
tools like festival or espeak.
There are three possible trigger mechanisms for calling scripts:

* Event creation/update
* Amplitude creation
* Origin creation (with status = preliminary)

Although this tool was designed to alert the user acoustically it can also be
used to send e-mails, sms or to do any other kind of alert. scvoice can only
run one script per call type at a time! A template (:ref:`scalert`) Python script with
more options has been added to |scname| to be used as source for custom notifications.


Examples
========


Event script
------------

The following script is used as event script. It requires
`festival <http://www.cstr.ed.ac.uk/projects/festival/>`_ which should be
available in almost any Linux distribution.

.. important::
   When saving the scripts given below do not forget to set the executable
   bit otherwise scvoice cannot call the scripts. In Linux just run:

   .. code-block:: sh

      chmod +x /path/to/file


#. Save an executable script file, e.g., under, e.g. :file:`~/.seiscomp/event.sh`:

   .. code-block:: sh

      #!/bin/sh
      if [ "$2" = "1" ]; then
      echo " $1" | sed 's/,/, ,/g'   | festival --tts;
      else
      echo "Event updated, $1" | sed 's/,/, ,/g'   | festival --tts;
      fi

#. Add the file to the configuration of :confval:`scripts.event` in the file
   :file:`SEISCOMP_ROOT/etc/scvoice.cfg` or :file:`~/.seiscomp/scvoice.cfg`:

   .. code-block:: sh

      scripts.event = /home/sysop/.seiscomp/event.sh

Amplitude script
----------------

#. Save an executable script file, e.g., under :file:`~/.seiscomp/amplitude.sh`

   .. code-block:: sh

      #!/bin/sh
      # Play a wav file with a particular volume
      # derived from the amplitude itself.
      playwave ~/.seiscomp/beep.wav -v $3

#. Add the file to the configuration of :confval:`scripts.amplitude` in the
   file :file:`SEISCOMP_ROOT/etc/scvoice.cfg` or :file:`~/.seiscomp/scvoice.cfg`:

   .. code-block:: sh

      scripts.amplitude = /home/sysop/.seiscomp/amplitude.sh


Alert script
------------


#. Create a sound file :file:`siren.wav` for accoustic alerts.
#. Save an executable script file under, e.g., :file:`~/.seiscomp/alert.sh`:

   .. code-block:: sh

      #!/bin/sh
      playwave /home/sysop/.seiscomp/siren.wav

#. Add the script filename to the configuration of :confval:`scripts.alert` in
   the file :file:`SEISCOMP_ROOT/etc/scvoice.cfg` or :file:`~/.seiscomp/scvoice.cfg`.

   .. code-block:: sh

      scripts.alert = /home/sysop/.seiscomp/alert.sh
