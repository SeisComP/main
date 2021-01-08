import os
import sys
import seiscomp.kernel
import seiscomp.config
import seiscomp.system


def parseBindPort(bind):
    bindToks = bind.split(':')
    if len(bindToks) == 1:
        return int(bindToks[0])
    elif len(bindToks) == 2:
        return int(bindToks[1])
    else:
        return -1


class Module(seiscomp.kernel.Module):
    def __init__(self, env):
        seiscomp.kernel.Module.__init__(self, env, "inventory")
        # This is a config module which synchronizes bindings with the database
        self.isConfigModule = True
        # Give this module a high priority to be executed at first (unless
        # another module defines a negative value. It ensures that successive
        # modules can read an up-to-date inventory and use the latest rc files.
        self.order = 0

    def updateConfig(self):
        messaging = True
        messagingPort = 18180
        messagingProtocol = 'scmp'

        try:
            messaging = self.env.getBool("messaging.enable")
        except:
            pass

        # If messaging is disabled in kernel.cfg, do not do anything
        if not messaging:
            sys.stdout.write("- messaging disabled, nothing to do\n")
            return 0

        # Load scmaster configuration and figure the bind ports of scmaster out
        cfg = seiscomp.config.Config()
        seiscomp.system.Environment.Instance().initConfig(cfg, "scmaster")

        # First check the unencrypted port and prefer that
        p = parseBindPort(cfg.getString("interface.bind"))
        if p > 0:
            messagingPort = p

            try:
                bind = self.env.getString("messaging.bind")
                bindToks = bind.split(':')
                if len(bindToks) == 1:
                    messagingPort = int(bindToks[0])
                elif len(bindToks) == 2:
                    messagingPort = int(bindToks[1])
                else:
                    sys.stdout.write(
                        "E invalid messaging bind parameter: %s\n" % bind)
                    sys.stdout.write("  expected either 'port' or 'ip:port'\n")
                    return 1
            except:
                pass

        # Otherwise check if ssl is enabled
        else:
            p = parseBindPort(cfg.getString("interface.ssl.bind"))
            if p > 0:
                messagingPort = p
                messagingProtocol = 'scmps'

        # Synchronize inventory
        return os.system("scinv sync --console=1 -H %s://localhost:%d/production --filebase \"%s\" --rc-dir \"%s\" --key-dir \"%s\""
                         % (messagingProtocol, messagingPort,
                            os.path.join(self.env.root, "etc", "inventory"),
                            os.path.join(self.env.root, "var", "lib", "rc"),
                            os.path.join(self.env.root, "etc", "key")))

