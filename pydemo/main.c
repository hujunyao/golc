#include "launcher.h"
#include "_inline_pixbuf.h"
#include <sys/wait.h>

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
  WEB_BG_PIXBUF,
  MEDIA_BG_PIXBUF,
  BM_MAX_PIXBUF
};

typedef struct {
  gchar *path;
  GdkPixbuf *pixbuf;
  gint w, h;
} photo_item_t;

typedef struct {
  GtkWidget *ebox[3];
  gint alpha, idx;
  GSList *pl[3], *curr[3];
  photo_item_t *pi;
} photo_ui_t;

static GdkPixbuf  *g_bmpixbuf[BM_MAX_PIXBUF];
static GtkWidget  *g_main_window = NULL;
static GHashTable *g_bmht = NULL;
static photo_ui_t  g_pu = {0};

static void show_selection_dialog(gpointer ebox, gint type, GtkTreeModel *model);
static gboolean launch_app_async(gchar *argv[], GPid *pid, gint flag);
static GtkWidget *create_media_ui(GtkWidget *window);
static GtkTreeModel *create_model_for_music(gchar *prefix_path);
static GtkTreeModel *create_model_for_photo(gchar *prefix_path);

static void free_attach_object(gpointer data) {
  //g_object_unref(data);
}

static void free_attach_memory(gpointer data) {
  //g_free(data);
}

static void search_button_clicked(GtkButton *button, gpointer data) {
  gchar *str = NULL;
  gchar url[1024] = {0};
  gchar *argv[] = {"firefox-skype", NULL, NULL};

  str = g_uri_escape_string(gtk_entry_get_text(GTK_ENTRY(data)), NULL, TRUE);
  snprintf(url, 1024, "http://www.google.com/search?hl=en&q=%s&btnG=Google+Search&aq=f&oq=", str);
  g_free(str);

  argv[1] = (gchar *)url;
  launch_app_async(argv, NULL, 0);
}

static void go_button_clicked(GtkButton *button, gpointer data) {
  const gchar *url= NULL;
  gchar *argv[] = {"firefox-skype", NULL, NULL};

  url = gtk_entry_get_text(GTK_ENTRY(data));
  argv[1] = (gchar *)url;

  launch_app_async(argv, NULL, 0);
}

static gboolean photo_ui_animation_cb(gpointer data) {
  photo_ui_t *pu = (photo_ui_t *)data;
  GtkWidget *ebox = NULL;
  gint idx = 0;
  gchar *path = NULL;
  gboolean ret = TRUE;

  idx = pu->idx;
  ebox = pu->ebox[idx];
  pu->alpha += 64;

  if(pu->alpha > 255) {
    pu->alpha = 255;
    ret = FALSE;
  }

  //DEBUG("composite pixbuf, idx = %d, alpha = %d\n", idx, pu->alpha);
  gtk_widget_queue_draw(GTK_WIDGET(ebox));

  return ret;
}

static gboolean photo_ui_switch_photo_cb(gpointer data) {
  photo_ui_t *pu = (photo_ui_t *)data;
  GSList *curr = NULL;
  gint idx;

  pu->alpha = 32;
  idx = pu->idx;
  curr = g_slist_next(pu->curr[idx]);
  if(curr == NULL)
    curr = pu->pl[idx];

  if(curr) {
    photo_item_t *pi = NULL;
    pi = (photo_item_t *)(curr->data);
    //DEBUG("pi = %x, idx = %d, pi->path = %s, pi->pixbuf = %x\n", pi, idx, pi->path, pi->pixbuf);
    if(pi->path && pi->pixbuf == NULL) {
      GError *err = NULL;

      if(pi->pixbuf = gdk_pixbuf_new_from_file_at_scale(pi->path, MEDIAUI_WIDTH-2, MEDIAUI_HEIGHT-2, TRUE, &err)) {
        pi->w = gdk_pixbuf_get_width(pi->pixbuf); pi->h = gdk_pixbuf_get_height(pi->pixbuf);
      }
      //DEBUG("new thumbnail photo %s, pixbuf = %x\n", pi->path, pi->pixbuf);
    }
    pu->pi = pi;
  }
  pu->curr[idx] = curr;

  g_timeout_add(160, photo_ui_animation_cb, data);
  return TRUE;
}

static gboolean launch_app_async(gchar *argv[], GPid *pid, gint flag) {
  gboolean ret;
  GError *err = NULL;

  ret = g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH|flag, NULL, NULL, pid, &err);
  if(ret == FALSE) {
    ERROR("**LAUNCH APP %s %s failed**\n", argv[0], argv[1]);
  }

  return ret;
}

static void web_photo_watch_cb(GPid pid, gint status, gpointer data) {
  DEBUG("status = %x\n", status);
  if(WIFEXITED(status)) {
    gchar path[1024] = {0};
    GError *err = NULL;
    GdkPixbuf *pixbuf = NULL;
    gchar *url = NULL;

    DEBUG("gnome-web-photo exit normally, exit status = %d.\n", WEXITSTATUS(status));
    snprintf(path, 1024, "/tmp/%x.png", data);
    url = g_object_get_data(G_OBJECT(data), "url");
    pixbuf = gdk_pixbuf_new_from_file_at_scale(path, WEBUI_WIDTH-2, WEBUI_HEIGHT-2, FALSE, &err);
    if(pixbuf) {
      DEBUG("generate web photo for website %s success.\n", url);
      g_object_set_data_full(G_OBJECT(data), "icon", pixbuf, free_attach_object);
      gtk_widget_queue_draw(GTK_WIDGET(data));
    } else {
      ERROR("generate web photo for website %s failed.\n", url);
      g_object_set_data_full(G_OBJECT(data), "icon", NULL, NULL);
      g_object_set_data_full(G_OBJECT(data), "url", NULL, NULL);
    }
  } else {
    ERROR("gnome-web-photo exit abnormally.\n");
  }
}

static void generate_photo_photo(gchar *url, gpointer ebox) {
  GdkPixbuf *pixbuf = NULL;
  GError *err = NULL;

  pixbuf = gdk_pixbuf_new_from_file_at_scale(url, MEDIAUI_WIDTH-2, MEDIAUI_HEIGHT-2, TRUE, &err);
  g_object_set_data_full(G_OBJECT(ebox), "icon", pixbuf, NULL);
  gtk_widget_queue_draw(GTK_WIDGET(ebox));
}

static void generate_music_photo(gchar *url, gpointer ebox) {
  gchar path[1024] = {0};
  char *ptr = NULL;
  GdkPixbuf *pixbuf = NULL;
  GError *err = NULL;

  ptr = strrchr(url, '/');
  if(ptr) {
    strncpy(path, url, ptr-url);
    g_strlcat(path, "/logo.jpg", 1024);
    pixbuf = gdk_pixbuf_new_from_file_at_scale(path, MEDIAUI_WIDTH-2, MEDIAUI_HEIGHT-2, FALSE, &err);
    g_object_set_data_full(G_OBJECT(ebox), "icon", pixbuf, NULL);
    gtk_widget_queue_draw(GTK_WIDGET(ebox));
  }
}

static void generate_web_photo(gchar *url, gpointer ebox) {
  GPid pid;
  gchar path[1024] = {0};
  gchar *argv[] = {"gnome-web-photo", "--mode", "thumbnail", "--size", "256", NULL, NULL, NULL};
  snprintf(path, 1024, "/tmp/%x.png", ebox);
  argv[5] = url, argv[6] = path;

  if(launch_app_async(argv, &pid, G_SPAWN_DO_NOT_REAP_CHILD)) {
    g_child_watch_add(pid, web_photo_watch_cb, ebox);
  }
}

static void ebox_set_data(GtkWidget *ebox, gchar *url, GdkPixbuf *pixbuf, GtkTreeModel *model) {
  g_object_set_data_full(G_OBJECT(ebox), "url", url, NULL);
  g_object_set_data_full(G_OBJECT(ebox), "icon", pixbuf, NULL);
  g_object_set_data(G_OBJECT(ebox), "model", model);
}

static gboolean ebox_draw_background(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
  gint width = widget->allocation.width, height = widget->allocation.height;
  gint type = (gint)data;
  gint x = 2, y = 2, w = width - 4, h = height - 4;
  GdkPixbuf *icon = g_object_get_data(G_OBJECT(widget), "icon");
  GdkPixbuf *background = NULL;

  switch(type) {
  case EBOX_WEB_UI:
    background = g_bmpixbuf[WEB_BG_PIXBUF];
    //DEBUG("EBOX_WEB_UI width = %d, height = %d\n", width, height);
  break;
  case EBOX_PHOTO_UI:
    background = g_bmpixbuf[MEDIA_BG_PIXBUF];
    if(icon) {
      gint ph = gdk_pixbuf_get_height(icon), pw = gdk_pixbuf_get_width(icon);
      if(ph < (MEDIAUI_HEIGHT-2)) {
        y = (height - ph) >> 1;
        h = ph;
      } else if(pw < (MEDIAUI_WIDTH-2)) {
        x = (width - pw) >> 1;
        w = pw;
      }
    }
    //DEBUG("EBOX_PHOTO_UI x=%d, y=%d, w=%d, h=%d, width = %d, height = %d\n", x, y, w, h, width, height);
  break;
  case EBOX_MUSIC_UI:
    //DEBUG("EBOX_MUSIC_UI width = %d, height = %d\n", width, height);
    background = g_bmpixbuf[MEDIA_BG_PIXBUF];
  break;
  default:
    DEBUG("default handle of ebox_draw_background.\n");
  break;
  }

  if(background)
    gdk_draw_pixbuf(widget->window, NULL, background, 0, 0, -1, -1, width+1, height+1, GDK_RGB_DITHER_NORMAL, 0, 0);
  if(icon) {
    if(type == EBOX_PHOTO_UI) {
      if(g_pu.pi) {
        GdkPixbuf *new = NULL;
        new = gdk_pixbuf_copy(icon);
        gdk_pixbuf_composite(g_pu.pi->pixbuf, new, 0, 0, w, h, 0, 0, 1.0, 1.0, GDK_INTERP_NEAREST, g_pu.alpha);
        gdk_draw_pixbuf(widget->window, NULL, new, 0, 0, x, y, w, h, GDK_RGB_DITHER_NORMAL, 0, 0);
        g_object_unref(new);
        if(g_pu.alpha >= 255) {
          g_object_set_data(G_OBJECT(widget), "icon", g_object_ref(g_pu.pi->pixbuf));
          g_object_set_data(G_OBJECT(widget), "url", strdup(g_pu.pi->path));
          g_pu.pi = NULL;
          g_pu.alpha = 0;

          g_pu.idx ++;
          if(g_pu.idx == 3)
            g_pu.idx = 0;
        }
      } else {
        gdk_draw_pixbuf(widget->window, NULL, icon, 0, 0, x, y, w, h, GDK_RGB_DITHER_NORMAL, 0, 0);
      }
    } else {
      gdk_draw_pixbuf(widget->window, NULL, icon, 0, 0, x, y, w, h, GDK_RGB_DITHER_NORMAL, 0, 0);
    }
  }

  return TRUE;
}

static gboolean main_window_draw_background(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
  gint width = widget->allocation.width, height = widget->allocation.height;

  gdk_draw_pixbuf(widget->window, NULL, data, 0, 0, 0, 0, width, height, GDK_RGB_DITHER_NORMAL, 0, 0);

  return FALSE;
}

static gboolean main_window_close(GtkWidget *widget, GdkEvent *event, gpointer data) {
  DEBUG("close main window\n");
  gtk_main_quit();

  return FALSE;
}

static void popmenu_open_cb(GtkMenuItem *mi, gpointer ebox) {
  GdkPixbuf *icon = g_object_get_data(G_OBJECT(ebox), "icon");
  gchar *url = g_object_get_data(G_OBJECT(ebox), "url");
  gint  type = (gint)g_object_get_data(G_OBJECT(mi), "uitype");
  gchar *argv[] = {NULL, NULL, NULL};

  switch(type) {
  case EBOX_WEB_UI:
    argv[0] = "firefox-skype";
  break;
  case EBOX_PHOTO_UI:
    argv[0] = "photo-viewer";
  break;
  case EBOX_MUSIC_UI:
    argv[0] = "music-player";
  break;
  default:
    argv[0] = "none-app";
  break;
  }
  if(icon) {
    argv[1] = url;
    launch_app_async(argv, NULL, 0);
  }
}

static void popmenu_change_favorite_cb(GtkMenuItem *mi, gpointer ebox) {
  gint type = (gint)g_object_get_data(G_OBJECT(mi), "uitype");
  GtkTreeModel *model = NULL;

  if(type == EBOX_MUSIC_UI) {
    model = create_model_for_music(getenv("MUSICPREFIX"));
  } else if (type == EBOX_PHOTO_UI) {
    model = create_model_for_photo(getenv("PHOTOPREFIX"));
  }

  show_selection_dialog(ebox, type, model);
}

static void popmenu_remove_favorite_cb(GtkMenuItem *mi, gpointer data) {
  gchar *url = NULL;
  GdkPixbuf *icon = NULL;

  url = g_object_get_data(data, "url");
  if(url)
    g_free(url);

  icon = g_object_get_data(data, "icon");
  if(icon)
    g_object_unref(G_OBJECT(icon));

  g_object_set_data(data, "url", NULL);
  g_object_set_data(data, "icon", NULL);
  gtk_widget_queue_draw(GTK_WIDGET(data));
}

static void web_ui_new_bookmark_cb(GtkMenuItem *mi, gpointer data) {
  GtkWidget *dlg = NULL;
  GtkWidget *vhbox = NULL, *label = NULL, *entry = NULL;
  gchar markup[1024] = {0};
  gint resp;
  gchar *url = NULL;
  GdkPixbuf *pixbuf = NULL;

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
    if(url = g_object_get_data(G_OBJECT(data), "url"))
      g_free(url);
    if(pixbuf = g_object_get_data(G_OBJECT(data), "icon")) {
      g_object_unref(pixbuf);
      g_object_set_data(G_OBJECT(data), "icon", NULL);
      gtk_widget_queue_draw(GTK_WIDGET(data));
    }

    url = strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    g_object_set_data_full(G_OBJECT(data), "url", url, free_attach_memory);
    DEBUG("new localtion URL %s\n", url);
  }
  gtk_widget_destroy(dlg);

  /**do generate web thumbnail here*/
  if(url) {
    generate_web_photo(url, data);
  }
}

static gboolean ebox_button_press_event_cb(GtkWidget *widget, GdkEventButton *e, gpointer data) {
  if(e->type == GDK_BUTTON_PRESS) {
    gint type = (gint)data;
    GdkPixbuf *icon = g_object_get_data(G_OBJECT(widget), "icon");

    if(e->button == 3) {
      GtkWidget *menu = NULL, *menuitem = NULL;

      menu = gtk_menu_new();
      menuitem = gtk_menu_item_new_with_label(_("Open"));
      if(icon == NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(menuitem), FALSE);
      g_signal_connect (menuitem, "activate", G_CALLBACK (popmenu_open_cb), widget);
      g_object_set_data(G_OBJECT(menuitem), "uitype", data);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

      menuitem = gtk_menu_item_new_with_label(_("Change Favorite"));
      g_signal_connect (menuitem, "activate", G_CALLBACK (popmenu_change_favorite_cb), widget);
      g_object_set_data(G_OBJECT(menuitem), "uitype", data);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
      menuitem = gtk_menu_item_new_with_label(_("Remove Favorite"));
      g_signal_connect (menuitem, "activate", G_CALLBACK (popmenu_remove_favorite_cb), widget);
      g_object_set_data(G_OBJECT(menuitem), "uitype", data);
      if(icon == NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(menuitem), FALSE);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

      if(type == EBOX_WEB_UI) {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        menuitem = gtk_menu_item_new_with_label(_("New Bookmark..."));
        g_signal_connect (menuitem, "activate", G_CALLBACK (web_ui_new_bookmark_cb), widget);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
      }

      gtk_widget_show_all(menu);
      gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, e->button, e->time);

      return TRUE;
    } else if(e->button == 1) {
      gchar *url = g_object_get_data(G_OBJECT(widget), "url");
      gchar *argv[] = {NULL, NULL, NULL};

      switch(type) {
      case EBOX_WEB_UI:
        argv[0] = "firefox-skype";
      break;
      case EBOX_PHOTO_UI:
        argv[0] = "photo-viewer";
      break;
      case EBOX_MUSIC_UI:
        argv[0] = "music-player";
      break;
      default:
        argv[0] = "none-app";
      break;
      }
      if(icon) {
        argv[1] = url;
        launch_app_async(argv, NULL, 0);
      }
    }
  }

  return FALSE;
}

static GtkWidget *create_web_thumbnail_view(GtkWidget *window, GtkTreeModel *model) {
  GtkWidget *btn = NULL, *vbox = NULL, *vhbox = NULL;
  GtkWidget *ebox = NULL;
  gchar *prefix = getenv("ICONSPREFIX");
  gchar path[1024] = {0};
  GdkPixbuf *pixbuf = NULL;
  GError *err = NULL;

  if(!prefix) prefix= ICONS_PATH;

  vbox = gtk_vbox_new(FALSE, 12);
  vhbox = gtk_hbox_new(FALSE, 12);
  
  snprintf(path, 1024, "%s/yahoo_128x128.png", prefix);
  ebox = gtk_event_box_new();
  pixbuf = gdk_pixbuf_new_from_file_at_scale(path, WEBUI_WIDTH-2, WEBUI_HEIGHT-2, FALSE, &err);
  ebox_set_data(ebox, strdup("www.yahoo.com"), pixbuf, model);
  gtk_widget_set_size_request(ebox, WEBUI_WIDTH, WEBUI_HEIGHT);
  g_signal_connect(G_OBJECT(ebox), "button_press_event", G_CALLBACK(ebox_button_press_event_cb), (gpointer)EBOX_WEB_UI);
  g_signal_connect(G_OBJECT(ebox), "expose-event", G_CALLBACK(ebox_draw_background), (gpointer)EBOX_WEB_UI);
  gtk_box_pack_start(GTK_BOX(vhbox), ebox, FALSE, FALSE, 0);

  bzero(path, 1024), err = NULL;
  snprintf(path, 1024, "%s/163_128x128.png", prefix);
  ebox = gtk_event_box_new();
  pixbuf = gdk_pixbuf_new_from_file_at_scale(path, WEBUI_WIDTH-2, WEBUI_HEIGHT-2, FALSE, &err);
  ebox_set_data(ebox, strdup("www.163.com"), pixbuf, model);
  gtk_widget_set_size_request(ebox, WEBUI_WIDTH, WEBUI_HEIGHT);
  g_signal_connect(G_OBJECT(ebox), "button_press_event", G_CALLBACK(ebox_button_press_event_cb), (gpointer)EBOX_WEB_UI);
  g_signal_connect(G_OBJECT(ebox), "expose-event", G_CALLBACK(ebox_draw_background), (gpointer)EBOX_WEB_UI);
  gtk_box_pack_start(GTK_BOX(vhbox), ebox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, FALSE, FALSE, 0);

  vhbox = gtk_hbox_new(FALSE, 12);
  bzero(path, 1024), err = NULL;
  snprintf(path, 1024, "%s/sina_128x128.png", prefix);
  ebox = gtk_event_box_new();
  pixbuf = gdk_pixbuf_new_from_file_at_scale(path, WEBUI_WIDTH-2, WEBUI_HEIGHT-2, FALSE, &err);
  ebox_set_data(ebox, strdup("news.sina.com.cn"), pixbuf, model);
  gtk_widget_set_size_request(ebox, WEBUI_WIDTH, WEBUI_HEIGHT);
  g_signal_connect(G_OBJECT(ebox), "button_press_event", G_CALLBACK(ebox_button_press_event_cb), (gpointer)EBOX_WEB_UI);
  g_signal_connect(G_OBJECT(ebox), "expose-event", G_CALLBACK(ebox_draw_background), (gpointer)EBOX_WEB_UI);
  gtk_box_pack_start(GTK_BOX(vhbox), ebox, FALSE, FALSE, 0);

  bzero(path, 1024), err = NULL;
  snprintf(path, 1024, "%s/google_128x128.png", prefix);
  ebox = gtk_event_box_new();
  pixbuf = gdk_pixbuf_new_from_file_at_scale(path, WEBUI_WIDTH-2, WEBUI_HEIGHT-2, FALSE, &err);
  ebox_set_data(ebox, strdup("www.google.cn"), pixbuf, model);
  gtk_widget_set_size_request(ebox, WEBUI_WIDTH, WEBUI_HEIGHT);
  g_signal_connect(G_OBJECT(ebox), "button_press_event", G_CALLBACK(ebox_button_press_event_cb), (gpointer)EBOX_WEB_UI);
  g_signal_connect(G_OBJECT(ebox), "expose-event", G_CALLBACK(ebox_draw_background), (gpointer)EBOX_WEB_UI);
  gtk_box_pack_start(GTK_BOX(vhbox), ebox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, FALSE, FALSE, 0);

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
      gchar *argv[] = {"firefox-skype", NULL, NULL};
      argv[1] = (gchar *)szurl;
    
      gtk_combo_box_set_active(GTK_COMBO_BOX(button), 0);
      launch_app_async(argv, NULL, 0);
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
  g_signal_connect(G_OBJECT(gobtn), "clicked", G_CALLBACK(go_button_clicked), entry);
  gtk_button_set_image(GTK_BUTTON(gobtn), img);
  gtk_container_set_border_width(GTK_CONTAINER(gobtn), 0);
  gtk_box_pack_start(GTK_BOX(vhbox), gobtn, FALSE, FALSE, 0);
  searchbtn = gtk_button_new();
  g_signal_connect(G_OBJECT(searchbtn), "clicked", G_CALLBACK(search_button_clicked), entry);
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

  gtk_box_pack_start(GTK_BOX(vbox), create_web_thumbnail_view(window, model), FALSE, TRUE, 0);

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
            ERROR("load favicon failed (fk=%x, data=%x, size=%d)\n", fk, data, size);
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
      ERROR("exec SQL command %s failed.\n", sqlbuf);
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
    ERROR("exec SQL command %s failed.\n", sqlbuf);
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
    ERROR("exec SQL command %s failed (%s).\n", sqlbuf, sqlite3_errmsg(db));
  }
}

static GtkTreeModel *create_model_from_database(sqlite3 *db) {
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
    ERROR("exec SQL command %s failed (%s).\n", SQLCMD_GET_TOPBM, sqlite3_errmsg(db));
  }

  return GTK_TREE_MODEL(store);
}

static void add_columns(GtkTreeView *treeview, gpointer ebox) {
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
    const gchar *url = NULL;
    GtkTreePath *_path = NULL;
    GtkTreeModel *model = NULL;
    GtkTreeViewColumn *_column = NULL;
    GtkTreeIter iter;
    GValue val = {0, };

    model =  gtk_tree_view_get_model (GTK_TREE_VIEW(widget));
    gtk_tree_view_get_cursor(GTK_TREE_VIEW(widget), &_path, &_column);
    //DEBUG("model= %x, path = %x, column = %x\n", model, _path, _column);
    if(_path && gtk_tree_model_get_iter(model, &iter, _path)) {
      //DEBUG("path = %x, column = %x\n", _path, _column);
      gtk_tree_model_get_value(model, &iter, BM_URL_COL, &val);
      url = g_value_get_string(&val);
      //DEBUG("new url = %s\n", url);
      g_value_unset(&val);
    } else {
      url = NULL;
    }

    if(url)
      gtk_dialog_response(GTK_DIALOG(data), GTK_RESPONSE_ACCEPT);
    return TRUE;
  }

  return FALSE;
}

void show_selection_dialog(gpointer ebox, gint type, GtkTreeModel *_model) {
  GtkWidget *dlg = NULL;
  GtkWidget *vhbox = NULL, *label = NULL, *entry = NULL;
  GtkWidget *sw = NULL, *treeview = NULL;
  gint resp;
  GtkTreeModel *model;
  gchar *url = NULL;

  dlg = gtk_dialog_new_with_buttons(_("Change Website"), GTK_WINDOW(g_main_window), GTK_DIALOG_MODAL|GTK_DIALOG_NO_SEPARATOR, NULL);

  if(_model == NULL)
    model = g_object_get_data(G_OBJECT(ebox), "model");
  else
    model = _model;

  gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_ACCEPT);
  gtk_widget_set_size_request(dlg, 280, 480);

  entry = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), entry, FALSE, TRUE, 2);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  treeview = gtk_tree_view_new_with_model (model);
  if(_model) g_object_unref(_model);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)), GTK_SELECTION_SINGLE);
  add_columns(GTK_TREE_VIEW(treeview), ebox);
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
    GtkTreePath *_path = NULL;
    GtkTreeViewColumn *_column = NULL;
    GtkTreeIter iter;
    GValue val = {0, };
    GdkPixbuf *pixbuf = NULL;

    if(url = g_object_get_data(G_OBJECT(ebox), "url"))
      g_free(url);
    if(pixbuf = g_object_get_data(G_OBJECT(ebox), "icon")) {
      g_object_unref(pixbuf);
      g_object_set_data(G_OBJECT(ebox), "icon", NULL);
      gtk_widget_queue_draw(GTK_WIDGET(ebox));
    }

    gtk_tree_view_get_cursor(GTK_TREE_VIEW(treeview), &_path, &_column);
    if(_path && gtk_tree_model_get_iter(model, &iter, _path)) {
      //DEBUG("path = %x, column = %x\n", _path, _column);
      gtk_tree_model_get_value(model, &iter, BM_URL_COL, &val);
      url = strdup(g_value_get_string(&val));
      g_value_unset(&val);
    } else {
      url = NULL;
    }
    g_object_set_data_full(G_OBJECT(ebox), "url", url, free_attach_memory);
    DEBUG("new localtion URL %s\n", url);
    if(_path) gtk_tree_path_free(_path);
  }
  gtk_widget_destroy(dlg);

  switch(type) {
    case EBOX_WEB_UI:
      if(url)
        generate_web_photo(url, ebox);
    break;
    case EBOX_MUSIC_UI:
      if(url)
        generate_music_photo(url, ebox);
    break;
    case EBOX_PHOTO_UI:
      if(url)
        generate_photo_photo(url, ebox);
    break;
  }

}

static void ht_value_destroy_cb(gpointer data) {
  if(data)
    g_object_unref(data);
}

#include "photomusic.c"

typedef struct {
  const gchar *group;
  const gchar *key;
  const gchar *app;
} EPG1_app_item_t;

static void do_EPG1_action(const gchar *cmdline) {
  GError *err = NULL;
  GKeyFile *kf;
  gchar **groups = NULL;
  gchar *app = NULL, *key = NULL;
  gchar *val = NULL;
  gint len = 0;
  gchar *argv[] = {NULL, NULL, NULL};
  EPG1_app_item_t apps[] = {
    {"RSS", "rss", "rss-app"},
    {"Photo", "photo", "photo-viewer"},
    {"Music", "music", "music-player"},
    {"Fav", "fav", "firefox-skype"},
    {NULL, NULL, NULL}};

  kf = g_key_file_new();
  if(! g_file_test(cmdline, G_FILE_TEST_EXISTS)) {
    cmdline = getenv("CLFILE");
    if(! g_file_test(cmdline, G_FILE_TEST_EXISTS)) {
      DEBUG("EPG1 cmdline file isn't exist!\n");
      return;
    }
  }
  if(g_key_file_load_from_file(kf, cmdline, G_KEY_FILE_NONE, &err)) {
    gint idx = 0;
    groups = g_key_file_get_groups(kf, &len);
    if(len > 0 && groups[0]) {
      for(; apps[idx].group; idx++) {
        if(strcmp(apps[idx].group, groups[0]) == 0) {
          key = (gchar *)(apps[idx].key), app = (gchar *)(apps[idx].app);
          break;
        }
      }/**for(...)*/
      val = g_key_file_get_string(kf, groups[0], key, &err);
      if(val) {
        argv[0] = app, argv[1] = val;
        launch_app_async(argv, NULL, 0);
        g_free(val);
      }
      g_strfreev(groups);
    }
  } else {
    ERROR("load EPG1 cmdline file failed.\n");
  }
  g_key_file_free(kf);
}

int main(int argc, char *argv[]) {
  GtkWidget *window = NULL, *vbox = NULL;
  gchar buff[1024] = {0};
  gchar *username = "DeviceVM";
  GError *err = NULL;
  sqlite3 *bmDB = NULL;
  gint ret, idx, tid;
  GtkTreeModel *model = NULL;
  GdkPixbuf *background = NULL;

  DEBUG("BUILD TIMESTAMP @ "__TIME__", "__DATE__"\n");
  bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
  textdomain(GETTEXT_PACKAGE);

  ret = sqlite3_open_v2(BOOKMARK_SQLITE3_DBFILE, &bmDB, SQLITE_OPEN_NOMUTEX|SQLITE_OPEN_READONLY, NULL);
  if(ret != SQLITE_OK) {
    ERROR("open bookmark database failed (%s) path = %s.\n", sqlite3_errmsg(bmDB), BOOKMARK_SQLITE3_DBFILE);
    sqlite3_close(bmDB);
    bmDB = NULL;
  }

  if(! bmDB) {
    gchar *dbfile = getenv("DBFILE");
    if(dbfile) {
      ret = sqlite3_open_v2(dbfile, &bmDB, SQLITE_OPEN_NOMUTEX|SQLITE_OPEN_READONLY, NULL);
      if(ret != SQLITE_OK) {
        ERROR("open bookmark database failed (%s) path = %s.\n", sqlite3_errmsg(bmDB), dbfile);
        sqlite3_close(bmDB);
        bmDB = NULL;
      }
    }
  }
  gtk_init(&argc, &argv);

  g_pu.alpha = 0, g_pu.idx = 0;
  background = gdk_pixbuf_new_from_inline(-1, background_pixbuf, FALSE, &err);
  g_bmpixbuf[BM_TAG_PIXBUF] = gdk_pixbuf_new_from_inline(-1, bm_tag_pixbuf, FALSE, &err);
  g_bmpixbuf[BM_DIRECTORY_PIXBUF] = gdk_pixbuf_new_from_inline(-1, bm_directory_pixbuf, FALSE, &err);
  g_bmpixbuf[BM_MENU_PIXBUF] = gdk_pixbuf_new_from_inline(-1, bm_menu_pixbuf, FALSE, &err);
  g_bmpixbuf[BM_TOOLBAR_PIXBUF] = gdk_pixbuf_new_from_inline(-1, bm_toolbar_pixbuf, FALSE, &err);
  g_bmpixbuf[BM_UNSORTED_PIXBUF] = gdk_pixbuf_new_from_inline(-1, bm_unsorted_pixbuf, FALSE, &err);
  g_bmpixbuf[BM_FAVICON_PIXBUF] = gdk_pixbuf_new_from_inline(-1, bm_favicon_pixbuf, FALSE, &err);
  g_bmpixbuf[WEB_BG_PIXBUF] = gdk_pixbuf_new_from_inline(-1, web_background_pixbuf, FALSE, &err);
  g_bmpixbuf[MEDIA_BG_PIXBUF] = gdk_pixbuf_new_from_inline(-1, media_background_pixbuf, FALSE, &err);

  g_bmht = g_hash_table_new_full(NULL, NULL, NULL, ht_value_destroy_cb);
  if(bmDB) {
    fetch_bookmarks_favicons(bmDB, g_bmht);
    model = create_model_from_database(bmDB);
    sqlite3_close(bmDB);
  }

  g_main_window = window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_app_paintable(window, TRUE);
  gtk_window_set_title(GTK_WINDOW(window), "DVM_Dashboard");
  gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
  gtk_widget_set_size_request(window, 1024, 576);
  gtk_window_set_decorated(GTK_WINDOW(window), TRUE);
  gtk_container_set_border_width(GTK_CONTAINER(window), 2);

  g_signal_connect(G_OBJECT(window), "expose-event", G_CALLBACK(main_window_draw_background), background);
  g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(main_window_close), NULL);

  vbox = gtk_vbox_new(FALSE, 40);
  gtk_box_pack_start(GTK_BOX(vbox), create_time_stamp(window), FALSE, TRUE, 6);
  gtk_box_pack_start(GTK_BOX(vbox), create_launcher_ui(window, model), TRUE, TRUE, 0);

  gtk_container_add(GTK_CONTAINER(window), vbox);

  gtk_widget_show_all(window);
  tid = g_timeout_add_seconds(5, photo_ui_switch_photo_cb, &g_pu);
  do_EPG1_action("/proc/hidcmd/cmdline");
  gtk_main();

  g_source_remove(tid);
  g_object_unref(background);
  for (idx=0; idx < BM_MAX_PIXBUF; idx++) {
    g_object_unref(g_bmpixbuf[idx]);
  }
  if(g_bmht)
    g_hash_table_destroy(g_bmht);
}
