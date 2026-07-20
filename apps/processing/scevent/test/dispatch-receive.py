#!/usr/bin/env python3
"""
Standalone dispatch/receive helper for the scevent remote test.

Launched as a separate process by the test harness (see ManagedDispatchReceive in
utils.py), the same way scmaster and scevent are launched. Running it as its own process
keeps the blocking SeisComP run loop on this process's main thread with its own GIL, so
it cannot deadlock against the controlling test process the way an in-process worker
thread did.

Behavior:
  * connect to the messaging broker given by -H
  * subscribe to the EVENT group
  * if --origin FILE is given, read the first origin from that SCML file and
    send it once messaging is up
  * if --split-origin ORIGINID (together with --split-event EVENTID) is given,
    send an "EvSplitOrg" journal request that asks scevent to remove ORIGINID
    from event EVENTID and form a new event for it. This is used to exercise
    the eventID synchronization on the split-origin code path.
  * if --new-event FILE is given, read the first origin from that SCML file and
    send it together with an "EvNewEvent" journal request (in a single message)
    that asks scevent to form a new event from that (as yet unassociated)
    origin. This exercises the eventID synchronization on the new-event code
    path.
  * wait for the first Event object, print

        EVENTID <publicID>

    to stdout and quit
  * if no event arrives within --event-timeout seconds (default 5), print

        NOEVENT

    to stdout and exit non-zero

The harness reads the EVENTID / NOEVENT line back from the captured stdout.

The helper-specific options (--origin, --event-timeout, --split-origin,
--split-event, --new-event) are parsed from argv here and stripped before the
remaining
arguments are handed to the SeisComP Application, so the app only ever sees
options it natively understands (-H, --logging.level, --log-file, ...). This
avoids depending on the custom command-line-option registration API.

Compatible with Python >= 3.6.
"""

import os
import signal
import sys
import threading
import time

from seiscomp import client, core, datamodel, io as scio, logging


def _extractOption(argv, name):
    """Remove '--name=VALUE' or '--name VALUE' from argv (in place) and return
    VALUE, or None if the option is not present."""
    prefix = "--" + name + "="
    for i, arg in enumerate(argv):
        if arg.startswith(prefix):
            value = arg[len(prefix) :]
            del argv[i]
            return value
        if arg == "--" + name:
            if i + 1 < len(argv):
                value = argv[i + 1]
                del argv[i : i + 2]
                return value
            del argv[i]
            return None
    return None


class DispatchReceive(client.Application):
    def __init__(self, argc, argv, originFile, eventTimeout,
                 splitOrigin=None, splitEvent=None, newEventFile=None):
        super().__init__(argc, argv)
        self.setMessagingEnabled(True)
        self.setDatabaseEnabled(False, False)
        self.setPrimaryMessagingGroup("LOCATION")
        self.addMessagingSubscription("EVENT")
        self.setLoggingToStdErr(True)

        self._originFile = originFile
        self._splitOrigin = splitOrigin
        self._splitEvent = splitEvent
        self._newEventFile = newEventFile
        self._eventTimeout = eventTimeout
        self._secondsWaited = 0
        self._done = False
        # Seconds to allow the graceful quit()/messaging shutdown to complete
        # before the watchdog force-exits the process. Kept short: the helper
        # has already emitted its result by the time shutdown starts.
        self._shutdownGraceSeconds = 3.0
        self._watchdogStarted = False

    @staticmethod
    def readEventParameters(fileName):
        ar = scio.XMLArchive()
        if not ar.open(fileName):
            raise OSError("could not read SeisComP XML")
        obj = ar.readObject()
        if not obj:
            raise ValueError("no object found")
        ep = datamodel.EventParameters.Cast(obj)
        if not ep:
            raise ValueError("XML object not of type EventParameters")

        return ep

    def init(self):
        if not super().init():
            return False

        # Wall-clock timeout so we never block forever waiting for an event.
        # handleTimeout fires once per second.
        self.enableTimer(1)

        if self._originFile:
            try:
                logging.info("sending origin")
                self._sendOrigin(self._originFile)
            except Exception as e:  # noqa: BLE001
                print("ERROR " + repr(e), flush=True)
                return False

        if self._splitOrigin:
            try:
                logging.info("sending EvSplitOrg request")
                self._sendSplitRequest(self._splitEvent, self._splitOrigin)
            except Exception as e:  # noqa: BLE001
                print("ERROR " + repr(e), flush=True)
                return False

        if self._newEventFile:
            try:
                logging.info("sending EvNewEvent request")
                self._sendNewEvent(self._newEventFile)
            except Exception as e:  # noqa: BLE001
                print("ERROR " + repr(e), flush=True)
                return False

        return True

    # Public ID of the EventParameters root object; used as the parent ID when
    # publishing origin and pick notifiers, matching what scevent expects to receive
    # over the messaging bus.
    EP_PARENT_ID = "EventParameters"

    # Public ID of the Journaling root object; used as the parent ID when
    # publishing JournalEntry notifiers. Journaling, like EventParameters, has a
    # fixed public ID in the SeisComP data model.
    JOURNALING_PARENT_ID = "Journaling"

    # Messaging group scevent listens on for journal entries (its primary group).
    EVENT_GROUP = "EVENT"

    def _sendOrigin(self, originFile):
        ep = self.readEventParameters(originFile)
        if ep.originCount() == 0:
            raise ValueError("no origin found")

        # The event-ID association algorithm compares origins by their common pick set:
        # it walks the incoming origin's arrivals, resolves each referenced Pick and
        # compares pick times (eventAssociation. maxMatchingPicksTimeDiff >= 0, the
        # default). So the message must carry the origin *with its arrivals* AND the
        # picks those arrivals reference, otherwise the receiver cannot resolve the
        # picks and the pick-based match silently fails.
        #
        # Notifier.Create records an OP_ADD for each object without moving or
        # re-parenting it, so the origin and picks can stay attached to 'ep'. The
        # arrivals ride along with the origin (they are sub-elements of it). 'ep' is
        # kept referenced until after send() so none of the referenced objects are
        # collected mid-flight.
        origin = ep.origin(0)
        originID = origin.publicID()

        picks = []
        for i in range(ep.pickCount()):
            picks.append(ep.pick(i))

        datamodel.Notifier.SetEnabled(True)
        # Picks first so they are already present when the receiver processes the origin
        # and tries to resolve its arrivals' picks.
        for pick in picks:
            datamodel.Notifier.Create(self.EP_PARENT_ID, datamodel.OP_ADD, pick)
        datamodel.Notifier.Create(self.EP_PARENT_ID, datamodel.OP_ADD, origin)
        msg = datamodel.Notifier.GetMessage()
        datamodel.Notifier.SetEnabled(False)
        if msg is None:
            raise ValueError("no notifier message generated for origin")

        self.connection().send(msg)
        logging.info(f"origin sent: {originID}")
        # 'ep' (and thus the origin/picks referenced above) stays alive as a local until
        # the function returns here, well after send().

    def _sendSplitRequest(self, eventID, originID):
        """Send an "EvSplitOrg" journal request asking scevent to remove
        origin ``originID`` from event ``eventID`` and form a new event for it.

        The journal entry maps to scevent's handleJournalEntry():
          * objectID   -> the event the origin currently belongs to
          * action     -> "EvSplitOrg"
          * parameters -> the origin to split off
        The entry is published under the fixed Journaling parent and sent on the
        EVENT group, which is where scevent expects journal entries.
        """
        if not eventID:
            raise ValueError("EvSplitOrg requires the current event ID "
                             "(--split-event)")

        entry = datamodel.JournalEntry()
        entry.setObjectID(eventID)
        entry.setAction("EvSplitOrg")
        entry.setParameters(originID)
        entry.setSender("dispatch-receive")
        entry.setCreated(core.Time.UTC())

        datamodel.Notifier.SetEnabled(True)
        datamodel.Notifier.Create(
            self.JOURNALING_PARENT_ID, datamodel.OP_ADD, entry
        )
        msg = datamodel.Notifier.GetMessage()
        datamodel.Notifier.SetEnabled(False)
        if msg is None:
            raise ValueError("no notifier message generated for EvSplitOrg")

        self.connection().send(self.EVENT_GROUP, msg)
        logging.info(
            f"EvSplitOrg sent: event={eventID} origin={originID}"
        )

    def _sendNewEvent(self, originFile):
        """Send the origin from ``originFile`` together with an "EvNewEvent"
        journal request for it, in a single message.

        EvNewEvent requires the origin to be present in scevent but not yet
        associated with an event. scevent processes all objects of a message
        first (running the EvNewEvent journal handler, which then forms the new
        event) before it works its queued origin-association TODO list, so
        packing the origin and the journal entry into one message lets the
        journal handler claim the origin before automatic association would.

        The journal entry maps to scevent's handleJournalEntry():
          * objectID   -> the origin publicID to form the new event from
          * action     -> "EvNewEvent"
          * parameters -> unused
        """
        ep = self.readEventParameters(originFile)
        if ep.originCount() == 0:
            raise ValueError("no origin found")

        origin = ep.origin(0)
        originID = origin.publicID()

        picks = []
        for i in range(ep.pickCount()):
            picks.append(ep.pick(i))

        entry = datamodel.JournalEntry()
        entry.setObjectID(originID)
        entry.setAction("EvNewEvent")
        entry.setParameters("")
        entry.setSender("dispatch-receive")
        entry.setCreated(core.Time.UTC())

        datamodel.Notifier.SetEnabled(True)
        # Picks and origin first so the origin is known when the journal entry
        # (added last, in the same message) is handled.
        for pick in picks:
            datamodel.Notifier.Create(self.EP_PARENT_ID, datamodel.OP_ADD, pick)
        datamodel.Notifier.Create(self.EP_PARENT_ID, datamodel.OP_ADD, origin)
        datamodel.Notifier.Create(
            self.JOURNALING_PARENT_ID, datamodel.OP_ADD, entry
        )
        msg = datamodel.Notifier.GetMessage()
        datamodel.Notifier.SetEnabled(False)
        if msg is None:
            raise ValueError("no notifier message generated for EvNewEvent")

        self.connection().send(self.EVENT_GROUP, msg)
        logging.info(f"EvNewEvent sent: origin={originID}")
        # 'ep' stays referenced until the function returns, after send().

    def handleTimeout(self):
        self._secondsWaited += 1
        if not self._done and self._secondsWaited >= self._eventTimeout:
            print("NOEVENT", flush=True)
            logging.info("terminating")
            self._armShutdownWatchdog()
            self.quit()

    def _reportEvent(self, event):
        if self._done:
            return
        self._done = True
        logging.info(f"received eventID: {event.publicID()}")
        print("EVENTID " + event.publicID(), flush=True)
        # Stop the run loop from inside its own thread; the clean, supported way to
        # terminate a SeisComP client.
        logging.info("terminating")
        self._armShutdownWatchdog()
        self.quit()

    def updateObject(self, parentID, scobject):
        # In split / new-event mode the interesting event is the *new* one
        # created for the origin, which arrives via addObject(). Pre-existing
        # events are only *modified* (e.g. origin reference removed) and show up
        # here via updateObject(); ignore them so we report the freshly formed
        # event rather than a modified pre-existing one.
        if self._splitOrigin or self._newEventFile:
            return
        event = datamodel.Event.Cast(scobject)
        if event:
            self._reportEvent(event)

    def addObject(self, parentID, scobject):
        event = datamodel.Event.Cast(scobject)
        if event:
            self._reportEvent(event)

    def reported(self):
        return self._done

    def _armShutdownWatchdog(self):
        # Arm a fallback that force-terminates the process if the graceful
        # quit()/messaging shutdown does not complete within the grace period.
        #
        # This MUST be GIL-independent. The graceful shutdown ends in a C++
        # message-thread join (Application::done) that runs with the GIL held
        # and never releases it (no Py_BEGIN_ALLOW_THREADS). While the main
        # thread is parked there, no Python code on any thread can run -- a
        # Python daemon-thread watchdog wakes from sleep() but then blocks
        # re-acquiring the GIL until the join returns, so it fires only *after*
        # the stall it was meant to cut short (observed: it triggered at the
        # exact moment "Leaving ::done" was logged, ~15 s in).
        #
        # A kernel timer sidesteps the GIL entirely: setitimer/alarm schedules
        # SIGALRM, and with NO Python handler installed the kernel's default
        # disposition for SIGALRM terminates the process. That happens no
        # matter who holds the GIL, so it reliably bounds the shutdown. The
        # EVENTID/NOEVENT line was already written and flushed before this is
        # armed, so a signal-kill loses no output.
        #
        # Signal scheduling is only allowed on the main thread; the report
        # callbacks that call this run on the run-loop (main) thread.
        if self._watchdogStarted:
            return
        self._watchdogStarted = True

        try:
            # Force the default (terminate) disposition so delivery kills the
            # process without needing the GIL or a Python-level handler.
            signal.signal(signal.SIGALRM, signal.SIG_DFL)
            grace = self._shutdownGraceSeconds
            if hasattr(signal, "setitimer"):
                signal.setitimer(signal.ITIMER_REAL, grace)
            else:
                import math
                signal.alarm(max(1, int(math.ceil(grace))))
        except (ValueError, OSError, RuntimeError, AttributeError):
            # Not on the main thread, or no SIGALRM on this platform: fall back
            # to a minimal daemon-thread watchdog. It is subject to the GIL
            # limitation above so it may be late, but it does the absolute
            # minimum (no logging, no buffered I/O) to stay finalization-safe.
            def watchdog():
                time.sleep(self._shutdownGraceSeconds)
                try:
                    os.write(2, b"shutdown-watchdog: forcing exit\n")
                except Exception:  # noqa: BLE001
                    pass
                os._exit(0)

            t = threading.Thread(target=watchdog, name="shutdown-watchdog")
            t.daemon = True
            t.start()


def main():
    argv = list(sys.argv)

    originFile = _extractOption(argv, "origin")
    splitOrigin = _extractOption(argv, "split-origin")
    splitEvent = _extractOption(argv, "split-event")
    newEventFile = _extractOption(argv, "new-event")

    eventTimeoutStr = _extractOption(argv, "event-timeout")
    try:
        eventTimeout = float(eventTimeoutStr) if eventTimeoutStr else 5.0
    except ValueError:
        eventTimeout = 5.0

    app = DispatchReceive(
        len(argv), argv, originFile, eventTimeout, splitOrigin, splitEvent,
        newEventFile
    )
    rc = app()

    # Graceful shutdown completed (app() returned). Cancel any pending
    # shutdown alarm so it cannot fire during the normal exit path and turn a
    # clean run into a signal-kill.
    try:
        if hasattr(signal, "setitimer"):
            signal.setitimer(signal.ITIMER_REAL, 0)
        else:
            signal.alarm(0)
    except Exception:  # noqa: BLE001
        pass

    # If the loop ended without ever reporting (e.g., messaging failure), emit a
    # definitive line so the harness is not left guessing.
    if not app.reported():
        print("NOEVENT", flush=True)
        return rc if rc != 0 else 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
