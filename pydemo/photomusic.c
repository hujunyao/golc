#define MEDIAUI_TREE_MODEL_ICON_SIZE 48

static gboolean scan_music_path(gchar *prefix_path, gchar *file, gchar *logo) {
  gboolean is_music_exist = FALSE, is_logo_exist = FALSE;
  gchar path[1024] = {0};

  snprintf(logo, 1024, "%s/logo.jpg", prefix_path);
  is_logo_exist = g_file_test(logo, G_FILE_TEST_IS_REGULAR|G_FILE_TEST_EXISTS) && gdk_pixbuf_get_file_info(logo, NULL, NULL);
  if(! is_logo_exist) {
    bzero(logo, 1024);
    snprintf(logo, 1024, "%s/logo.png", prefix_path);
    is_logo_exist = g_file_test(logo, G_FILE_TEST_IS_REGULAR|G_FILE_TEST_EXISTS) && gdk_pixbuf_get_file_info(logo, NULL, NULL);
  }

  snprintf(file, 1024, "%s/music.pls", prefix_path);
  is_music_exist = g_file_test(file, G_FILE_TEST_IS_REGULAR|G_FILE_TEST_EXISTS);
  if(! is_music_exist) {
    snprintf(file, 1024, "%s/music.mp3", prefix_path);
    is_music_exist = g_file_test(logo, G_FILE_TEST_IS_REGULAR|G_FILE_TEST_EXISTS);
  }

  return is_music_exist && is_logo_exist;
}

static gboolean scan_photo_path(gchar *prefix_path, gchar *file, gchar *logo, GSList **pl) {
  GDir *dir = NULL;
  gchar path[1024] = {0};
  gboolean is_photo_dir = FALSE;
  GError *err = NULL;
  const gchar *name = NULL;
  photo_item_t *pi = NULL;

  dir = g_dir_open(prefix_path, 0, &err);
  if(dir) {
    while(name = g_dir_read_name(dir)) {
      snprintf(path, 1024, "%s/%s", prefix_path, name);
      //DD("path = %s\n", path);
      if(is_photo_dir == FALSE) {
        if(g_file_test(path, G_FILE_TEST_IS_REGULAR) && gdk_pixbuf_get_file_info(path, NULL, NULL)) {
          strncpy(logo, path, strlen(path));
          is_photo_dir = TRUE;
          if(pl) {
            pi = g_new(photo_item_t, 1);
            pi->path = strdup(path);
            pi->pixbuf = gdk_pixbuf_new_from_file_at_scale(path, MEDIAUI_WIDTH-2, MEDIAUI_HEIGHT-2, TRUE, &err);
            pi->w = gdk_pixbuf_get_width(pi->pixbuf); pi->h = gdk_pixbuf_get_height(pi->pixbuf);
            *pl = g_slist_append(*pl, pi);
          }
        }
      } else if(gdk_pixbuf_get_file_info(path, NULL, NULL)) {
        if(pl) {
          pi = g_new(photo_item_t, 1);
          pi->path = strdup(path);
          pi->pixbuf = NULL;
          *pl = g_slist_append(*pl, pi);
        }
      }
    }
    g_dir_close(dir);
  } else {
    ERROR("open photo dir = %s failed\n", prefix_path);
  }
  if(is_photo_dir)
    strncpy(file, prefix_path, 1024);

  return is_photo_dir;
}

static GtkTreeModel *create_model_for_photo(gchar *prefix_path) {
  GtkTreeStore *store = NULL;
  GtkTreeIter iter;
  GDir *dir = NULL;
  gchar path[1024] = {0};
  GError *err = NULL;

  store = gtk_tree_store_new(BM_NUM_COL, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

  if(! prefix_path)
    prefix_path = PHOTO_PATH;

  dir = g_dir_open(prefix_path, 0, &err);
  if(dir) {
    const gchar *name = NULL;
    while(name = g_dir_read_name(dir)) {
      //DEBUG("path name = %s\n", name);
      snprintf(path, 1024, "%s/%s", prefix_path, name);
      if(g_file_test(path, G_FILE_TEST_IS_DIR)) {
        gchar file[1024] = {0}, logo[1024] = {0};
        if(scan_photo_path(path, file, logo, NULL)) {
          GdkPixbuf *pixbuf = NULL;
          GError *err = NULL;

          DD("load pixbuf %s\n", logo);
          pixbuf = gdk_pixbuf_new_from_file_at_scale(logo, MEDIAUI_TREE_MODEL_ICON_SIZE, MEDIAUI_TREE_MODEL_ICON_SIZE, TRUE, &err);
          if(pixbuf) {
            gtk_tree_store_append(store, &iter, NULL);
            gtk_tree_store_set(store, &iter, BM_PIXBUF_COL, pixbuf, BM_TITLE_COL, name, BM_URL_COL, logo, -1);
            g_object_unref(pixbuf);
          }
        }/**g_file_test*/
      }
    } /**while(read_dir_name)*/

    g_dir_close(dir);
  }

  return GTK_TREE_MODEL(store);
}

static GtkTreeModel *create_model_for_music(gchar *prefix_path) {
  GtkTreeStore *store = NULL;
  GtkTreeIter iter;
  GDir *dir = NULL;
  gchar path[1024] = {0};
  GError *err = NULL;

  store = gtk_tree_store_new(BM_NUM_COL, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

  if(! prefix_path)
    prefix_path = MUSIC_PATH;

  dir = g_dir_open(prefix_path, 0, &err);
  if(dir) {
    const gchar *name = NULL;
    while(name = g_dir_read_name(dir)) {
      //DEBUG("path name = %s\n", name);
      snprintf(path, 1024, "%s/%s", prefix_path, name);
      if(g_file_test(path, G_FILE_TEST_IS_DIR)) {
        gchar file[1024] = {0}, logo[1024] = {0};
        if(scan_music_path(path, file, logo)) {
          GdkPixbuf *pixbuf = NULL;
          GError *err = NULL;

          //DD("load pixbuf %s\n", logo);
          pixbuf = gdk_pixbuf_new_from_file_at_scale(logo, MEDIAUI_TREE_MODEL_ICON_SIZE, MEDIAUI_TREE_MODEL_ICON_SIZE, FALSE, &err);
          if(pixbuf) {
            gtk_tree_store_append(store, &iter, NULL);
            gtk_tree_store_set(store, &iter, BM_PIXBUF_COL, pixbuf, BM_TITLE_COL, name, BM_URL_COL, file, -1);
            g_object_unref(pixbuf);
          }
        }/**g_file_test*/
      }
    } /**while(read_dir_name)*/

    g_dir_close(dir);
  }

  return GTK_TREE_MODEL(store);
}

static GtkWidget *create_media_preview_ui_by_type (GtkWidget *window, gint type) {
  GtkWidget *hbox = NULL;
  GtkWidget *ebox = NULL;
  GdkPixbuf *pixbuf = NULL;
  GError *err = NULL;
  GtkTreeModel *model = NULL;
  gchar *url = NULL;
  gchar *prefix_env = NULL;

  hbox = gtk_hbox_new(TRUE, 6);
  ebox = gtk_event_box_new();
  if(type == EBOX_PHOTO_UI) {
    photo_item_t *pi = NULL;
    gchar file[1024] = {0}, logo[1024] = {0};

    prefix_env = getenv("PHOTO_PATH1");
    if(! prefix_env)
      prefix_env = PHOTO_PATH1;

    if(scan_photo_path(prefix_env, file, logo, &(g_pu.pl[0]))) {
      pi = (photo_item_t *) (g_pu.pl[0]->data);
      pixbuf = g_object_ref(pi->pixbuf);
      url = strdup(logo);
    }
    g_pu.ebox[0] = ebox;
    g_pu.curr[0] = g_pu.pl[0];
    DEBUG("pixbuf = %x, width = %d, height = %d\n", pixbuf, gdk_pixbuf_get_width(pixbuf), gdk_pixbuf_get_height(pixbuf));
  } else {
    gchar file[1024] = {0}, logo[1024] = {0};

    prefix_env = getenv("MUSIC_PATH1");
    if(! prefix_env)
      prefix_env = MUSIC_PATH1;

    if(scan_music_path(prefix_env, file, logo)) {
      pixbuf = gdk_pixbuf_new_from_file_at_scale(logo, MEDIAUI_WIDTH-2, MEDIAUI_HEIGHT-2, FALSE, &err);
      url = strdup(file);
    }
  }
  ebox_set_data(ebox, url, pixbuf, model);
  gtk_widget_set_size_request(ebox, MEDIAUI_WIDTH, MEDIAUI_HEIGHT);
  g_signal_connect(G_OBJECT(ebox), "button_press_event", G_CALLBACK(ebox_button_press_event_cb), (gpointer)type);
  g_signal_connect(G_OBJECT(ebox), "expose-event", G_CALLBACK(ebox_draw_background), (gpointer)type);
  gtk_box_pack_start(GTK_BOX(hbox), ebox, FALSE, FALSE, 0);

  ebox = gtk_event_box_new();
  if(type == EBOX_PHOTO_UI) {
    photo_item_t *pi = NULL;
    gchar file[1024] = {0}, logo[1024] = {0};

    prefix_env = getenv("PHOTO_PATH2");
    if(! prefix_env)
      prefix_env = PHOTO_PATH2;

    if(scan_photo_path(prefix_env, file, logo, &(g_pu.pl[1]))) {
      pi = (photo_item_t *) (g_pu.pl[1]->data);
      pixbuf = g_object_ref(pi->pixbuf);
      url = strdup(logo);
    }
    g_pu.ebox[1] = ebox;
    g_pu.curr[1] = g_pu.pl[1];
    DEBUG("file = %s, pixbuf = %x, width = %d, height = %d\n", file, pixbuf, gdk_pixbuf_get_width(pixbuf), gdk_pixbuf_get_height(pixbuf));
  } else {
    gchar file[1024] = {0}, logo[1024] = {0};

    prefix_env = getenv("MUSIC_PATH2");
    if(! prefix_env)
      prefix_env = MUSIC_PATH2;

    if(scan_music_path(prefix_env, file, logo)) {
      pixbuf = gdk_pixbuf_new_from_file_at_scale(logo, MEDIAUI_WIDTH-2, MEDIAUI_HEIGHT-2, FALSE, &err);
      url = strdup(file);
    }
  }
  ebox_set_data(ebox, url, pixbuf, model);
  gtk_widget_set_size_request(ebox, MEDIAUI_WIDTH, MEDIAUI_HEIGHT);
  g_signal_connect(G_OBJECT(ebox), "button_press_event", G_CALLBACK(ebox_button_press_event_cb), (gpointer)type);
  g_signal_connect(G_OBJECT(ebox), "expose-event", G_CALLBACK(ebox_draw_background), (gpointer)type);
  gtk_box_pack_start(GTK_BOX(hbox), ebox, FALSE, FALSE, 0);

  ebox = gtk_event_box_new();
  if(type == EBOX_PHOTO_UI) {
    photo_item_t *pi = NULL;
    gchar file[1024] = {0}, logo[1024] = {0};

    prefix_env = getenv("PHOTO_PATH3");
    if(! prefix_env)
      prefix_env = PHOTO_PATH3;

    if(scan_photo_path(prefix_env, file, logo, &(g_pu.pl[2]))) {
      pi = (photo_item_t *) (g_pu.pl[2]->data);
      pixbuf = g_object_ref(pi->pixbuf);
      url = strdup(logo);
    }
    g_pu.ebox[2] = ebox;
    g_pu.curr[2] = g_pu.pl[2];
    DEBUG("pixbuf = %x, width = %d, height = %d\n", pixbuf, gdk_pixbuf_get_width(pixbuf), gdk_pixbuf_get_height(pixbuf));
  } else {
    gchar file[1024] = {0}, logo[1024] = {0};

    prefix_env = getenv("MUSIC_PATH3");
    if(! prefix_env)
      prefix_env = MUSIC_PATH3;

    if(scan_music_path(prefix_env, file, logo)) {
      pixbuf = gdk_pixbuf_new_from_file_at_scale(logo, MEDIAUI_WIDTH-2, MEDIAUI_HEIGHT-2, FALSE, &err);
      url = strdup(file);
    }
  }
  ebox_set_data(ebox, url, pixbuf, model);
  gtk_widget_set_size_request(ebox, MEDIAUI_WIDTH, MEDIAUI_HEIGHT);
  g_signal_connect(G_OBJECT(ebox), "button_press_event", G_CALLBACK(ebox_button_press_event_cb), (gpointer)type);
  g_signal_connect(G_OBJECT(ebox), "expose-event", G_CALLBACK(ebox_draw_background), (gpointer)type);
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
  gtk_box_pack_start(GTK_BOX(vvbox), create_media_preview_ui_by_type(window, EBOX_MUSIC_UI), FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), vvbox, FALSE, TRUE, 0);

  vvbox = gtk_vbox_new(FALSE, 12);
  vhbox = gtk_hbox_new(FALSE, 0);
  label = gtk_label_new(NULL);
  bzero(str, 1024);
  snprintf(str, 1024, H1_FONT_FORMAT, _("_Photos"));
  gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), str);
  gtk_box_pack_start(GTK_BOX(vhbox), label, FALSE, TRUE, 2);
  gtk_box_pack_start(GTK_BOX(vvbox), vhbox, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vvbox), create_media_preview_ui_by_type(window, EBOX_PHOTO_UI), FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), vvbox, FALSE, TRUE, 0);

  return vbox;
}
