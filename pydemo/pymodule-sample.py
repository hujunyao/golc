import gtk
import pango
import struct
import baseconfig
import dvmcfg

class ConfigPage(baseconfig.Page):
  def __init__(self):
    self.icon = "date.png"
    self.tabicon = "env-tab.jpg"
    self.label = "Date"
    self.tooltip = "Environment Config Page"
    self.description = "DVMConfig powered by DeviceVM"

  def doAction(self):
    print 'Do real action HERE'

  def pid_watch_cb(self, status):
    print 'watch cb here,%d', status
    print self, '\n'

  def createPage(self, dialog, ok, cancel):
    pid = dvmcfg.launchapp(self.pid_watch_cb, "/usr/bin/gnome-appearance-properties");
    return None
