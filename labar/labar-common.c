#include <X11/Xlib.h>
#include "labar-types.h"

#define DEBUG(...) fprintf(stdout, "[labar.common] "__VA_ARGS__)

#define GLOBAL_GROUP "GLOBAL"

void _free_item(gpointer data, gpointer userdata) {
  la_item_t *item = (la_item_t *)data;

  if(item->nr_icon)
    g_object_unref(item->nr_icon);
  if(item->hl_icon)
    g_object_unref(item->hl_icon);
  if(item->title)
    free(item->title);
  if(item->exec)
    free(item->exec);
}

void la_conf_free(la_conf_t *c) {
  if(c->font)
    free(c->font);

  g_slist_foreach(c->apps, _free_item, NULL);
  g_slist_free(c->apps);
}

void la_conf_init(la_conf_t *c, char *confpath) {
  const gchar *posstr[] = {"top", "bottom", "left", "right", NULL};
  GError *err = NULL;
  gchar **groups = NULL;
  gsize len;

  c->kf = g_key_file_new();
  g_key_file_load_from_file(c->kf, confpath, G_KEY_FILE_NONE, &err);

  groups = g_key_file_get_groups(c->kf, &len);
  if(len > 0) {
    int i = 0;

    c->apps = NULL;
    for(; i<len; i++) {
      if(strcmp(groups[i], GLOBAL_GROUP) == 0) {
        gchar *_pos = NULL;

        c->font = g_key_file_get_value(c->kf, GLOBAL_GROUP, "font", &err);

        c->expand = g_key_file_get_boolean(c->kf, GLOBAL_GROUP, "expand", &err);
        if(!c->expand) {
          if(err) {
            DEBUG("parse GLOBAL.expand failure!, set to default TRUE\n");
            c->expand = TRUE;
          }
        }

        c->pos = LA_BAR_POS_BOTTOM; /**set default value*/
        _pos = g_key_file_get_string(c->kf, GLOBAL_GROUP, "pos", &err);
        if(_pos) {
          gint idx = 0;
          for(; posstr[idx]; idx++) {
            if(strcmp(_pos, posstr[idx]) == 0) {
              c->pos = idx;
              break;
            }
          }
          free(_pos);
        }
      } else {
        la_item_t *item = NULL;
        char *iconpath = NULL;

        item = g_malloc0(sizeof(la_item_t));
        item->title = g_strdup(groups[i]);
        err=NULL, iconpath = g_key_file_get_string(c->kf, groups[i], "nr_icon", &err);
        if(iconpath) {
          err=NULL, item->nr_icon = gdk_pixbuf_new_from_file(iconpath, &err);
          //DEBUG("nr-icon = %s, nr-icon.obj = %x, %s\n", iconpath, item->nr_icon, err->message);
          free(iconpath);
        }

        err=NULL, iconpath = g_key_file_get_string(c->kf, groups[i], "hl_icon", &err);
        if(iconpath) {
          err=NULL, item->hl_icon = gdk_pixbuf_new_from_file(iconpath, &err);
          DEBUG("nl-icon = %s, nl_icon.obj = %x\n", iconpath, item->hl_icon);
          free(iconpath);
        }
        err=NULL, item->exec = g_key_file_get_string(c->kf, groups[i], "exec", &err);
        DEBUG("exec path = %s\n", item->exec);

        c->apps = g_slist_append(c->apps, item);
      }
    } /**for(...)*/

    g_strfreev(groups);
  }
}

