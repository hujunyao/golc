#include <gtk/gtk.h>
#include "_updater_inline_pixbuf.h"

#define GETTEXT_PACKAGE "updater"
#define LOCALEDIR "/usr/share/locale"
#define UPDATER_CFG_FILE "/home/.updater.ini"

#include <glib/gi18n-lib.h>
#include <libintl.h>

#define DEBUG(...) fprintf(stdout, "[Updater] "__VA_ARGS__)

#define UPDATER_VERSION "0.
enum {
  UPDATER_STA_IDLE,
  UPDATER_STA_REFRESH,
  UPDATER_STA_SHOWERR,
  UPDATER_STA_DLREADY,
  UPDATER_STA_DOWNLOADING,
  UPDATER_STA_DLDONE,
  UPDATER_STA_INSTALL
};

enum {
  PACKAGE_TYPE_SYS,
  PACKAGE_TYPE_APP
};

enum {
  COL_PACKAGE_NAME = 0x1000,
  COL_PACKAGE_REMAIN_SIZE,
  COL_PACKAGE_VER,
  COL_PACKAGE_REMAIN_TIME,
  PACKAGE_COL_NUM = 4
};

typedef struct {
  gchar *name;
  gchar *version;
  gchar *detail;
  gint   size;
  gint   type;

  gint   gotsize;
  time_t t0;
  GtkTreeIter iter;
} package_info_t;

typedef struct {
  GtkWidget *parent;
  gboolean is_background;
  gint sta;
  GtkWidget *container;
  GtkWidget *ebox;
  GtkWidget *label;
  GtkWidget *expander;

  /**all package list*/
  GSList *pkglist;

  /**system package list*/
  GSList *syspkg;

  /**system packages info*/
  package_info_t spi;

  /**updater config*/
  gint check_every_month;
  gint last_time;
} updater_t;

static gboolean load_default_cfg_file(updater_t *u) {
  GKeyFile *kf = NULL;
  GKeyFileFlags flags;

  kf = g_key_file_new();
  flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
  if(!g_key_file_load_from_file(kf, UPDATER_CFG_FILE, flags, &err)) {
    g_error (error->message);
    g_key_file_free(kf);
    generate_default_cfg_file(u);

    return FALSE;
  }

  err = NULL;
  u->check_every_month = g_key_file_get_integer(kf, "General", "mode", &err);
  if(err != NULL)
    g_error(err->message);
  else
    u->check_every_month = 1;

  err = NULL;
  u->last_time = g_key_file_get_integer(kf, "General", "time", &err);
  if(err != NULL)
    g_error(err->message);
  else
    u->last_time = time(NULL);

  g_key_file_free(kf);
}

static gboolean generate_default_cfg_file(updater_t *u) {
#define GROUP_COMMENT "DON'T EDIT THIS FILE BY HAND !"
  GKeyFile *kf = NULL;
  GError *err = NULL;
  gchar *data = NULL;
  gsize length;

  kf = g_key_file_new();

  u->check_every_month = 1;
  u->last_time = time(NULL);

  g_key_file_set_integer(kf, "General", "mode", 1);
  g_key_file_set_integer(kf, "General", "time", time(NULL));
  g_key_file_set_comment(kf, "General", NULL, GROUP_COMMENT, &err);
  data = g_key_file_to_data(kf, &length, &err);
  g_key_file_free(kf);

  if(data) {
    gboolean ret;

    ret = g_file_set_contents(UPDATER_CFG_FILE, data, length, &err);
    g_free(data);
    return ret;
  }

  DEBUG("Generate updater default config file failure!\n");
  return FALSE;
}

static gboolean updater_startup_policy(updater_t *u) {
  /**return FALSE, updater will be quit*/

  if(! g_file_test(UPDATER_CFG_FILE, G_FILE_TEST_EXISTS)) {
    generate_default_cfg_file(u);
    if(u->is_background)
      return FALSE;
  }

  load_default_cfg_file(u);
  if(! u->check_every_month) {
    if(u->is_background)
      return FALSE;
  } else {
    if(u->is_background) {
      gint curr_time = time(NULL);
      if(curr_time - u->time < ONE_MONTH)
        return FALSE;
    }
  }

  return TRUE;
}

static void parse_command_option(updater_t *u, gint *argc, gchar ***argv) {
  GError *err = NULL;
  GOptionContext *oc = NULL;
  gboolean checknow;
  GOptionEntry entries[] = {
    {"background", 'b', 0, G_OPTION_ARG_NONE, NULL, "run updater background", NULL},
    {"checknow", 'n', 0, G_OPTION_ARG_NONE, &checknow, "check package upgrade force", NULL},
    { NULL }
  };

  entries[0].arg_data = &(u->is_background);

  oc = g_option_context_new("- BUILD TIME @ "__TIME__", "__DATE__);
  g_option_context_add_main_entries(oc, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (oc, gtk_get_option_group (TRUE));
  if (!g_option_context_parse (oc, argc, argv, &err)) {
    DEBUG("option parsing failed: %s\n", err->message);
    exit(1);
  }
  g_option_context_free(oc);
  DEBUG(" background = %d, checknow = %d\n", u->is_background, checknow);

}

static void cancel_button_clicked(GtkButton *widget, gpointer data) {
}

static void download_button_clicked(GtkButton *widget, gpointer data) {
}

static void switch_button_clicked(GtkButton *widget, gpointer data) {
}

static void refresh_button_clicked(GtkButton *widget, gpointer data) {
}

static gboolean main_window_close(GtkWidget *widget, GdkEvent *event, gpointer data) {
  DEBUG("close main window\n");
  gtk_main_quit();

  return FALSE;
}

static GtkWidget *_ui_create_updater_header(updater_t *u) {
  GtkWidget *hbox = NULL, *hvbox = NULL, *hhbox = NULL, *hhvbox = NULL, *hvhbox = NULL, *hvvbox = NULL;
  GtkWidget *image = NULL, *label = NULL, *swbtn, *rfhbtn;
  GdkPixbuf *updater_logo = NULL;
  GError *err = NULL;

  hbox = gtk_hbox_new(FALSE, 0);
  hvbox = gtk_vbox_new(FALSE, 0);
  updater_logo = gdk_pixbuf_new_from_inline(-1, updater_logo_pixbuf, FALSE, &err);
  image = gtk_image_new_from_pixbuf(updater_logo);
  g_object_unref(updater_logo);
  gtk_box_pack_end (GTK_BOX(hvbox), image, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), hvbox, FALSE, FALSE, 0);

  hvbox = gtk_vbox_new(FALSE, 0);
  hvhbox = gtk_hbox_new(FALSE, 0);
  label = gtk_label_new("updater v0.2, 2008 - 2009 deviceVM");
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD);
  gtk_box_pack_start (GTK_BOX(hvhbox), label, FALSE, FALSE, 0);
  gtk_box_pack_end (GTK_BOX(hvbox), hvhbox, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), hvbox, TRUE, TRUE, 8);

  hvbox = gtk_vbox_new(FALSE, 0);
  hvvbox = gtk_vbox_new(TRUE, 8);
  if(u->is_background)
    swbtn = gtk_button_new_with_label(_("Show"));
  else
    swbtn = gtk_button_new_with_label(_("Hide"));
  gtk_widget_set_size_request(swbtn, 120, -1);
  g_signal_connect(swbtn, "clicked", G_CALLBACK(switch_button_clicked), u);
  gtk_box_pack_start (GTK_BOX(hvvbox), swbtn, FALSE, TRUE, 0);
  rfhbtn = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
  gtk_box_pack_start (GTK_BOX(hvvbox), rfhbtn, FALSE, TRUE, 0);
  g_signal_connect(rfhbtn, "clicked", G_CALLBACK(refresh_button_clicked), u);
  gtk_box_pack_end (GTK_BOX(hvbox), hvvbox, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), hvbox, FALSE, TRUE, 0);

  return hbox;
}

static void format_time(gchar *buf, gint len, gint sec) {
  gint h, m, s;

  /**remain hour*/
  m = sec % 3600;
  h = sec / 3600;

  /**remain minute*/
  s = m % 60;
  m = m / 60;

  if(h >= 24) {
    snprintf(buf, len, "%s", _("more than one day"));
  } else if(h > 0) {
    snprintf(buf, len, "%d %s %d %s", h, _("hour"), m, _("minute"));
  } else if(h <= 0) {
    if(m > 0) {
      snprintf(buf, len, "%d %s %d %s", m, _("minute"), s, _("second"));
    } else {
      snprintf(buf, len, "%d %s", s, _("second"));
    }
  }
}

static void format_remain_time(gchar *buf, gint len, package_info_t *pi) {
  gdouble speed;
  gint interval, remain_second;

  interval = time(NULL) - pi->t0;
  speed = (gdouble)pi->gotsize/interval;

  remain_second = (pi->size - pi->gotsize)/speed;

  format_time(buf, len, remain_second);
}

static void format_size(gchar *buf, gint len, gint bytes) {
  gint mb, kb, b;

  kb = bytes % (1024*1024);
  mb = bytes / (1024*1024);

  b  = kb % 1024;
  kb = kb / 1024;

  if(mb >= 1024) {
    snprintf(buf, len, " %s", _("more than 1GB"));
  } else if(mb > 0) {
    snprintf(buf, len, "%d %s %d %s", mb, _("MB"), kb, _("KB"));
  } else if(mb <= 0) {
    if(kb > 0) {
      snprintf(buf, len, "%d %s", kb, _("KB"));
    } else {
      snprintf(buf, len, "%s", _("less than 1KB"));
    }
  }
}

static void format_remain_size(gchar *buf, gint len, package_info_t *pi) {
  format_size(buf, len, pi->size-pi->gotsize);
}

static GtkTreeModel* _create_package_view_model(updater_t *u) {
  GtkListStore *store = NULL;
  GSList *curr = NULL;

  u->spi.size = 0;
  u->spi.gotsize = 0;
  u->spi.type = PACKAGE_TYPE_SYS;
  u->spi.name = "System package";

  store = gtk_list_store_new (PACKAGE_COL_NUM, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_POINTER);
  for(curr = u->pkglist; curr; curr = g_slist_next(curr)) {
    package_info_t *pi = (package_info_t *)(curr->data);

    if(pi->type == PACKAGE_TYPE_APP) {
      gtk_list_store_append (store, &pi->iter);
      gtk_list_store_set(store, &pi->iter, COL_PACKAGE_NAME, pi->name, COL_PACKAGE_REMAIN_SIZE, pi,
                         COL_PACKAGE_VER, pi->version, COL_PACKAGE_REMAIN_TIME, pi, -1);
    } else {
      u->syspkg = g_slist_append(u->syspkg, pi);
      u->spi.size += pi->size;
    }
  }

  if(u->syspkg) {
    gtk_list_store_prepend (store, &(u->spi.iter));
    gtk_list_store_set(store, &(u->spi.iter), COL_PACKAGE_NAME, u->spi.name, COL_PACKAGE_REMAIN_SIZE, &u->spi, COL_PACKAGE_VER, u->spi.version, COL_PACKAGE_REMAIN_TIME, &u->spi, -1);
  }

  return GTK_TREE_MODEL (store);
}

static void pkg_remain_size_set_func (GtkTreeViewColumn *column, GtkCellRenderer *cell, 
                                      GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
  updater_t *u = (updater_t *)data;
  package_info_t *pi = NULL;
  gchar buf[1024] = {0};

  gtk_tree_model_get(model, iter, 0, &pi, -1);
  if(pi == NULL) return;

  if(u->sta == UPDATER_STA_DOWNLOADING) {
    format_remain_size(buf, 1024, pi);
    g_object_set (GTK_CELL_RENDERER (cell), "value", (100 * (pi->gotsize))/pi->size, NULL);
    g_object_set (GTK_CELL_RENDERER (cell), "text", buf, NULL);
  }
}

static void pkg_version_and_remain_time_set_func (GtkTreeViewColumn *column, GtkCellRenderer *cell, 
                                                  GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
  updater_t *u = (updater_t *)data;
  package_info_t *pi = NULL;
  gchar buf[1024] = {0};

  gtk_tree_model_get(model, iter, 0, &pi, -1);
  if(pi == NULL) return;

  if(u->sta == UPDATER_STA_DLREADY) {
    g_object_set (GTK_CELL_RENDERER (cell), "text", pi->version, NULL);
  } else if(u->sta == UPDATER_STA_DOWNLOADING) {
    format_remain_time(buf, 1024, pi);
    g_object_set (GTK_CELL_RENDERER (cell), "text", buf, NULL);
  }
}

static void package_view_selection_changed(GtkTreeSelection *selection, gpointer data) {
  updater_t *u = (updater_t *)data;
  GtkWidget *textview = NULL;
  GtkTreeModel *model;
  GtkTreeIter iter;

  if(u->expander)
    textview = gtk_bin_get_child(GTK_BIN(u->expander));
  if (!textview) return;

  if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
    package_info_t *pi;

    gtk_tree_model_get(model, &iter, COL_PACKAGE_REMAIN_SIZE, &pi, -1);
    if(pi) {
      DEBUG("package detail = %s\n", pi->detail);
    }
  }
}

static GtkWidget* _ui_create_package_view(updater_t *u) {
  GtkWidget *sw = NULL, *treeview = NULL;
  GtkTreeModel *model = NULL;
  GtkTreeViewColumn *column = NULL;
  GtkTreeSelection *selection = NULL;
  GtkCellRenderer *renderer;

  sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  model = _create_package_view_model(u);
  treeview = gtk_tree_view_new_with_model(model);
  g_object_unref(model);

  gtk_container_add(GTK_CONTAINER(sw), treeview);

  column = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(column, _("Application name"));
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(column, renderer, FALSE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

  column = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(column, _("File size"));
  renderer = gtk_cell_renderer_progress_new ();
  gtk_tree_view_column_pack_start(column, renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func (column, renderer, pkg_remain_size_set_func, u, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

  column = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(column, _("Version"));
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(column, renderer, FALSE);
  gtk_tree_view_column_set_cell_data_func (column, renderer, pkg_version_and_remain_time_set_func, u, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
  g_signal_connect(selection, "changed", G_CALLBACK(package_view_selection_changed), u);

  return sw;
}

static GtkWidget *ui_create_package_view(updater_t *u) {
  GtkWidget *vbox = NULL, *expander = NULL;
  GtkWidget *textview = NULL;

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), _ui_create_package_view(u), TRUE, TRUE, 0);
  expander = gtk_expander_new(_("Detail"));
  gtk_expander_set_expanded(GTK_EXPANDER(expander), FALSE);
  gtk_expander_set_spacing(GTK_EXPANDER(expander), 6);

  textview = gtk_text_view_new();
  gtk_container_add(GTK_CONTAINER(expander), textview);
  u->expander = expander;

  return vbox;
}

static GtkWidget* ui_create_updater_header(updater_t *u) {
  GtkWidget *hbox = NULL;

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), _ui_create_updater_header(u), TRUE, TRUE, 16);

  return hbox;
}

static gboolean ebox_on_expose (GtkWidget *widget, GdkEventExpose *expose) {
  cairo_t *cr;
  cairo_pattern_t *pattern;
  static cairo_surface_t *image = NULL;
  gint ww = widget->allocation.width, wh = widget->allocation.height;
  gint iw, ih;

  cr = gdk_cairo_create (widget->window);
  cairo_rectangle(cr, 0, 0, ww, wh);
  cairo_clip(cr);

  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
  cairo_paint(cr);

  cairo_rectangle(cr, 0, 0, ww, 28);
  cairo_set_source_rgba(cr, 0.52, 0.82, 0.97, 1.0);
  cairo_fill_preserve(cr);

  cairo_set_line_width(cr, 1.2);
  cairo_set_source_rgba(cr, 0.02, 0.02, 0.07, 1.0);
  cairo_stroke(cr);

  cairo_rectangle(cr, 0, 0, ww, wh);
  cairo_set_source_rgba(cr, 0.02, 0.02, 0.07, 1.0);
  cairo_set_line_width(cr, 2.0);
  cairo_stroke(cr);

  cairo_destroy(cr);

  return FALSE;
}

static GtkWidget *_ui_create_message_widget() {
  GtkWidget *vbox = NULL, *vvbox=NULL, *label = NULL, *image = NULL;
  GtkWidget *hbox = NULL, *hvbox = NULL;

  vbox = gtk_vbox_new(FALSE, 8);
  image = gtk_image_new_from_stock(GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX(vbox), image, FALSE, TRUE, 0);
  label = gtk_label_new(_("This is a demo message widget, you can display a label and icon in event box."));
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD);
  gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, TRUE, 0);

  return vbox;
}

static GtkWidget *ui_create_message_widget() {
  GtkWidget *hbox = NULL, *hvbox = NULL;

  hbox = gtk_hbox_new(FALSE, 0);
  hvbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hvbox), _ui_create_message_widget(), TRUE, TRUE, 96);
  //gtk_box_pack_start(GTK_BOX(hbox), _ui_create_message_widget(), TRUE, TRUE, 32);
  gtk_box_pack_start(GTK_BOX(hbox), hvbox, TRUE, TRUE, 0);

  return hbox;
}

static GtkWidget *ui_create_refresh_animation(updater_t *u) {
  GtkWidget *ebox = NULL;
  GtkWidget *image = NULL;

  ebox = gtk_event_box_new();
  gtk_widget_set_app_paintable(ebox, TRUE);
  image = gtk_image_new_from_file("icons/sample.gif");
  g_signal_connect(ebox, "expose-event", G_CALLBACK(ebox_on_expose), NULL);
  //gtk_container_add(GTK_CONTAINER(ebox), image);
  //gtk_container_add(GTK_CONTAINER(ebox), ui_create_message_widget());
  gtk_container_add(GTK_CONTAINER(ebox), ui_create_package_view(u));

  u->ebox = ebox;

  return ebox;
}

static GtkWidget *ui_create_updater_body(updater_t *u) {

  u->container = gtk_hbox_new(FALSE, 0);

  if(!u->is_background) {
    GtkWidget *ebox = NULL;

    ebox = ui_create_refresh_animation(u);
    gtk_box_pack_start(GTK_BOX(u->container), ebox, TRUE, TRUE, 16);
  }
  return u->container;
}

int main(int argc, char *argv[]) {
  updater_t u = {0};
  GtkWidget *window = NULL;
  GtkWidget *dlbtn = NULL, *ccbtn = NULL;
  GtkWidget *vbox = NULL, *vhbox = NULL, *vhhbox = NULL;

  DEBUG("BUILD TIMESTAMP @ "__TIME__", "__DATE__"\n");
  bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
  textdomain(GETTEXT_PACKAGE);

  u.is_background = FALSE;
  u.sta = UPDATER_STA_REFRESH;
  gtk_init(&argc, &argv);
  u.parent = window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), _("Updater"));
  gtk_container_set_border_width(GTK_CONTAINER(window), 0);
  gtk_window_set_default_size (GTK_WINDOW(window), 640, 480);

  g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(main_window_close), NULL);

  dlbtn = gtk_button_new_with_label(_("Download"));
  ccbtn = gtk_button_new_with_label(_("Cancel"));

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), ui_create_updater_header(&u), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), ui_create_updater_body(&u), TRUE, TRUE, 6);

  vhbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vhbox), gtk_hseparator_new(), TRUE, TRUE, 6);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, FALSE, TRUE, 0);

  vhbox = gtk_hbox_new(FALSE, 0);
  vhhbox = gtk_hbox_new(TRUE, 8);
  gtk_box_pack_start(GTK_BOX(vhhbox), ccbtn, TRUE, TRUE, 0);
  //gtk_widget_set_size_request(ccbtn, 96, -1);
  g_signal_connect(ccbtn, "clicked", G_CALLBACK(cancel_button_clicked), &u);
  gtk_box_pack_start(GTK_BOX(vhhbox), dlbtn, TRUE, TRUE, 0);
  g_signal_connect(ccbtn, "clicked", G_CALLBACK(download_button_clicked), &u);
  gtk_box_pack_end(GTK_BOX(vhbox), vhhbox, FALSE, TRUE, 16);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, FALSE, TRUE, 8);

  gtk_container_add(GTK_CONTAINER(window), vbox);

  if(!u.is_background)
    gtk_widget_show_all(window);

  gtk_main();

  return 0;
}
