#include <sqlite3.h>
#include <gtk/gtk.h>

#define GETTEXT_PACKAGE "launcher"
#define LOCALEDIR "/usr/share/locale"

#include <glib/gi18n-lib.h>
#include <libintl.h>

#include "_inline_pixbuf.h"

#define DEBUG(...) fprintf(stdout, "[launcher] "__VA_ARGS__)
#define DD(...) fprintf(stdout, __VA_ARGS__)

#define H1_FONT_FORMAT "<span size='large' weight='bold' underline_color='#FFFFFF' foreground='#FFFFFF' >%s</span>"
#define H3_FONT_FORMAT "<span size='medium' foreground='#FFFFFF' >%s</span>"
#define HDR_FONT_FORMAT "<span foreground='#6C6CD0'>%s</span>"
#define BOOKMARK_SQLITE3_DBFILE "/home/user/.mozilla/firefox/ro1nl8mo.default/places.sqlite"


enum {
  BM_PIXBUF_COL = 0,
  BM_TITLE_COL,
  BM_URL_COL,
  BM_NUM_COL
};

enum {
  BM_MENU_PIXBUF,
  BM_TOOLBAR_PIXBUF,
  BM_TAG_PIXBUF,
  BM_UNSORTED_PIXBUF,
  BM_DIRECTORY_PIXBUF,
  BM_FAVICON_PIXBUF,
  BM_MAX_PIXBUF
};

static GdkPixbuf *g_background_pixbuf = NULL;
static GdkPixbuf *g_bmpixbuf[BM_MAX_PIXBUF];
static GtkWidget *g_main_window = NULL, *g_bookmark_dialog = NULL;
static GHashTable *g_bmht = NULL;

static void show_bookmark_dialog(gpointer data);

static gboolean main_window_draw_background(GtkWidget *widget, GdkEventExpose *event) {
  gint width = widget->allocation.width, height = widget->allocation.height;

  gdk_draw_pixbuf(widget->window, NULL, g_background_pixbuf, 0, 0, 0, 0, width, height, GDK_RGB_DITHER_NORMAL, 0, 0);

  return FALSE;
}

static gboolean main_window_close(GtkWidget *widget, GdkEvent *event, gpointer data) {
  DEBUG("close main window\n");
  gtk_main_quit();

  return FALSE;
}

static void web_ui_open_cb(GtkMenuItem *mi, gpointer data) {
  DEBUG("goto URL %s\n", g_object_get_data(data, "url"));
}

static void web_ui_change_favorite_cb(GtkMenuItem *mi, gpointer data) {
  show_bookmark_dialog(data);
}

static void web_ui_remove_favorite_cb(GtkMenuItem *mi, gpointer data) {
  gchar *url = NULL;

  url = g_object_get_data(data, "url");
  if(url)
    g_free(url);

  g_object_set_data(data, "url", NULL);
}

static void web_ui_new_bookmark_cb(GtkMenuItem *mi, gpointer data) {
  GtkWidget *dlg = NULL;
  GtkWidget *vhbox = NULL, *label = NULL, *entry = NULL;
  gchar markup[1024] = {0};
  gint resp;

#define FONT_FORMAT "<span size='medium' foreground='#FFFFFF' >%s:</span>"

  dlg = gtk_dialog_new_with_buttons(_("New Bookmark"), GTK_WINDOW(g_main_window), GTK_DIALOG_MODAL|GTK_DIALOG_NO_SEPARATOR, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);

  gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_ACCEPT);
  vhbox = gtk_hbox_new(FALSE, 0);
  label = gtk_label_new("");
  snprintf(markup, 1024, FONT_FORMAT, _("Localtion(URL)"));
  gtk_label_set_markup(GTK_LABEL(label), markup);
  gtk_box_pack_start(GTK_BOX(vhbox), label, FALSE, FALSE, 0);
  entry = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(vhbox), entry, TRUE, TRUE, 3);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), vhbox, TRUE, TRUE, 6);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), gtk_hseparator_new(), FALSE, TRUE, 8);
  gtk_container_set_border_width(GTK_CONTAINER(dlg), 10);

  gtk_widget_show_all(dlg);
  resp = gtk_dialog_run(GTK_DIALOG(dlg));
  if(resp == GTK_RESPONSE_ACCEPT) {
    gchar *url = NULL;

    if(url = g_object_get_data(G_OBJECT(data), "url"))
      g_free(url);

    url = strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    g_object_set_data(G_OBJECT(data), "url", url);
    DEBUG("new localtion URL %s\n", url);
  }
  gtk_widget_destroy(dlg);

  /**do generate web thumbnail here*/
}

static gboolean web_ui_button_press_event_cb(GtkWidget *widget, GdkEventButton *e, gpointer data) {
  if(e->type == GDK_BUTTON_PRESS) {
    if(e->button == 3) {
      GtkWidget *menu = NULL, *menuitem = NULL;

      menu = gtk_menu_new();
      menuitem = gtk_menu_item_new_with_label(_("Open"));
      g_signal_connect (menuitem, "activate", G_CALLBACK (web_ui_open_cb), data);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

      menuitem = gtk_menu_item_new_with_label(_("Change Favorite"));
      g_signal_connect (menuitem, "activate", G_CALLBACK (web_ui_change_favorite_cb), data);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
      menuitem = gtk_menu_item_new_with_label(_("Remove Favorite"));
      g_signal_connect (menuitem, "activate", G_CALLBACK (web_ui_remove_favorite_cb), data);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

      gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
      menuitem = gtk_menu_item_new_with_label(_("New Bookmark..."));
      g_signal_connect (menuitem, "activate", G_CALLBACK (web_ui_new_bookmark_cb), data);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

      gtk_widget_show_all(menu);
      gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, e->button, e->time);

      return TRUE;
    } else if(e->button == 1) {
      DEBUG("goto URL %s\n", g_object_get_data(G_OBJECT(data), "url"));
    }
  } 

  return FALSE;
}

static GtkWidget *create_web_thumbnail_view(GtkWidget *window) {
  GtkWidget *btn = NULL, *vbox = NULL, *vhbox = NULL;
  GtkWidget *ebox = NULL, *img = NULL;

  vbox = gtk_vbox_new(TRUE, 12);
  vhbox = gtk_hbox_new(TRUE, 12);
  img = gtk_image_new_from_file("icons/yahoo_128x128.png");
  g_object_set_data(G_OBJECT(img), "url", strdup("www.yahoo.com"));
  ebox = gtk_event_box_new();
  g_signal_connect(G_OBJECT(ebox), "button_press_event", G_CALLBACK(web_ui_button_press_event_cb), img);
  gtk_container_add(GTK_CONTAINER(ebox), img);
  gtk_box_pack_start(GTK_BOX(vhbox), ebox, TRUE, TRUE, 0);
  img = gtk_image_new_from_file("icons/163_128x128.png");
  g_object_set_data(G_OBJECT(img), "url", strdup("www.163.com"));
  ebox = gtk_event_box_new();
  g_signal_connect(G_OBJECT(ebox), "button_press_event", G_CALLBACK(web_ui_button_press_event_cb), img);
  gtk_container_add(GTK_CONTAINER(ebox), img);
  gtk_box_pack_start(GTK_BOX(vhbox), ebox, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, TRUE, TRUE, 0);

  vhbox = gtk_hbox_new(TRUE, 16);
  img = gtk_image_new_from_file("icons/sina_128x128.png");
  g_object_set_data(G_OBJECT(img), "url", strdup("news.sina.com.cn"));
  ebox = gtk_event_box_new();
  g_signal_connect(G_OBJECT(ebox), "button_press_event", G_CALLBACK(web_ui_button_press_event_cb), img);
  gtk_container_add(GTK_CONTAINER(ebox), img);
  gtk_box_pack_start(GTK_BOX(vhbox), ebox, TRUE, TRUE, 0);
  img = gtk_image_new_from_file("icons/google_128x128.png");
  g_object_set_data(G_OBJECT(img), "url", strdup("www.google.cn"));
  ebox = gtk_event_box_new();
  g_signal_connect(G_OBJECT(ebox), "button_press_event", G_CALLBACK(web_ui_button_press_event_cb), img);
  gtk_container_add(GTK_CONTAINER(ebox), img);
  gtk_box_pack_start(GTK_BOX(vhbox), ebox, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, TRUE, TRUE, 0);

  return vbox;
}

static GtkWidget *create_rss_ui(GtkWidget *window) {
  GtkWidget *label = NULL, *vbox = NULL, *vhbox = NULL;
  gchar str[1024] = {0};

  snprintf(str, 1024, H1_FONT_FORMAT, _("_RSS feeds"));
  vbox = gtk_vbox_new(FALSE, 0);
  vhbox = gtk_hbox_new(FALSE, 0);
  label = gtk_label_new(NULL);
  gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), str);
  gtk_box_pack_start(GTK_BOX(vhbox), label, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, FALSE, TRUE, 0);

  return vbox;
}

static void bmcb_is_sensitive(GtkCellLayout *cl, GtkCellRenderer *r, GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
  gboolean sensitive;

  sensitive = !gtk_tree_model_iter_has_child (model, iter);
  g_object_set (r, "sensitive", sensitive, NULL);
}

void bmcb_changed_cb(GtkComboBox *button, gpointer data) {
  GtkTreeIter iter;
  GValue url = {0, }, title = {0, };
  GtkTreeModel *model = NULL;
  const gchar *szurl = NULL, *sztitle = NULL;

  if(gtk_combo_box_get_active_iter(button, &iter)) {
    DEBUG("get active iter ok\n");
    model = gtk_combo_box_get_model(button);
    gtk_tree_model_get_value(model, &iter, BM_URL_COL, &url);
    gtk_tree_model_get_value(model, &iter, BM_TITLE_COL, &title);
    DEBUG("GtkComboBox changed event, path = %s\n", gtk_tree_path_to_string(gtk_tree_model_get_path(model, &iter)));

    szurl = g_value_get_string(&url);
    sztitle = g_value_get_string(&title);
    if(szurl) {
      gtk_combo_box_set_active(GTK_COMBO_BOX(button), 0);
      DEBUG("goto %s, title = %s\n", szurl, sztitle);
    }
  
    g_value_unset(&url);
    g_value_unset(&title);
  }
}

static GtkWidget *create_web_ui(GtkWidget *window, GtkTreeModel *model) {
  GtkWidget *label = NULL, *vbox = NULL, *vhbox = NULL;
  GtkWidget *entry = NULL, *searchbtn = NULL, *gobtn = NULL, *bmcombo = NULL;
  GtkWidget *img = NULL;
  GtkCellRenderer *r = NULL;
  gchar str[1024] = {0};

  snprintf(str, 1024, H1_FONT_FORMAT, _("_Web"));
  vbox = gtk_vbox_new(FALSE, 6);
  vhbox = gtk_hbox_new(FALSE, 0);
  label = gtk_label_new(NULL);
  gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), str);
  gtk_box_pack_start(GTK_BOX(vhbox), label, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, FALSE, TRUE, 0);

  vhbox = gtk_hbox_new(FALSE, 0);
  entry = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(vhbox), entry, TRUE, TRUE, 0);
  
  img = gtk_image_new_from_stock(GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_SMALL_TOOLBAR);
  gobtn = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(gobtn), img);
  gtk_container_set_border_width(GTK_CONTAINER(gobtn), 0);
  gtk_box_pack_start(GTK_BOX(vhbox), gobtn, FALSE, FALSE, 0);
  searchbtn = gtk_button_new();
  img = gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_button_set_image(GTK_BUTTON(searchbtn), img);
  gtk_container_set_border_width(GTK_CONTAINER(searchbtn), 0);
  gtk_box_pack_start(GTK_BOX(vhbox), searchbtn, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, FALSE, TRUE, 0);

  vhbox = gtk_hbox_new(FALSE, 0);
  bmcombo = gtk_combo_box_new_with_model(model);
  gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(bmcombo), 1);
  gtk_combo_box_set_active(GTK_COMBO_BOX(bmcombo), 0);
  g_object_unref (model);
  gtk_box_pack_start(GTK_BOX(vhbox), bmcombo, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, FALSE, TRUE, 0);

  g_signal_connect(G_OBJECT(bmcombo), "changed", G_CALLBACK(bmcb_changed_cb), NULL);
  r = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (bmcombo), r, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (bmcombo), r, "pixbuf", BM_PIXBUF_COL, NULL);
  gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(bmcombo), r, bmcb_is_sensitive, NULL, NULL);
  r = gtk_cell_renderer_text_new();
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(bmcombo), r, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (bmcombo), r, "text", BM_TITLE_COL, NULL);
  gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(bmcombo), r, bmcb_is_sensitive, NULL, NULL);

  vhbox = gtk_hbox_new(FALSE, 0);
  label = gtk_label_new(NULL);
  bzero(str, 1024);
  snprintf(str, 1024, H3_FONT_FORMAT, _("Favorite Websites"));
  gtk_label_set_markup(GTK_LABEL(label), str);
  gtk_box_pack_start(GTK_BOX(vhbox), label, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, FALSE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(vbox), create_web_thumbnail_view(window), FALSE, TRUE, 0);

  return vbox;
}

static void music_ui_open_cb(GtkMenuItem *mi, gpointer data) {
}

static void music_ui_change_favorite_cb(GtkMenuItem *mi, gpointer data) {
}

static void music_ui_remove_favorite_cb(GtkMenuItem *mi, gpointer data) {
}

static gboolean music_ui_button_press_event_cb(GtkWidget *widget, GdkEventButton *e, gpointer data) {
  if(e->type == GDK_BUTTON_PRESS) {
    if(e->button == 3) {
      GtkWidget *menu = NULL, *menuitem = NULL;

      menu = gtk_menu_new();
      menuitem = gtk_menu_item_new_with_label(_("Open"));
      g_signal_connect (menuitem, "activate", G_CALLBACK (music_ui_open_cb), data);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

      menuitem = gtk_menu_item_new_with_label(_("Change Favorite"));
      g_signal_connect (menuitem, "activate", G_CALLBACK (music_ui_change_favorite_cb), data);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
      menuitem = gtk_menu_item_new_with_label(_("Remove Favorite"));
      g_signal_connect (menuitem, "activate", G_CALLBACK (music_ui_remove_favorite_cb), data);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

      gtk_widget_show_all(menu);
      gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, e->button, e->time);

      return TRUE;
    } else if(e->button == 1) {
      DEBUG("click on photo eventbox %x\n", widget);
    }
  } 

  return FALSE;
}

static GtkWidget *create_music_ui(GtkWidget *window) {
  GtkWidget *hbox = NULL;
  GtkWidget *ebox = NULL, *img = NULL;

  hbox = gtk_hbox_new(TRUE, 6);
  img = gtk_image_new_from_file("icons/music01.jpg");
  ebox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(ebox), img);
  g_signal_connect(G_OBJECT(ebox), "button_press_event", G_CALLBACK(music_ui_button_press_event_cb), img);
  gtk_container_set_border_width(GTK_CONTAINER(ebox), 0);
  gtk_box_pack_start(GTK_BOX(hbox), ebox, FALSE, FALSE, 0);

  img = gtk_image_new_from_file("icons/music02.jpg");
  ebox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(ebox), img);
  g_signal_connect(G_OBJECT(ebox), "button_press_event", G_CALLBACK(music_ui_button_press_event_cb), img);
  gtk_container_set_border_width(GTK_CONTAINER(ebox), 0);
  gtk_box_pack_start(GTK_BOX(hbox), ebox, FALSE, FALSE, 0);

  img = gtk_image_new_from_file("icons/music03.jpg");
  ebox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(ebox), img);
  g_signal_connect(G_OBJECT(ebox), "button_press_event", G_CALLBACK(music_ui_button_press_event_cb), img);
  gtk_container_set_border_width(GTK_CONTAINER(ebox), 0);
  gtk_box_pack_start(GTK_BOX(hbox), ebox, FALSE, FALSE, 0);

  return hbox;
}

static GtkWidget *create_media_ui(GtkWidget *window) {
  GtkWidget *label = NULL, *vbox = NULL, *vhbox = NULL, *vvbox = NULL;
  gchar str[1024] = {0};

  snprintf(str, 1024, H1_FONT_FORMAT, _("M_usic"));
  vbox = gtk_vbox_new(FALSE, 48);

  vvbox = gtk_vbox_new(FALSE, 12);
  vhbox = gtk_hbox_new(FALSE, 0);
  label = gtk_label_new(NULL);
  gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), str);
  gtk_box_pack_start(GTK_BOX(vhbox), label, FALSE, TRUE, 2);
  gtk_box_pack_start(GTK_BOX(vvbox), vhbox, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vvbox), create_music_ui(window), FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), vvbox, FALSE, TRUE, 0);

  vhbox = gtk_hbox_new(FALSE, 0);
  label = gtk_label_new(NULL);
  bzero(str, 1024);
  snprintf(str, 1024, H1_FONT_FORMAT, _("_Photos"));
  gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), str);
  gtk_box_pack_start(GTK_BOX(vhbox), label, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, FALSE, TRUE, 0);

  return vbox;
}

static GtkWidget *create_launcher_ui(GtkWidget *window, GtkTreeModel *model) {
  GtkWidget *hbox = NULL, *hhbox = NULL;

  hbox = gtk_hbox_new(FALSE, 0);
  hhbox= gtk_hbox_new(TRUE, 24);
  gtk_box_pack_start(GTK_BOX(hhbox), create_rss_ui(window), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hhbox), create_web_ui(window, model), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hhbox), create_media_ui(window), TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), hhbox, TRUE, TRUE, 16);

  return hbox;
}

static GtkWidget *create_time_stamp(GtkWidget *window) {
  GtkWidget *label = NULL;
  GTimeVal tv;
  gchar buff[1024] = {0}, str[1024] = {0};
  GDate *date = NULL;

  g_get_current_time(&tv);
  date = g_date_new();
  g_date_set_time_val(date, &tv);
  g_date_strftime(str, 1024, "%A, %B, %d", date);
  snprintf(buff, 1024, HDR_FONT_FORMAT, str);
  g_date_free(date);

  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), buff);

  return label;
}

#define SQLCMD_GET_TOPBM "select id, position, title from moz_bookmarks where parent=1"
#define SQLCMD_GET_DIRBM "select id, title from moz_bookmarks where type=2 and parent = %d"
#define SQLCMD_GET_URLBM "select moz_bookmarks.fk, moz_places.title, moz_places.url from moz_bookmarks, moz_places where parent = %d and moz_bookmarks.fk=moz_places.id"
#define SQLCMD_GET_FAVICONS "select moz_bookmarks.fk, moz_favicons.mime_type, moz_favicons.data from moz_bookmarks, moz_places, moz_favicons where moz_bookmarks.parent=%d and moz_places.id=moz_bookmarks.fk and moz_places.favicon_id = moz_favicons.id"

static void fetch_bookmarks_favicons(sqlite3 *db, GHashTable *ht) {
  gint parray[] = {2, 3, 5, -1};
  gchar sqlbuf[1024] = {0};
  sqlite3_stmt *stmt = NULL;
  const gchar *unused = NULL;
  GtkTreeIter iter;
  gint ret, idx = 0;

  do {
    snprintf(sqlbuf, 1024, SQLCMD_GET_FAVICONS, parray[idx]);
    ret = sqlite3_prepare_v2(db, sqlbuf, strlen(sqlbuf), &stmt, &unused);
    if(ret == SQLITE_OK) {
      do {
        ret = sqlite3_step(stmt);
        if(ret == SQLITE_ROW) {
          GdkPixbufLoader *pl = NULL;
          GdkPixbuf *pixbuf = NULL;
          GError *err = NULL;
          gint fk = sqlite3_column_int(stmt, 0);
          const guchar *mime_type = sqlite3_column_text(stmt, 1);
          const void *data = sqlite3_column_blob(stmt, 2);
          gint size = sqlite3_column_bytes(stmt, 2);

          pl = gdk_pixbuf_loader_new_with_mime_type(mime_type, &err);
          if(!gdk_pixbuf_loader_write(pl, data, size, &err)) {
            DEBUG("load favicon failed (fk=%x, data=%x, size=%d)\n", fk, data, size);
          } else {
            pixbuf = gdk_pixbuf_loader_get_pixbuf(pl);
            g_object_ref(pixbuf);
            g_hash_table_insert(ht, (gpointer)fk, (gpointer)pixbuf);
          }
          gdk_pixbuf_loader_close(pl, &err);
        }
      } while(ret == SQLITE_ROW);
      sqlite3_finalize(stmt);
    } else {
      DEBUG("compile SQLCMD %s failed.\n", sqlbuf);
    }
  } while(parray[++idx] > 0);
}

static void fill_urlbm_by_parent(sqlite3 *db, GtkTreeStore *store, gint parent, GtkTreeIter *pi) {
  gchar sqlbuf[1024] = {0};
  sqlite3_stmt *stmt = NULL;
  const gchar *unused = NULL;
  GtkTreeIter iter;
  gint ret;

  if(parent == 2) {
    snprintf(sqlbuf, 1024, SQLCMD_GET_URLBM" and position >1", parent);
  } else {
    snprintf(sqlbuf, 1024, SQLCMD_GET_URLBM, parent);
  }
  ret = sqlite3_prepare_v2(db, sqlbuf, strlen(sqlbuf), &stmt, &unused);
  if(ret == SQLITE_OK) {
    do {
      ret = sqlite3_step(stmt);
      if(ret == SQLITE_ROW) {
        GdkPixbuf *pixbuf = NULL;
        gint fk = sqlite3_column_int(stmt, 0);
        const guchar *text = sqlite3_column_text(stmt, 1);
        const guchar *url = sqlite3_column_text(stmt, 2);
        if(text && strlen(text) && url && strncmp(url, "place:", 6) != 0) {
          //DD("URL bookmark: moz_bookmarks.fk = %d, title = %s, url=%s.\n", fk, text, url);
          gtk_tree_store_append(store, &iter, pi);
          if(! (pixbuf = g_hash_table_lookup(g_bmht, (gpointer)fk))) {
            pixbuf = g_bmpixbuf[BM_FAVICON_PIXBUF];
            //DD("Load favicon for URL %s failed, default icon used\n", url);
          }
          gtk_tree_store_set(store, &iter, BM_PIXBUF_COL, pixbuf, BM_TITLE_COL, text, BM_URL_COL, url, -1);
        }
      }
    } while(ret == SQLITE_ROW);
    sqlite3_finalize(stmt);
  } else {
    DEBUG("compile SQLCMD %s failed.\n", sqlbuf);
  }
}

static void fill_dirbm_by_parent(sqlite3 *db, GtkTreeStore *store, gint parent, GtkTreeIter *pi) {
  gchar sqlbuf[1024] = {0};
  sqlite3_stmt *stmt = NULL;
  const gchar *unused = NULL;
  GtkTreeIter iter;
  gint ret;

  snprintf(sqlbuf, 1024, SQLCMD_GET_DIRBM, parent);
  ret = sqlite3_prepare_v2(db, sqlbuf, strlen(sqlbuf), &stmt, &unused);
  if(ret == SQLITE_OK) {
    do {
      ret = sqlite3_step(stmt);
      if(ret == SQLITE_ROW) {
        gint id = sqlite3_column_int(stmt, 0);
        const guchar *text = sqlite3_column_text(stmt, 1);
        if(text && strlen(text)) {
          //DD("directory bookmark: id = %d, title = %s.\n", id, text);
          gtk_tree_store_append(store, &iter, pi);
          if(parent == 4)
            gtk_tree_store_set(store, &iter, BM_PIXBUF_COL, g_bmpixbuf[BM_TAG_PIXBUF], BM_TITLE_COL, text, -1);
          else
            gtk_tree_store_set(store, &iter, BM_PIXBUF_COL, g_bmpixbuf[BM_DIRECTORY_PIXBUF], BM_TITLE_COL, text, -1);
          fill_dirbm_by_parent(db, store, id, &iter);
          fill_urlbm_by_parent(db, store, id, &iter);
        }
      }
    } while(ret == SQLITE_ROW);
    sqlite3_finalize(stmt);
  } else {
    DEBUG("compile SQLCMD %s failed.\n", sqlbuf);
  }
}

static GtkTreeModel *create_stock_from_database(sqlite3 *db) {
  GtkTreeStore *store = NULL;
  GtkTreeIter iter;
  gint ret;
  sqlite3_stmt *stmt = NULL;
  const gchar *unused = NULL;

  store = gtk_tree_store_new(BM_NUM_COL, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

  gtk_tree_store_append(store, &iter, NULL);
  gtk_tree_store_set(store, &iter, BM_TITLE_COL, _("Bookmarks"), BM_URL_COL, NULL, -1);

  ret = sqlite3_prepare_v2(db, SQLCMD_GET_TOPBM, strlen(SQLCMD_GET_TOPBM), &stmt, &unused);
  if(ret == SQLITE_OK) {
    do {
      ret = sqlite3_step(stmt);
      if(ret == SQLITE_ROW) {
        gint id  = sqlite3_column_int(stmt, 0), num;
        gint pos = sqlite3_column_int(stmt, 1);
        const guchar *text = sqlite3_column_text(stmt, 2);
        //DD("id = %d, position = %d, text = %s, len = %d.\n", id, pos, text, strlen(text));
        if(text && strlen(text)) {
          gtk_tree_store_append(store, &iter, NULL);
          gtk_tree_store_set(store, &iter, BM_PIXBUF_COL, g_bmpixbuf[pos], BM_TITLE_COL, text, -1);
          fill_dirbm_by_parent(db, store, id, &iter);
          fill_urlbm_by_parent(db, store, id, &iter);
        }
      }
    } while(ret == SQLITE_ROW);
    sqlite3_finalize(stmt);
  } else {
    DEBUG("exec SQL command %s failed (%s).\n", SQLCMD_GET_TOPBM, sqlite3_errmsg(db));
  }

  return GTK_TREE_MODEL(store);
}

static void add_columns(GtkTreeView *treeview, gpointer data) {
  gint col_offset;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);

  column = gtk_tree_view_column_new ();

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer, "pixbuf", BM_PIXBUF_COL, NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer, "text", BM_TITLE_COL, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);
}

static gboolean bookmark_treeview_dbclicked(GtkWidget *widget, GdkEventButton *e, gpointer data) {
  if(e->type == GDK_2BUTTON_PRESS) {
    gtk_dialog_response(GTK_DIALOG(data), GTK_RESPONSE_ACCEPT);
    return TRUE;
  }

  return FALSE;
}

void show_bookmark_dialog(gpointer data) {
  GtkWidget *dlg = NULL;
  GtkWidget *vhbox = NULL, *label = NULL, *entry = NULL;
  GtkWidget *sw = NULL, *treeview = NULL;
  gint resp;
  GtkTreeModel *model;

  g_bookmark_dialog = dlg = gtk_dialog_new_with_buttons(_("Change Website"), GTK_WINDOW(g_main_window), GTK_DIALOG_MODAL|GTK_DIALOG_NO_SEPARATOR, NULL);

  model = g_object_get_data(G_OBJECT(g_main_window), "model");
  gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_ACCEPT);
  gtk_widget_set_size_request(dlg, 280, 480);

  entry = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), entry, FALSE, TRUE, 2);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  treeview = gtk_tree_view_new_with_model (model);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)), GTK_SELECTION_SINGLE);
  add_columns(GTK_TREE_VIEW(treeview), data);
  gtk_tree_view_set_search_entry(GTK_TREE_VIEW(treeview), GTK_ENTRY(entry));
  gtk_tree_view_set_enable_search(GTK_TREE_VIEW(treeview), TRUE);
  gtk_container_add (GTK_CONTAINER (sw), treeview);
  g_signal_connect(G_OBJECT(treeview), "button_press_event", G_CALLBACK(bookmark_treeview_dbclicked), dlg);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), sw, TRUE, TRUE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(dlg), 2);
  gtk_widget_set_size_request(GTK_DIALOG(dlg)->action_area, 0, 0);

  gtk_widget_show_all(dlg);
  resp = gtk_dialog_run(GTK_DIALOG(dlg));
  if(resp == GTK_RESPONSE_ACCEPT) {
    gchar *url = NULL;
    GtkTreePath *_path = NULL;
    GtkTreeViewColumn *_column = NULL;
    GtkTreeIter iter;
    GValue val = {0, };

    if(url = g_object_get_data(G_OBJECT(data), "url"))
      g_free(url);

    gtk_tree_view_get_cursor(GTK_TREE_VIEW(treeview), &_path, &_column);
    if(_path && gtk_tree_model_get_iter(model, &iter, _path)) {
      //DEBUG("path = %x, column = %x\n", _path, _column);
      gtk_tree_model_get_value(model, &iter, BM_URL_COL, &val);
      url = strdup(g_value_get_string(&val));
      g_value_unset(&val);
    } else {
      url = NULL;
    }
    g_object_set_data(G_OBJECT(data), "url", url);
    DEBUG("new localtion URL %s\n", url);
    if(_path) gtk_tree_path_free(_path);
  }
  gtk_widget_destroy(dlg);
}

static void ht_value_destroy_cb(gpointer data) {
  if(data)
    g_object_unref(data);
}

int main(int argc, char *argv[]) {
  GtkWidget *window = NULL, *vbox = NULL;
  gchar buff[1024] = {0};
  gchar *username = "DeviceVM";
  GError *err = NULL;
  sqlite3 *bmDB = NULL;
  gint ret, idx;
  GtkTreeModel *model = NULL;

  DEBUG("BUILD TIMESTAMP @ "__TIME__", "__DATE__"\n");
  bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
  textdomain(GETTEXT_PACKAGE);

  ret = sqlite3_open_v2(BOOKMARK_SQLITE3_DBFILE, &bmDB, SQLITE_OPEN_NOMUTEX|SQLITE_OPEN_READONLY, NULL);
  if(ret != SQLITE_OK) {
    DEBUG("open bookmark database failed (%s).\n", sqlite3_errmsg(bmDB));
    sqlite3_close(bmDB);
    bmDB = NULL;
  }

  gtk_init(&argc, &argv);

  g_background_pixbuf = gdk_pixbuf_new_from_inline(-1, background_pixbuf, FALSE, &err);
  g_bmpixbuf[BM_TAG_PIXBUF] = gdk_pixbuf_new_from_inline(-1, bm_tag_pixbuf, FALSE, &err);
  g_bmpixbuf[BM_DIRECTORY_PIXBUF] = gdk_pixbuf_new_from_inline(-1, bm_directory_pixbuf, FALSE, &err);
  g_bmpixbuf[BM_MENU_PIXBUF] = gdk_pixbuf_new_from_inline(-1, bm_menu_pixbuf, FALSE, &err);
  g_bmpixbuf[BM_TOOLBAR_PIXBUF] = gdk_pixbuf_new_from_inline(-1, bm_toolbar_pixbuf, FALSE, &err);
  g_bmpixbuf[BM_UNSORTED_PIXBUF] = gdk_pixbuf_new_from_inline(-1, bm_unsorted_pixbuf, FALSE, &err);
  g_bmpixbuf[BM_FAVICON_PIXBUF] = gdk_pixbuf_new_from_inline(-1, bm_favicon_pixbuf, FALSE, &err);

  g_bmht = g_hash_table_new_full(NULL, NULL, NULL, ht_value_destroy_cb);
  if(bmDB) {
    fetch_bookmarks_favicons(bmDB, g_bmht);
    model = create_stock_from_database(bmDB);
    sqlite3_close(bmDB);
  }

  g_main_window = window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_app_paintable(window, TRUE);
  snprintf(buff, 1024, "%s, %s", _("Welcome"), username);
  gtk_window_set_title(GTK_WINDOW(window), buff);
  gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
  gtk_widget_set_size_request(window, 1024, 576);
  gtk_container_set_border_width(GTK_CONTAINER(window), 2);

  g_signal_connect(G_OBJECT(window), "expose-event", G_CALLBACK(main_window_draw_background), NULL);
  g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(main_window_close), NULL);

  vbox = gtk_vbox_new(FALSE, 40);
  gtk_box_pack_start(GTK_BOX(vbox), create_time_stamp(window), FALSE, TRUE, 6);
  gtk_box_pack_start(GTK_BOX(vbox), create_launcher_ui(window, model), TRUE, TRUE, 0);
  g_object_set_data(G_OBJECT(g_main_window), "model", model);

  gtk_container_add(GTK_CONTAINER(window), vbox);

  gtk_widget_show_all(window);
  gtk_main();

  g_object_unref(g_background_pixbuf);
  for (idx=0; idx < BM_MAX_PIXBUF; idx++) {
    g_object_unref(g_bmpixbuf[idx]);
  }
  if(g_bmht)
    g_hash_table_destroy(g_bmht);
}
