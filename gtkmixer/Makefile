ICONSDIR = icons
DVMPATH=/home/user/dev/fs/pub/
ICONS = \
		$(ICONSDIR)/date.png \
		$(ICONSDIR)/input.png \
		$(ICONSDIR)/langkb.png \
		$(ICONSDIR)/network.png \
		$(ICONSDIR)/security.png \
		$(ICONSDIR)/powermgr.png \
		$(ICONSDIR)/env.png \
		$(ICONSDIR)/about.png \
		$(ICONSDIR)/dvmcfg-hplogo.png \
		$(ICONSDIR)/powermgr-ac.png \
		$(ICONSDIR)/powermgr-ba.png \
		$(ICONSDIR)/about-line.png \
		$(ICONSDIR)/about-hplogo.png \
		$(ICONSDIR)/imagetab-background.png \
		$(ICONSDIR)/about-stlogo.png

all: gtkalsamixer.c
	gcc -c gtkalsamixer.c -o _gtkalsamixer.o -I./ `pkg-config gtk+-2.0 alsa --cflags`
	gcc _gtkalsamixer.o -o mixer `pkg-config gtk+-2.0 gmodule-2.0 alsa --libs`

_cfg_inline_pixbuf.h: $(ICONS)
	gdk-pixbuf-csource --raw --build-list \
		date_icon_pixbuf $(ICONSDIR)/date.png \
		input_icon_pixbuf $(ICONSDIR)/input.png \
		langkb_icon_pixbuf $(ICONSDIR)/langkb.png \
		network_icon_pixbuf  $(ICONSDIR)/network.png \
		security_icon_pixbuf $(ICONSDIR)/security.png \
		powermgr_icon_pixbuf $(ICONSDIR)/powermgr.png \
		env_icon_pixbuf      $(ICONSDIR)/env.png \
		about_icon_pixbuf    $(ICONSDIR)/about.png \
		dvmcfg_hplogo_pixbuf $(ICONSDIR)/dvmcfg-hplogo.png \
		about_line_pixbuf    $(ICONSDIR)/about-line.png \
		about_hplogo_pixbuf  $(ICONSDIR)/about-hplogo.png \
		about_stlogo_pixbuf  $(ICONSDIR)/about-stlogo.png \
		powermgr_ac_pixbuf   $(ICONSDIR)/powermgr-ac.png \
		powermgr_ba_pixbuf   $(ICONSDIR)/powermgr-ba.png \
		imagetab_background_pixbuf   $(ICONSDIR)/imagetab-background.png \
		> _cfg_inline_pixbuf.h

install:
	qscp go 10.10.106.2:/usr/lib/dvm-config/dvmcfg

cfg:
	strip go networkcfg.so
	cp go $(DVMPATH)/dvmcfg
	cp networkcfg.so $(DVMPATH)/networkcfg.so
po:
	xgettext --from-code=UTF-8 -kN_ -o dvmcfg.po dvmcfg.c
py:
	qscp dvmcfg.py 192.168.9.90:/usr/lib/dvm-config/dvmcfg.py
	qscp libaurora.so 192.168.9.90:/usr/lib/dvm-config/libaurora.so
	qscp gtkrc 192.168.9.90:/usr/lib/dvm-config/gtkrc
	qscp icons/about.png 192.168.9.90:/usr/lib/dvm-config/icons/about.png
	qscp icons/date.png 192.168.9.90:/usr/lib/dvm-config/icons/date.png
	qscp icons/env.png 192.168.9.90:/usr/lib/dvm-config/icons/env.png
	qscp icons/input.png 192.168.9.90:/usr/lib/dvm-config/icons/input.png
	qscp icons/langkb.png 192.168.9.90:/usr/lib/dvm-config/icons/langkb.png
	qscp icons/network.png 192.168.9.90:/usr/lib/dvm-config/icons/network.png
	qscp icons/powermgr.png 192.168.9.90:/usr/lib/dvm-config/icons/powermgr.png
	qscp icons/security.png 192.168.9.90:/usr/lib/dvm-config/icons/security.png
clean:
	rm _cfg_inline_pixbuf.h *.o -f
