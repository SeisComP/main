import os
import subprocess
import time

import seiscomp.kernel


class Module(seiscomp.kernel.Module):
    def __init__(self, env):
        super().__init__(env, env.moduleName(__file__))

    def supportsAliases(self):
        # The default handler does not support aliases
        return True

    def reload(self):
        if not self.isRunning():
            self.env.log(f"{self.name} is not running")
            return 1

        self.env.log(f"reloading {self.name}")

        lockfile = self.env.lockFile(self.name)
        reloadfile = os.path.join(os.path.dirname(lockfile), f"{self.name}.reload")

        # Open pid file
        with open(lockfile, "r", encoding="utf-8") as f:
            # Try to read the pid
            pid = int(f.readline())

            # touch reload file
            with open(reloadfile, "a", encoding="utf-8") as _:
                pass

            if not os.path.isfile(reloadfile):
                self.env.log(f"could not touch reload file: {reloadfile}")
                return 1

            # Send SIGHUP
            subprocess.call(f"kill -s HUP {pid}", shell=True)

            # wait for reload file to disappear
            for _ in range(0, int(self.reloadTimeout * 5)):
                time.sleep(0.2)
                if not os.path.isfile(reloadfile):
                    return 0

            self.env.log("timeout exceeded")

        return 1


# Uncomment for authbind (running service on privileged ports)
#  def _run(self):
#    params = "--depth 2 " + self.env.binaryFile(self.name) + " " + self._get_start_params()
#    binaryPath = "authbind"
#    return self.env.start(self.name, binaryPath, params)
