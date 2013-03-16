#ifndef __LABAR_TYPES_H__
#define __LABAR_TYPES_H__
#include <glib-2.0/glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

typedef enum {
  LA_BAR_POS_TOP,
  LA_BAR_POS_BOTTOM,
  LA_BAR_POS_LEFT,
  LA_BAR_POS_RIGHT
} la_bar_pos_t;

typedef struct {
  GKeyFile *kf;
  char *font;
  gboolean expand;
  la_bar_pos_t pos;
  GSList *apps;
} la_conf_t;

typedef struct {
  GdkPixbuf *nr_icon;
  GdkPixbuf *hl_icon;
  gchar *exec;
  gchar *title;
} la_item_t;

#endif
