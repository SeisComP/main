################################################################################
# Copyright (C) 2013-2014 gempa GmbH
#
# Thread-safe file logger
#
# Author:  Stephan Herrnkind
# Email:   herrnkind@gempa.de
################################################################################

import os
import sys
import time
import threading

from queue import Queue

# -------------------------------------------------------------------------------


def _worker(log):
    while True:
        # pylint: disable=W0212
        msg = log._queue.get()
        log._write(str(msg))
        log._queue.task_done()


################################################################################
class Log:
    # ---------------------------------------------------------------------------
    def __init__(self, filePath, archiveSize=7):
        self._filePath = filePath
        self._basePath = os.path.dirname(filePath)
        self._fileName = os.path.basename(filePath)
        self._archiveSize = archiveSize
        self._queue = Queue()
        self._lastLogTime = None
        self._fd = None

        self._archiveSize = max(self._archiveSize, 0)

        # worker thread, responsible for writing messages to file
        t = threading.Thread(target=_worker, args=(self,))
        t.daemon = True
        t.start()

    # ---------------------------------------------------------------------------
    def __del__(self):
        # wait for worker thread to write all pending log messages
        self._queue.join()

        if self._fd is not None:
            self._fd.close()

    # ---------------------------------------------------------------------------
    def log(self, msg):
        self._queue.put(msg)

    # ---------------------------------------------------------------------------
    def _rotate(self):
        self._fd.close()
        self._fd = None

        try:
            pattern = f"{self._filePath}.%i"
            for i in range(self._archiveSize, 1, -1):
                src = pattern % (i - 1)
                if os.path.isfile(src):
                    os.rename(pattern % (i - 1), pattern % i)
            os.rename(self._filePath, pattern % 1)
        except Exception as e:
            print(f"failed to rotate access log: {e}", file=sys.stderr)

        self._fd = open(self._filePath, "w", encoding="utf-8")

    # ---------------------------------------------------------------------------
    def _write(self, msg):
        try:
            now = time.localtime()
            if self._fd is None:
                if self._basePath and not os.path.exists(self._basePath):
                    os.makedirs(self._basePath)
                self._fd = open(self._filePath, "a", encoding="utf-8")
            elif (
                self._archiveSize > 0
                and self._lastLogTime is not None
                and (
                    self._lastLogTime.tm_yday != now.tm_yday
                    or self._lastLogTime.tm_year != now.tm_year
                )
            ):
                self._rotate()

            print(msg, file=self._fd)
            self._fd.flush()
            self._lastLogTime = now
        except Exception as e:
            print(f"access log: {e}", file=sys.stderr)


# vim: ts=4 et
