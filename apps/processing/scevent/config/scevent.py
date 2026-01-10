import os
import seiscomp.config
import seiscomp.kernel
import seiscomp.system


class Module(seiscomp.kernel.Module):
    def __init__(self, env):
        seiscomp.kernel.Module.__init__(self, env, env.moduleName(__file__))

    def updateConfigProxy(self):
        return "trunk"

    def updateConfig(self):
        # By default the "trunk" module must be configured to write the
        # bindings into the database
        return 0

    def supportsAliases(self):
        # The default handler does not support aliases
        return True

    def setup(self, setup_config):
        cfgfile = os.path.join(self.env.SEISCOMP_ROOT, "etc", self.env.moduleName(__file__) + ".cfg")

        cfg = seiscomp.config.Config()
        cfg.readConfig(cfgfile)
        try:
            cfg.setString("eventIDPrefix", setup_config.getString("scevent.eventID.prefix"))
        except:
            cfg.remove("eventIDPrefix")

        cfg.writeConfig()

        return 0
