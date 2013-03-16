import gtk
import pango
import dvmcfg

def okbtn_cb(self, button):
  print "About OK Button Clicked"

def cancelbtn_cb(self, button):
  print "About Cancel Button Clicked"

def create_ui(dialog, ok, cancel):
  vbox = gtk.VBox(False, 4)
  vbox.set_border_width(2)

  vvbox = gtk.VBox(False, 2)
  vhbox = gtk.HBox(False, 2)
  vhbox.set_border_width(2)
  image = gtk.Image()
  image.set_from_pixbuf(dvmcfg.get_pixbuf("about.hplogo"))
  vhbox.pack_start(image, False, False, 28)
  vvbox.pack_start(vhbox, False, True, 0)
  image = gtk.Image()
  image.set_from_pixbuf(dvmcfg.get_pixbuf("about.line"))
  vvbox.pack_start(image, False, True, 0)
  vbox.pack_start(vvbox, False, True, 8)

  vvbox = gtk.VBox(False, 2)
  hbox = gtk.HBox(False, 0)
  label = gtk.Label("")
  label.set_markup("<span size='small'>With a fast boot speed of only 5 seconds*, the HP QuickWeb offers a unique \nenvironment that allows you to enjoy instant access to commonly used functions like \nweb browsing or communication without entering the traditional OS.</span>")
  hbox.pack_start(label, True, True, 28)
  vvbox.pack_start(hbox, False, True, 2)

  hbox = gtk.HBox(False, 0)
  label = gtk.Label("")
  label.set_markup("<span size='small'> * The boot speed depends on system configuration.</span>")
  hbox.pack_start(label, False, True, 28)
  vvbox.pack_start(hbox, False, True, 12)

  hbox = gtk.HBox(False, 0)
  label = gtk.Label("")
  label.set_markup("<span size='x-small'> Powered by</span>")
  hbox.pack_start(label, False, True, 28)
  vvbox.pack_start(hbox, False, True, 2)

  hbox = gtk.HBox(False, 0)
  image = gtk.Image()
  image.set_from_pixbuf(dvmcfg.get_pixbuf("about.stlogo"))
  hbox.pack_start(image, False, True, 48)
  vvbox.pack_start(hbox, False, True, 2)

  hbox = gtk.HBox(False, 0)
  label = gtk.Label("")
  label.set_markup("<span size='small'>Copyright 2007-2008 DeviceVM, Inc. All Right Reserved.\nFor a list of included 3rd party software, please view Credits file at\n http://www.splashtop.com/dvmsplashtop/st/credits.html</span>")
  hbox.pack_start(label, False, True, 28)
  vvbox.pack_start(hbox, False, True, 0)

  vbox.pack_end(vvbox, False, True, 16)

  ok.connect("clicked", okbtn_cb, ok)
  cancel.connect("clicked", cancelbtn_cb, ok)

  return vbox
