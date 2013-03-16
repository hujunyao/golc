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

all: gtkimagetab.c dvmcfg.c _cfg_inline_pixbuf.h networkcfg.c
	gcc -c gtkimagetab.c -o _gtkimagetab.o -I./ `pkg-config gtk+-2.0 --cflags`
	gcc -c networkcfg.c -o _networkcfg.o -I./ `pkg-config gtk+-2.0 --cflags`
	gcc -Wl,-Bsymbolic --share --soname -o networkcfg.so _networkcfg.o 
	gcc -c dvmcfg.c -o _dvmcfg.o -I/usr/include/python2.4 -I./ `pkg-config gtk+-2.0 --cflags`
	gcc -Xlinker -export-dynamic _dvmcfg.o _gtkimagetab.o -o go `pkg-config gtk+-2.0 gmodule-2.0 --libs` -lnsl -ldl -lreadline -ltermcap -lieee -lpthread -lutil -lm /usr/lib/python2.4/config/libpython2.4.a

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

po:
	xgettext --from-code=UTF-8 -kN_ -o dvmcfg.po dvmcfg.c

clean:
	rm _cfg_inline_pixbuf.h *.o -f
