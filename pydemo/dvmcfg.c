#include <netinet/in.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <gtk/gtk.h>
#include <pygtk-2.0/pygobject.h>
#include "_cfg_inline_pixbuf.h"

#undef _POSIX_C_SOURCE
#include <Python.h>

#include "gtkimagetab.h"

#define GETTEXT_PACKAGE "dvmcfg"
#define LOCALEDIR "/usr/share/locale"
#include <glib/gi18n-lib.h>

#define DVMCFG_IPC_PATH "/tmp/.dvmcfg.ipc"
#define DVMCFG_EXTERNAL_MODULE_PATH "/usr/lib/dvm-config"
#define DVMCFG_EMBED_PYTHON_PATH "/usr/lib/dvm-config:./python"

#define DEBUG(...) fprintf(stdout, "[DVMCFG] "__VA_ARGS__)

typedef enum {
  PAGE_TYPE_BUILDIN,
  PAGE_TYPE_EXTERNAL_MODULE,
  PAGE_TYPE_EMBEDED_PYTHON,
  PAGE_TYPE_NULL
} page_type_t;

typedef enum {
  SYSTEM_FLAG_NONE = 0,
  SYSTEM_FLAG_RESTARTX         = (1 << 0),
  SYSTEM_FLAG_REBOOT           = (1 << 1),
  SYSTEM_FLAG_RESTORE_ALL      = (1 << 2),
  SYSTEM_FLAG_RESTORE_PLUGINS  = (1 << 3),
  SYSTEM_FLAG_RESTORE_USERDATA = (1 << 4),
  SYSTEM_FLAG_RESTORE = (SYSTEM_FLAG_RESTORE_ALL|SYSTEM_FLAG_RESTORE_PLUGINS|SYSTEM_FLAG_RESTORE_USERDATA)
} system_flag_t;

typedef struct {
  page_type_t type;
  const char *source;
  const guint8 *icon;
  char *label;
  char *description;
  char *tooltip;
  gboolean has_separator;
  gboolean draw_label;
  GtkWidget *guest;
} cfg_page_t;

typedef struct {
  char *name;
  GtkWidget * (* func) (const char *source, void *data);
} cfg_buildin_module_t;

typedef GtkWidget * (* create_ui_func_t) (GtkWidget *, GtkWidget *, GtkWidget *);
static GtkWidget *_create_buildin_ui(const char *source, void *data);
static void cfg_action_confirm_dialog_run(GtkWidget *parent, gint idx, const char *description);

static GtkWidget *g_dialog = NULL, *g_ok = NULL, *g_cancel = NULL, *g_currpage = NULL, *g_fake_ok = NULL;
static system_flag_t g_sysflag = SYSTEM_FLAG_NONE;

static cfg_buildin_module_t g_buildin_func[] = {
  {"foo_create_ui", _create_buildin_ui},
  {NULL, NULL}
};

static cfg_page_t g_pages[] = {
  {
    .type = PAGE_TYPE_EMBEDED_PYTHON,
    .source = "datecfg",
    .icon   = date_icon_pixbuf,
    .label  = N_("Date and Time"),
    .description = NULL, //N_("Please select the Date and Time for the system."),
    .tooltip = N_("Date and Time"),
    .has_separator = FALSE,
    .draw_label = FALSE
  },
  {
    .type = PAGE_TYPE_EMBEDED_PYTHON,
    .source = "langkbcfg",
    .icon   = langkb_icon_pixbuf,
    .label  = N_("Language and Keyboard"),
    .description = NULL, //N_("Please select your language and keyboard."),
    .tooltip = N_("Language and Keyboard"),
    .has_separator = FALSE,
    .draw_label = FALSE
  },
  {
    .type = PAGE_TYPE_EMBEDED_PYTHON,
    .source = "networkcfg",
    .icon   = network_icon_pixbuf,
    .label  = N_("Network"),
    .description = NULL,
    .tooltip = N_("Network"),
    .has_separator = FALSE,
    .draw_label = TRUE
  },
  {
    .type = PAGE_TYPE_EMBEDED_PYTHON,
    .source = "securitycfg",
    .icon   = security_icon_pixbuf,
    .label  = N_("Security"),
    .description = NULL, //N_("Please select security mode."),
    .tooltip = N_("Security"),
    .has_separator = FALSE,
    .draw_label = FALSE
  },
  {
    .type = PAGE_TYPE_EMBEDED_PYTHON,
    .source = "powermgrcfg",
    .icon   = powermgr_icon_pixbuf,
    .label  = N_("Power Management"),
    .description = NULL,
    .tooltip = N_("Power Management"),
    .has_separator = FALSE,
    .draw_label = FALSE
  },
  {
    .type = PAGE_TYPE_EMBEDED_PYTHON,
    .source = "gencfg",
    .icon   = env_icon_pixbuf,
    .label  = NULL, //N_("General"),
    .description = NULL,
    .tooltip = N_("General"),
    .has_separator = FALSE,
    .draw_label = FALSE
  },
  {
    .type = PAGE_TYPE_EMBEDED_PYTHON,
    .source = "about",
    .icon   = about_icon_pixbuf,
    .label  = NULL,
    .description = NULL,
    .tooltip = N_("About"),
    .has_separator = FALSE,
    .draw_label = FALSE
  },
  {
    .type = PAGE_TYPE_NULL,
    .source = NULL,
    .icon   = NULL,
    .label  = NULL,
    .description = NULL,
    .has_separator = FALSE,
    .draw_label = FALSE
  }
};

typedef struct {
  const char *name;
  const guint8 *image;
} image_res_t;
static image_res_t g_py_image_array [] = {
  {
    .name = "about.line",
    .image = about_line_pixbuf
  },
  {
    .name = "about.hplogo",
    .image = about_hplogo_pixbuf
  },
  {
    .name = "about.stlogo",
    .image = about_stlogo_pixbuf
  },
  {
    .name = "powermgr.ac",
    .image = powermgr_ac_pixbuf
  },
  {
    .name = "powermgr.ba",
    .image = powermgr_ba_pixbuf
  },
  {
    .name = "langkb.scim",
    .image = input_icon_pixbuf
  },
  {
    .name = NULL,
    .image = NULL
  }
};

static PyObject* py_cfg_get_pixbuf(PyObject *, PyObject *);
static PyObject* py_cfg_set_action(PyObject *, PyObject *);
static PyObject* py_cfg_launch(PyObject *, PyObject *);

static PyMethodDef g_embed_method[] = {
  {"launch", py_cfg_launch, METH_VARARGS,
   "launch application within dvmcfg"},
  {"get_pixbuf", py_cfg_get_pixbuf, METH_VARARGS, 
   "get pixbuf image resource from dvmcfg binary directly"},
  {"set_action", py_cfg_set_action, METH_VARARGS,
   "set configuration action after apply options"},
  {NULL, NULL, 0, NULL}
};

static void _dump_widget_path(GtkWidget *widget) {
  gint len;
  gchar *path;

  gtk_widget_path(widget, &len, &path, NULL);
  DEBUG("widget path = %s\n", path);
}

static PyObject* py_cfg_launch(PyObject *self, PyObject *args) {
  char *source = NULL, *method = NULL;
  char *ret = "SUCCESS";
  PyObject *parent = NULL;
  char path[1024] = {0};
  GModule *module = NULL;

  if(!PyArg_ParseTuple(args, "ssO", &source, &method, &parent))
    return NULL;

  DEBUG("cfg_launch call method %s @ %s\n", method, source);
  module = g_module_open(source, G_MODULE_BIND_LAZY|G_MODULE_BIND_LOCAL);
  if(module) {
    typedef void (* launch_ui_func_t) (GtkWidget *);
    gpointer create_ui_func = NULL;

    if(g_module_symbol(module, method, &create_ui_func)) {
      launch_ui_func_t launch_ui = (launch_ui_func_t )create_ui_func;
      launch_ui(g_dialog);
    } else {
      DEBUG("can't found symbol %s to launch %s\n", method, source);
      ret = "FAILURE";
    }
    g_module_close(module);
  } else {
    DEBUG("launch external module failure, %s, %s\n", path,  strerror(errno));
    ret = "FAILURE";
  }

  return Py_BuildValue("s", ret);
}

static PyObject* py_cfg_set_action(PyObject *self, PyObject *args) {
  char *action = NULL;
  char *ret = "ACCEPT";

  if(!PyArg_ParseTuple(args, "s", &action))
    return NULL;

  if(strcmp(action, "reboot") == 0)
    g_sysflag |= SYSTEM_FLAG_REBOOT;
  else if(strcmp(action, "lock.noact") == 0)
    g_sysflag &= ~(SYSTEM_FLAG_REBOOT);
  else if(strcmp(action, "restartx") == 0) 
    g_sysflag |= SYSTEM_FLAG_RESTARTX;
  else if(strcmp(action, "langkb.noact") == 0)
    g_sysflag &= ~(SYSTEM_FLAG_RESTARTX);
  else if(strcmp(action, "restore.all") == 0) 
    g_sysflag |= SYSTEM_FLAG_RESTORE_ALL;
  else if(strcmp(action, "restore.plugins") == 0)
    g_sysflag |= SYSTEM_FLAG_RESTORE_PLUGINS;
  else if(strcmp(action, "restore.userdata") == 0)
    g_sysflag |= SYSTEM_FLAG_RESTORE_USERDATA;
  else if(strcmp(action, "restore.noact") == 0)
    g_sysflag &= ~(SYSTEM_FLAG_RESTORE_ALL|SYSTEM_FLAG_RESTORE_PLUGINS|SYSTEM_FLAG_RESTORE_USERDATA);
  else
    ret = "REJECT";

  DEBUG("SET system action %s %s\n", action, ret);

  return Py_BuildValue("s", ret);
}

static PyObject* py_cfg_get_pixbuf(PyObject *self, PyObject *args) {
  char *name = NULL;
  const guint8 *image = NULL;
  int i;

  if(!PyArg_ParseTuple(args, "s", &name))
    return NULL;

  for(i=0; g_py_image_array[i].name; i++) {
    if(strcmp(g_py_image_array[i].name, name) == 0) {
      image = g_py_image_array[i].image;
      break;
    }
  }

  if(image) {
    GError *err = NULL;
    GdkPixbuf *pixbuf = NULL;
    PyObject *_pypixbuf = NULL;

    pixbuf = gdk_pixbuf_new_from_inline(-1, image,  FALSE, &err);
    _pypixbuf = pygobject_new((GObject *)pixbuf);
    return Py_BuildValue("O", _pypixbuf);
  }

  return NULL;
}

static GtkWidget *_create_buildin_ui(const char *source, void *data) {
  GtkWidget *widget = NULL;

  widget = gtk_button_new_from_stock(GTK_STOCK_OK);
  return widget;
}

PyObject *call_embeded_python_func(const char *source, void *data) {
  PyObject *create_ui = NULL, *_pywidget = NULL;
  PyObject *pymod = NULL;

  if(pymod = PyImport_ImportModule((char *)source)) {
    PyObject *_pydialog = NULL, *_pyok = NULL, *_pycancel = NULL, *args = NULL, *_pyfakeok = NULL;

    create_ui = PyObject_GetAttrString(pymod, "create_ui");
    if(create_ui && PyCallable_Check(create_ui)) {
      _pyok = pygobject_new((GObject *)g_ok);
      _pydialog = pygobject_new((GObject *)g_dialog);
      _pycancel = pygobject_new((GObject *)g_cancel);
      _pyfakeok = pygobject_new((GObject *)g_fake_ok);

      args = Py_BuildValue("(OOOO)", _pydialog, _pyok, _pycancel, _pyfakeok);
      _pywidget = PyObject_CallObject(create_ui, args);

     if(PyErr_Occurred())
       PyErr_Print();
    }
    if(create_ui)
      Py_DECREF(create_ui);
    Py_DECREF(pymod);
  } else {
    DEBUG("import embeded python module FAILURE (source = %s).\n", source);
    if(PyErr_Occurred())
      PyErr_Print();
  }

  return _pywidget;
}

GtkWidget *call_external_module_func(const char *source, void *data) {
  GtkWidget *widget = NULL;
  char path[1024] = {0};
  GModule *module = NULL;

  snprintf(path, 1024, "%s/%s.so", DVMCFG_EXTERNAL_MODULE_PATH, source);
  module = g_module_open(path, G_MODULE_BIND_LAZY|G_MODULE_BIND_LOCAL);
  if(module) {
    gpointer create_ui_func = NULL;
    if(g_module_symbol(module, "cfg_create_ui", &create_ui_func)) {
      //DEBUG("cfg_create_ui entry address = %x\n", create_ui_func);
      create_ui_func_t func = (create_ui_func_t )create_ui_func;
      widget = func(g_dialog, g_ok, g_cancel);
    } else {
      DEBUG("can't found symbol cfg_create_ui to create widget\n");
    }
  } else {
    DEBUG("load external module failure, %s, %s\n", path,  strerror(errno));
  }

  return widget;
}

GtkWidget *call_buildin_func(const char *source, void *data) {
  int idx = 0;
  GtkWidget *widget = NULL;

  DEBUG("exec buildin function\n");
  for(; g_buildin_func[idx].func; idx++) {
    if(strcmp(g_buildin_func[idx].name, source) == 0) {
      widget = (* g_buildin_func[idx].func)(source, data);
      break;
    }
  }

  if(!widget)
    DEBUG("load buildin module failure (source = %s).\n", source);

  return widget;
}

typedef enum
{
	AUR_CORNER_NONE        = 0,
	AUR_CORNER_TOPLEFT     = 1,
	AUR_CORNER_TOPRIGHT    = 2,
	AUR_CORNER_BOTTOMLEFT  = 4,
	AUR_CORNER_BOTTOMRIGHT = 8,
	AUR_CORNER_ALL         = 15
} AuroraCorners;

static void clearlooks_rounded_rectangle (cairo_t * cr,
			      double x, double y, double w, double h,
			      double radius, gint corners) {
				
    if (radius < 0.01 || (corners == AUR_CORNER_NONE)) {
        cairo_rectangle (cr, x, y, w, h);
        return;
    }
	
    if (corners & AUR_CORNER_TOPLEFT)
        cairo_move_to (cr, x + radius, y);
    else
        cairo_move_to (cr, x, y);

    if (corners & AUR_CORNER_TOPRIGHT)
        cairo_arc (cr, x + w - radius, y + radius, radius, M_PI * 1.5, M_PI * 2);
    else
        cairo_line_to (cr, x + w, y);

    if (corners & AUR_CORNER_BOTTOMRIGHT)
        cairo_arc (cr, x + w - radius, y + h - radius, radius, 0, M_PI * 0.5);
    else
        cairo_line_to (cr, x + w, y + h);

    if (corners & AUR_CORNER_BOTTOMLEFT)
        cairo_arc (cr, x + radius, y + h - radius, radius, M_PI * 0.5, M_PI);
    else
        cairo_line_to (cr, x, y + h);

    if (corners & AUR_CORNER_TOPLEFT)
        cairo_arc (cr, x + radius, y + radius, radius, M_PI, M_PI * 1.5);
    else
        cairo_line_to (cr, x, y);

}

static gboolean _cfg_guest_on_expose (GtkWidget *widget, GdkEventExpose *expose) {
  cairo_t *cr;
  cairo_pattern_t *pattern;
  static cairo_surface_t *image = NULL;
  gint w = widget->allocation.width, h = widget->allocation.height;

  cr = gdk_cairo_create (widget->window);

  clearlooks_rounded_rectangle(cr, 0, 0, w, h, 0.0, AUR_CORNER_NONE);
  cairo_clip(cr);

#if 0
  if(image == NULL)
    image = cairo_image_surface_create_from_png("dvmcfg-bg.png");
  w = cairo_image_surface_get_width (image);
  h = cairo_image_surface_get_height (image);
  cairo_set_source_surface (cr, image, 0, 0);
#endif
#if 1
  pattern = cairo_pattern_create_linear(0.0, 0.0, 0, h);
  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0.02, 0.02, 0.02, 1.0);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0.36, 0.36, 0.36, 1.0);
  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);
#endif
  cairo_paint (cr);

  cairo_destroy (cr);

  return FALSE;
}

static void _append_label_widget(GtkWidget *vbox, char *markup, char *description, gboolean has_separator) {
  GtkWidget *hbox = NULL, *label = NULL;
  GtkWidget *vvbox = NULL, *vvvbox = NULL;

  vvbox = gtk_vbox_new(FALSE, 24);
  hbox = gtk_hbox_new(FALSE, 0);
  label = gtk_label_new("");
  gtk_label_set_markup(GTK_LABEL(label), markup);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
  gtk_box_pack_start(GTK_BOX(vvbox), hbox, FALSE, TRUE, 2);

  if(has_separator || description) {
    vvvbox = gtk_vbox_new(FALSE, 0);
    if(has_separator) {
      hbox = gtk_hbox_new(FALSE, 0);
      gtk_box_pack_start(GTK_BOX(hbox), gtk_hseparator_new(), TRUE, TRUE, 4);
      gtk_box_pack_start(GTK_BOX(vvvbox), hbox, FALSE, TRUE, 6);
    }
  
    if(description) {
      bzero(markup, 1024);
      snprintf(markup, 1024, "<span font_desc='Verdana 14'> %s </span>", description);
      hbox = gtk_hbox_new(FALSE, 0);
      label = gtk_label_new("");
      gtk_label_set_markup(GTK_LABEL(label), markup);
      gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
      gtk_box_pack_start(GTK_BOX(vvvbox), hbox, FALSE, TRUE, 2);
    }
    gtk_box_pack_start(GTK_BOX(vvbox), vvvbox, FALSE, TRUE, 2);
  }

  gtk_box_pack_start(GTK_BOX(vbox), vvbox, FALSE, TRUE, 0);
  gtk_widget_show_all(vvbox);

  gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
}

GtkWidget *patch_cfg_create_image_tab (GtkBox *mainbox) {
  GtkWidget *imagetab = NULL;
  GdkPixbuf *background = NULL;
  GError *err = NULL;

  background = gdk_pixbuf_new_from_inline(-1, imagetab_background_pixbuf, FALSE, &err);
  imagetab = gtk_image_tab_new();
  gtk_box_pack_start(GTK_BOX(mainbox), imagetab, FALSE, TRUE, 0);
  gtk_image_tab_set_background_pixbuf (GTK_IMAGE_TAB(imagetab), background);
  gtk_image_tab_append(GTK_IMAGE_TAB(imagetab), "hpcs_1125_03.jpg");
  gtk_image_tab_append(GTK_IMAGE_TAB(imagetab), "hpcs_1125_04.jpg");
  gtk_image_tab_append(GTK_IMAGE_TAB(imagetab), "hpcs_1125_05.jpg");
  gtk_image_tab_append(GTK_IMAGE_TAB(imagetab), "hpcs_1125_06.jpg");
  gtk_image_tab_append(GTK_IMAGE_TAB(imagetab), "hpcs_1125_07.jpg");
  gtk_image_tab_append(GTK_IMAGE_TAB(imagetab), "hpcs_1125_08.jpg");
  gtk_image_tab_append(GTK_IMAGE_TAB(imagetab), "hpcs_1125_09.jpg");
  gtk_widget_show(imagetab);

  return imagetab;
}

gint patch_cfg_create_tabstyle_ui (char *execpath, GtkWidget *it) {
  GtkWidget *widget = NULL;
  GtkWidget *vbox = NULL, *hbox = NULL, *icon = NULL;
  void *data = NULL;
  PyObject *_pywidget = NULL;
  GtkWidget *label = NULL;
  int idx = 0, curr_page = 0;
  gchar *exec = NULL;

  exec = strrchr(execpath, '/');
  if(! exec)
    exec = execpath;
  else
    exec = exec + 1;

  for(; g_pages[idx].type != PAGE_TYPE_NULL; idx++) {
    char markup[1024] = {0};
    GError *err = NULL;
    GdkPixbuf *pixbuf = NULL;
    vbox = NULL, widget = NULL;
    _pywidget = NULL;

    //printf("\n");
    //DEBUG("type = %x, source = %s, idx = %d\n", g_pages[idx].type, g_pages[idx].source, idx);
    g_pages[idx].guest = NULL;
    switch(g_pages[idx].type) {
      case PAGE_TYPE_BUILDIN:
        vbox = call_buildin_func(g_pages[idx].source, data);
        break;
      case PAGE_TYPE_EMBEDED_PYTHON:
      case PAGE_TYPE_EXTERNAL_MODULE:
        if(g_pages[idx].draw_label) {
          vbox = gtk_vbox_new(FALSE, 8);
          snprintf(markup, 1024, "<span font_desc='Verdana 18' weight='bold' > %s </span>", gettext(g_pages[idx].label));
          _append_label_widget(vbox, markup, gettext(g_pages[idx].description), g_pages[idx].has_separator);
        }
        if(g_pages[idx].type == PAGE_TYPE_EXTERNAL_MODULE)
          widget = call_external_module_func(g_pages[idx].source, data);
        else
          _pywidget = call_embeded_python_func(g_pages[idx].source, data);
        break;
    }

    pixbuf = gdk_pixbuf_new_from_inline(-1, g_pages[idx].icon,  FALSE, &err);
    icon = gtk_image_new_from_pixbuf (pixbuf);
    //gtk_widget_set_tooltip_text(icon, gettext(g_pages[idx].tooltip));

    if(widget) {
      if(vbox)
        gtk_box_pack_start(GTK_BOX(vbox), widget, TRUE, TRUE, 0);
      else
        vbox = widget;
    } else if(_pywidget) {
      widget = (GtkWidget *)pygobject_get(_pywidget);
      gtk_widget_show(widget);
      if(vbox)
        gtk_box_pack_start(GTK_BOX(vbox), widget, TRUE, TRUE, 0);
      else
        vbox = widget;
    }
    if(vbox) {
      g_object_ref(vbox);
      g_pages[idx].guest = vbox;
      if(strcmp(exec, g_pages[idx].source) == 0) {
        curr_page = idx;
        g_currpage = g_pages[idx].guest;
        DEBUG("matched page g_curr_page = %x, idx = %d\n", g_currpage, idx);
        gtk_image_tab_set_selected(GTK_IMAGE_TAB(it), idx);
      }
      gtk_widget_show(vbox);
    }

    if(_pywidget)
      Py_DECREF(_pywidget);
  }

  return curr_page;
}

gboolean _cfg_switch_notebook (GIOChannel *source, GIOCondition cond, gpointer data) {
  GError *err = NULL;
  gchar buf[1024] = {0};
  gint nread = 0;

  DEBUG("udp socket fd is notifyed, cond = %x\n", cond);
  if(cond & (G_IO_ERR|G_IO_HUP|G_IO_NVAL)) {
    DEBUG("udp socket fd is invalid\n");
    return FALSE;
  }
  if(cond & G_IO_IN) {
    gint idx = 0;
    struct sockaddr_un c_addr;
    socklen_t len;
    GIOStatus sta;

    nread = recvfrom(g_io_channel_unix_get_fd(source), buf, 1024, 0, (struct sockaddr *)&c_addr, &len);
    DEBUG("recvfrom.nread= %x, buf = %s\n", nread, buf);
    for(; g_pages[idx].source; idx++) {
      if(strstr(buf, g_pages[idx].source)) {
        if(idx == gtk_image_tab_get_selected(GTK_IMAGE_TAB(data))) {
          g_signal_emit_by_name(g_cancel, "clicked", G_TYPE_NONE, NULL);
        } else {
          gtk_image_tab_set_selected(GTK_IMAGE_TAB(data), idx);
          gdk_window_raise(gtk_widget_get_parent_window(GTK_WIDGET(data)));
        }
        break;
      }
    }
  }

  return TRUE;
}

static void _dvmcfg_fake_ok_clicked (GtkButton *btn, gpointer data) {
  gint *resp = (gint *)data;

  DEBUG("fake OK button clicked.\n");
  if ((g_sysflag & SYSTEM_FLAG_RESTORE) && (g_sysflag & SYSTEM_FLAG_RESTARTX)) {
    cfg_action_confirm_dialog_run(g_dialog, -1, _("Changes to these settings require a restart.\nRestart now ?"));
  } else if(g_sysflag & SYSTEM_FLAG_RESTORE) {
    cfg_action_confirm_dialog_run(g_dialog, 5, _("Changes to this setting require a reboot.\nReboot now ?"));
  } else if(g_sysflag & SYSTEM_FLAG_REBOOT) {
    DEBUG("Do reboot system NOW!\n");
    cfg_action_confirm_dialog_run(g_dialog, 3, _("Changes to this setting require a restart.\nRestart now ?"));
  }else if(g_sysflag & SYSTEM_FLAG_RESTARTX) {
    DEBUG("Do restart xserver JOB!\n");
    cfg_action_confirm_dialog_run(g_dialog, 1, _("Changes to this setting require a restart.\nRestart now ?"));
  } else {
    DEBUG("No system flag detected, emit clicked signal to fake OK button.\n");
    g_signal_emit_by_name(g_ok, "clicked", G_TYPE_NONE, NULL);
  }
}

static void _dvmcfg_ok_clicked (GtkButton *btn, gpointer data) {
  gint *resp = (gint *)data;

  DEBUG("hide OK button clicked.\n");
  *resp = GTK_RESPONSE_ACCEPT;

  gtk_main_quit();
}

static void _dvmcfg_cancel_clicked (GtkButton *btn, gpointer data) {
  gint *resp = (gint *)data;

  DEBUG("Cancel button clicked.\n");
  *resp = GTK_RESPONSE_REJECT;

  gtk_main_quit();
}

static gboolean _patch_cfg_query_tooltip(GtkWidget *it, gint x, gint y, gboolean keymode, GtkTooltip *tooltip, gpointer data) {
  //DEBUG("query_tooltip %d, %d, tooltip = %x\n", x, y, tooltip);
  g_object_set_data(G_OBJECT(it), "tooltip-object", (gpointer )tooltip);

  return FALSE;
}

static void _patch_cfg_hovered_tab(GtkWidget *it, gint idx, gpointer data) {
  GtkTooltip *tooltip = NULL;
  GdkRectangle r;

  tooltip = (GtkTooltip *)g_object_get_data(G_OBJECT(it), "tooltip-object");
  DEBUG("hovered tab to %d, tooltip= %x, text = %s\n", idx, tooltip, g_pages[idx].tooltip);

  if(tooltip) {
    r.x = 0, r.y = 0;
    gtk_tooltip_set_tip_area(tooltip, &r);
  }
  gtk_widget_set_tooltip_text(it, g_pages[idx].tooltip);
}

static void _patch_cfg_switch_tab(GtkWidget *it, gint idx, gpointer data) {
  GtkWidget *new = NULL, *old = NULL;

  if(g_currpage) {
    gtk_widget_hide(g_currpage);
    g_currpage = g_pages[idx].guest;
    if(! g_slist_find((GSList *)gtk_container_get_children(GTK_CONTAINER(data)), g_currpage)) {
      gtk_box_pack_start(GTK_BOX(data), g_currpage, TRUE, TRUE, 0);
    }
    if(idx != 2)
      gtk_widget_show_all(g_currpage);
    else
     gtk_widget_show(g_currpage);
  }
  DEBUG("switch tab to %d, guest = %x\n", idx, g_pages[idx].guest);
  //_dump_widget_path(GTK_WIDGET(data));
}

static void _acd_no_button_clicked(GtkButton *btn, gpointer data) {
  DEBUG("NO button clicked.\n");
  gtk_widget_destroy(GTK_WIDGET(data));
}

static void _acd_yes_button_clicked(GtkButton *btn, gpointer data) {
  g_signal_emit_by_name(g_ok, "clicked", G_TYPE_NONE, NULL);
  DEBUG("emit clicked signal to hide OK button\n");

  gtk_widget_destroy(GTK_WIDGET(data));
}

static void cfg_action_confirm_dialog_run(GtkWidget *parent, gint idx, const char *description) {
  GtkWidget *acd = NULL;
  GtkWidget *yes, *no = NULL;
  GtkWidget *hvhbox = NULL, *icon = NULL, *hbox = NULL, *hvbox = NULL, *vbox = NULL, *hhbox = NULL, *label = NULL;
  GdkPixbuf *pixbuf = NULL;
  GError *err = NULL;
  gchar markup [1024] = {0};

  acd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(acd), TRUE);
  vbox = gtk_vbox_new(FALSE, 2);

  hbox = gtk_hbox_new(TRUE, 0);
  hhbox = gtk_hbox_new(TRUE, 6);
  yes = gtk_button_new_with_label("Yes");
  label = gtk_bin_get_child(GTK_BIN(yes));
  snprintf(markup, 1024, "<span font_desc='Verdana 12' weight='bold' >%s</span>", _("Yes"));
  gtk_label_set_markup(GTK_LABEL(label), markup);
  gtk_widget_set_size_request(yes, -1, 36);
  gtk_box_pack_start(GTK_BOX(hhbox), yes, FALSE, TRUE, 0);
  g_signal_connect(yes, "clicked", G_CALLBACK(_acd_yes_button_clicked), acd);
  no = gtk_button_new_with_label("No");
  gtk_widget_set_size_request(no, -1, 36);
  label = gtk_bin_get_child(GTK_BIN(no));
  bzero(markup, 1024);
  snprintf(markup, 1024, "<span font_desc='Verdana 12' weight='bold' >%s</span>", _("No"));
  gtk_label_set_markup(GTK_LABEL(label), markup);
  gtk_box_pack_start(GTK_BOX(hhbox), no, FALSE, TRUE, 0);
  g_signal_connect(no, "clicked", G_CALLBACK(_acd_no_button_clicked), acd);
  gtk_box_pack_start(GTK_BOX(hbox), hhbox, FALSE, TRUE, 64);

  gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, TRUE, 2);

  gtk_box_pack_end(GTK_BOX(vbox), gtk_hseparator_new(), TRUE, TRUE, 4);

  pixbuf = gdk_pixbuf_new_from_inline(-1, g_pages[idx].icon,  FALSE, &err);
  icon = gtk_image_new_from_pixbuf (pixbuf);

  hvbox = gtk_vbox_new(FALSE, 2);
  hvhbox = gtk_hbox_new(FALSE, 8);
  gtk_box_pack_start(GTK_BOX(hvhbox), icon, FALSE, TRUE, 0);
  bzero(markup, 1024);
  snprintf(markup, 1024, "<span font_desc='Verdana 12'>%s</span>", description);
  label = gtk_label_new("");
  gtk_label_set_markup(GTK_LABEL(label), markup);
  gtk_box_pack_start(GTK_BOX(hvhbox), label, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hvbox), hvhbox, FALSE, TRUE, 20);

  hbox = gtk_hbox_new(FALSE, 8);
  gtk_box_pack_start(GTK_BOX(hbox), hvbox, FALSE, TRUE, 20);

  if(idx == -1) {
    bzero(&markup, 1024);
    snprintf(markup, "%s %s", gettext(g_pages[1].label), gettext(g_pages[3].label));
    gtk_window_set_title(GTK_WINDOW(acd), markup);
  } else {
    gtk_window_set_title(GTK_WINDOW(acd), gettext(g_pages[idx].label));
  }

  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 6);
  
  gtk_container_set_border_width(GTK_CONTAINER(acd), 4);
  gtk_container_add(GTK_CONTAINER(acd), vbox);
  gtk_window_set_modal(GTK_WINDOW(acd), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(acd), GTK_WINDOW(g_dialog));
  gtk_widget_show_all(acd);
}

static gboolean cfg_close_main_window(GtkWidget *widget, GdkEvent *event, gpointer data) {
  DEBUG("cfg_close_main_window\n");
  g_signal_emit_by_name(g_cancel, "clicked", G_TYPE_NONE, NULL);
  return TRUE;
}

int main(int argc, char *argv[]) {
  gint resp = GTK_RESPONSE_ACCEPT, lockfd = 0, udpfd = 0, ret, idx;
  GtkWidget *vbox = NULL, *label = NULL, *vhbox = NULL, *vhhbox = NULL, *vhvbox = NULL, *eventbox = NULL,  *it = NULL, *hostbox = NULL;
  GIOChannel *channel = NULL;
  struct sockaddr_un s_addr;

  if(access("/tmp/.dvmcfg.lock", F_OK) < 0) {
    lockfd = open("/tmp/.dvmcfg.lock", O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
  } else {
    lockfd = open("/tmp/.dvmcfg.lock", O_RDWR|O_EXCL, S_IRUSR|S_IWUSR);
  }
  if(flock(lockfd, LOCK_EX|LOCK_NB) < 0) {
    DEBUG("locked by dvmcfg, lockfd = %d\n", lockfd);
    struct sockaddr_un s_addr;
    gint sock, len = sizeof(struct sockaddr_un);
    
    sock = socket(AF_UNIX, SOCK_DGRAM, 0);

    bzero(&s_addr, sizeof(struct sockaddr_un));
    s_addr.sun_family = AF_UNIX;
    strncpy(s_addr.sun_path, DVMCFG_IPC_PATH, sizeof(s_addr.sun_path)-1);
    ret = sendto(sock, argv[1], strlen(argv[1]), 0, (struct sockaddr *)&s_addr, sizeof(s_addr));
    if(ret < 0)
      DEBUG("sock = %d, ret = %d, error = %s\n", sock, ret, strerror(errno));
    close(sock);
    close(lockfd);
    return 0;
  }

  DEBUG("BUILD TIMESTAMP @ "__TIME__", "__DATE__"\n");
  bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
  textdomain(GETTEXT_PACKAGE);

  setenv("PYTHONPATH", DVMCFG_EMBED_PYTHON_PATH, 1);
  gtk_init(&argc, &argv);
  Py_Initialize();
  pygobject_init(-1, -1, -1);
  Py_InitModule("dvmcfg", g_embed_method);

  DEBUG("Python Path = %s\n", Py_GetPath());

  //gtk_rc_parse();
  g_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(g_dialog), "Configuration");
  gtk_container_set_border_width(GTK_CONTAINER(g_dialog), 0);
  gtk_window_set_resizable(GTK_WINDOW(g_dialog), FALSE);
  g_signal_connect(G_OBJECT(g_dialog), "delete-event", G_CALLBACK(cfg_close_main_window), NULL);

  /*global button*/
  g_ok = gtk_button_new_from_stock(GTK_STOCK_OK);

  g_fake_ok = gtk_button_new_with_label("OK");
  label = gtk_bin_get_child(GTK_BIN(g_fake_ok));
  gtk_label_set_markup(GTK_LABEL(label), "<b>OK</b>");
  gtk_widget_set_size_request(g_fake_ok, -1, 26);

  g_cancel = gtk_button_new_with_label("Cancel");
  gtk_widget_set_size_request(g_cancel, 96, 26);
  label = gtk_bin_get_child(GTK_BIN(g_cancel));
  gtk_label_set_markup(GTK_LABEL(label), "<b>Cancel</b>");
  /**END of fake OK, OK, Cancel button create*/

  gtk_widget_set_size_request(g_dialog, 642, 663);
  //g_signal_connect (g_dialog, "destroy", G_CALLBACK(gtk_widget_destroyed), &g_dialog);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(g_dialog), vbox);

  vhbox = gtk_hbox_new(FALSE, 0);
  vhvbox = gtk_vbox_new(FALSE, 0);
  eventbox = gtk_event_box_new();
  hostbox = gtk_hbox_new(TRUE, 0);
  it = patch_cfg_create_image_tab(GTK_BOX(vhvbox));
  idx = patch_cfg_create_tabstyle_ui(argv[1], it);
  gtk_widget_set_app_paintable(eventbox, TRUE);
  g_signal_connect(eventbox, "expose-event", G_CALLBACK(_cfg_guest_on_expose), NULL);
  g_signal_connect(it, "query-tooltip", G_CALLBACK(_patch_cfg_query_tooltip), NULL);
  g_signal_connect(it, "tab-switched", G_CALLBACK(_patch_cfg_switch_tab), hostbox);
  g_signal_connect(it, "tab-hovered",  G_CALLBACK(_patch_cfg_hovered_tab), NULL);
  gtk_box_pack_start(GTK_BOX(vhvbox), eventbox, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vhbox), vhvbox, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, TRUE, TRUE, 0);

  /**attach OK & Cancel button to main Window*/
  vhbox = gtk_hbox_new(FALSE, 0);
  vhhbox = gtk_hbox_new(TRUE, 8);
  gtk_box_pack_start(GTK_BOX(vhhbox), g_fake_ok, TRUE, TRUE, 0);
  g_signal_connect(g_fake_ok, "clicked", G_CALLBACK(_dvmcfg_fake_ok_clicked), &resp);
  gtk_box_pack_start(GTK_BOX(vhhbox), g_ok, FALSE, FALSE, 0);
  g_signal_connect(g_ok, "clicked", G_CALLBACK(_dvmcfg_ok_clicked), &resp);
  gtk_box_pack_start(GTK_BOX(vhhbox), g_cancel, TRUE, TRUE, 0);
  g_signal_connect(g_cancel, "clicked", G_CALLBACK(_dvmcfg_cancel_clicked), &resp);
  gtk_box_pack_end(GTK_BOX(vhbox), vhhbox, FALSE, TRUE, 12);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, FALSE, TRUE, 8);

  if(strstr(argv[1], "networkcfg")) {
    /**attach current page content to main Window*/
    gtk_container_add(GTK_CONTAINER(eventbox), hostbox);
    gtk_widget_show_all(g_dialog);
    gtk_container_add(GTK_CONTAINER(hostbox), g_currpage);
  } else {
    /**attach current page content to main Window*/
    gtk_container_add(GTK_CONTAINER(eventbox), hostbox);
    gtk_container_add(GTK_CONTAINER(hostbox), g_currpage);
    gtk_widget_show_all(g_dialog);
  }

  gtk_widget_hide(g_ok);

  /**create UDP socket for Embeded Gtk Application*/
  unlink(DVMCFG_IPC_PATH);
  udpfd = socket(AF_UNIX, SOCK_DGRAM, 0);
  bzero(&s_addr, sizeof(struct sockaddr_un));
  s_addr.sun_family = AF_UNIX;
  strncpy(s_addr.sun_path, DVMCFG_IPC_PATH, sizeof(s_addr.sun_path)-1);
  ret = bind(udpfd, (struct sockaddr *)&s_addr, sizeof(s_addr));
  if(ret < 0) {
    DEBUG("bind updfd failure, error = %s\n", strerror(errno));
  } else {
    channel = g_io_channel_unix_new(udpfd);
    g_io_add_watch(channel, G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL, _cfg_switch_notebook, it);
    g_io_channel_unref(channel);
  }

  gtk_main();

  if(udpfd) {
    close(udpfd);
  }
  gtk_widget_destroy(g_dialog);
  Py_Finalize();

  if(lockfd) {
    flock(lockfd, LOCK_UN);
    close(lockfd);
  }

  DEBUG("resp = %s\n", resp == GTK_RESPONSE_REJECT ? "GTK_RESPONSE_REJECT" : "GTK_RESPONSE_ACCEPT");
  if(resp == GTK_RESPONSE_ACCEPT) {
    if(g_sysflag & SYSTEM_FLAG_RESTORE) {
      DEBUG("Do restore system JOB!\n");
      if(g_sysflag & SYSTEM_FLAG_RESTORE_ALL) {
        system("bbdock -x /usr/bin/dvm-config/runUser.py");
      }
    } else if(g_sysflag & SYSTEM_FLAG_REBOOT) {
      DEBUG("Do reboot system NOW!\n");
      system("bbdock -x /usr/bin/dvm-config/X_quit");
    }else if(g_sysflag & SYSTEM_FLAG_RESTARTX) {
      DEBUG("Do restart xserver JOB!\n");
      system("bbdock -x /usr/bin/dvm-config/X_quit");
    }
  }

  return 0;
}
