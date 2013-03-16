#ifndef _LAUNCHER_H
#define _LAUNCHER_H

#include <sqlite3.h>
#include <gtk/gtk.h>

#define GETTEXT_PACKAGE "launcher"
#define LOCALEDIR "/usr/share/locale"

#include <glib/gi18n-lib.h>
#include <libintl.h>
#include <stdlib.h>


#define DEBUG(...) fprintf(stdout, "[launcher] "__VA_ARGS__)
#define ERROR(...) fprintf(stderr, "[launcher] "__VA_ARGS__)
#define DD(...) fprintf(stdout, __VA_ARGS__)

#define ICONS_PATH "/usr/share/launcher/icons"
#define MUSIC_PATH "/usr/share/music"
#define PHOTO_PATH "/usr/share/photo"

#define PHOTO_PATH1 "/home/user/PHOTOS/nanjing"
#define PHOTO_PATH2 "/home/user/PHOTOS/gnome"
#define PHOTO_PATH3 "/home/user/PHOTOS/baby"

#define MUSIC_PATH1 "/usr/share/MUSIC/music01"
#define MUSIC_PATH2 "/usr/share/MUSIC/music02"
#define MUSIC_PATH3 "/usr/share/MUSIC/music03"

#define BOOKMARK_SQLITE3_DBFILE   "/tmp/.places.sqlite"

#define H1_FONT_FORMAT "<span size='large' weight='bold' underline_color='#FFFFFF' foreground='#FFFFFF' >%s</span>"
#define H3_FONT_FORMAT "<span size='medium' foreground='#FFFFFF' >%s</span>"
#define HDR_FONT_FORMAT "<span foreground='#6C6CD0'>%s</span>"

#define EBOX_WEB_UI   0xff0909
#define EBOX_MUSIC_UI 0xff0808
#define EBOX_PHOTO_UI 0xff0707

#define WEBUI_WIDTH  150
#define WEBUI_HEIGHT 120

#define MEDIAUI_WIDTH  95
#define MEDIAUI_HEIGHT 100

#endif /*_LAUNCHER_H*/
