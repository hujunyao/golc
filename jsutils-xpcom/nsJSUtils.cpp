#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#define GETTEXT_PACKAGE "diskinfo"
#define LOCALEDIR "/usr/share/locale"

#include <glib/gi18n-lib.h>
#include <libintl.h>

#include "nscore.h"
#include "nsIGenericFactory.h"
#include "nsIBaseWindow.h"
#include "nsJSUtils.h"
#include "_inline_pixbuf.h"

#define BUFSIZE 1024
#define ERROR(...) fprintf(stderr, "[XXX] "__VA_ARGS__)
#define DEBUG(...) fprintf(stderr, "[III] "__VA_ARGS__)

Atom delete_window = XInternAtom (GDK_DISPLAY(), "WM_DELETE_WINDOW", False);

typedef struct {
  gchar *key, *value;
} kv_pair_t;

static void _kv_pair_dump(gpointer data, gpointer userdata) {
  kv_pair_t *kvp = (kv_pair_t *)data;
  DEBUG("skey = %s akey = %s\n", kvp->key, kvp->value);
}

static void _kv_pair_free(gpointer data, gpointer userdata) {
  kv_pair_t *kvp = (kv_pair_t *)data;
  if(kvp) {
    DEBUG("_kv_pair_free: skey = %s akey = %s\n", kvp->key, kvp->value);
    if(kvp->key) g_free(kvp->key);
    if(kvp->value) g_free(kvp->value);
    g_free(kvp);
  }
}

static void _app_watch_cb(GPid pid, gint status, gpointer data) {
  PRBool ret = TRUE;
  nsJSUtils *obj = (nsJSUtils *)data;

  DEBUG("app exit with status %d\n", status);
  obj->m_AppWatch->Call(0, status, &ret);
}

static gboolean launch_app_async(gchar *argv[], GPid *pid, gint flag) {
  gboolean ret;
  GError *err = NULL;
  gint spawn = (GSpawnFlags)(flag|G_SPAWN_SEARCH_PATH);

  ret = g_spawn_async(NULL, argv, NULL, (GSpawnFlags)spawn, NULL, NULL, pid, &err);
  if(ret == FALSE) {
    DEBUG("**LAUNCH APP %s %s failed**\n", argv[0], argv[1]);
  }

  return ret;
}

nsJSUtils::nsJSUtils() {
  this->m_AppWatch = NULL;
  this->m_Tray = NULL;
  this->m_MainWindow = NULL;
  this->m_EventFilter = NULL;
  this->m_PopupMenuCallback = NULL;
  this->m_KeyValue = NULL;
}

nsresult nsJSUtils::Init() {
  DEBUG("JSUtils->init\n");
	return NS_OK;
}

nsJSUtils::~nsJSUtils() {
  g_slist_foreach(this->m_KeyValue, _kv_pair_free, NULL);
}

static void tray_icon_activate(GtkStatusIcon *icon, gpointer data) {
  nsJSUtils *obj = (nsJSUtils *)data;
  if(obj->m_MainWindow) {
    if(gdk_window_is_visible(obj->m_MainWindow)) 
      gdk_window_hide(obj->m_MainWindow);
    else {
      GdkWindowState sta = gdk_window_get_state(obj->m_MainWindow);
      gpointer data = NULL;
      GValue val = {0};
      gdk_window_get_user_data(obj->m_MainWindow, &data);
      g_value_init(&val, G_TYPE_BOOLEAN);
      g_value_set_boolean(&val, TRUE);
      g_object_set_property(G_OBJECT(data), "visible", &val);
      DEBUG("window status = %x, title = %s\n", sta, gtk_window_get_title(GTK_WINDOW(data)));
      gdk_window_show(obj->m_MainWindow);
      gdk_window_raise(obj->m_MainWindow);
      if(sta & GDK_WINDOW_STATE_ICONIFIED)
        gdk_window_deiconify(obj->m_MainWindow);
    }
  }
}

static void popmenu_activated (GtkCheckMenuItem *item, gpointer data) {
  gchar *menu = (gchar *)g_object_get_data(G_OBJECT(item), "id");
  nsJSUtils *obj = (nsJSUtils *)data;
  PRBool ret = TRUE;

  DEBUG("menu = %s is activated\n", menu);
  if(obj->m_PopupMenuCallback) {
    obj->m_PopupMenuCallback->Call(0, menu, &ret);
    DEBUG("JS.menu activated = %d\n", ret);
  }
}

static GdkWindow *_get_gdk_window(nsIBaseWindow *baseWnd) {
  nsresult rv;
  nativeWindow aNativeWindow;
  GdkWindow *gdkWnd = NULL;

  rv = baseWnd->GetParentNativeWindow(&aNativeWindow);
  gdkWnd = reinterpret_cast<GdkWindow*>(aNativeWindow);
  DEBUG("baseWnd = %x, rv = %x, gdkWnd = %x.\n", (unsigned int )baseWnd, rv, (unsigned int )gdkWnd);

  return gdk_window_get_toplevel(gdkWnd);
}

static void tray_icon_popmenu(GtkStatusIcon *icon, guint button, guint32 activate_time, gpointer data) {
  GtkWidget *menu, *menuitem;

  menu = gtk_menu_new();
  gtk_menu_set_screen(GTK_MENU (menu), gtk_status_icon_get_screen (icon));
  menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
  g_signal_connect (menuitem, "activate", G_CALLBACK (popmenu_activated), data);
  g_object_set_data(G_OBJECT(menuitem), (const char *)"id", (gpointer)"QUIT");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new());

  menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
  g_signal_connect (menuitem, "activate", G_CALLBACK (popmenu_activated), data);
  g_object_set_data(G_OBJECT(menuitem), (const char *)"id", (gpointer)"PREFERENCES");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

  menuitem = gtk_menu_item_new_with_mnemonic("_Add Mail Account");
  g_signal_connect (menuitem, "activate", G_CALLBACK (popmenu_activated), data);
  g_object_set_data(G_OBJECT(menuitem), (const char *)"id", (gpointer)"ADDACCT");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

  menuitem = gtk_menu_item_new_with_mnemonic("_New Address Card");
  g_signal_connect (menuitem, "activate", G_CALLBACK (popmenu_activated), data);
  g_object_set_data(G_OBJECT(menuitem), (const char *)"id", (gpointer)"ADDCARD");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new());

  menuitem = gtk_menu_item_new_with_mnemonic("_Get Messages");
  g_signal_connect (menuitem, "activate", G_CALLBACK (popmenu_activated), data);
  g_object_set_data(G_OBJECT(menuitem), (const char *)"id", (gpointer)"GETMSGS");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

  menuitem = gtk_menu_item_new_with_mnemonic("_New Message");
  g_signal_connect (menuitem, "activate", G_CALLBACK (popmenu_activated), data);
  g_object_set_data(G_OBJECT(menuitem), (const char *)"id", (gpointer)"NEWMSG");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

  gtk_widget_show_all(menu);

  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, gtk_status_icon_position_menu, icon, button, activate_time);
}

NS_IMETHODIMP nsJSUtils::ShowTray(nsIBaseWindow *mainWnd) {
  if(! this->m_Tray) {
    GdkPixbuf *trayicon = NULL;
    trayicon = gdk_pixbuf_new_from_inline(-1, mail_tray_icon_pixbuf, FALSE, NULL);
    this->m_Tray = gtk_status_icon_new_from_pixbuf (trayicon);
    gtk_status_icon_set_screen (this->m_Tray, gdk_screen_get_default());
    gtk_status_icon_set_blinking (this->m_Tray, FALSE);
    gtk_status_icon_set_tooltip (this->m_Tray, "Splashtop email client");
    gtk_status_icon_set_visible (this->m_Tray, TRUE);
    g_signal_connect (this->m_Tray, "activate", G_CALLBACK(tray_icon_activate), this);
    g_signal_connect (this->m_Tray, "popup-menu", G_CALLBACK(tray_icon_popmenu), this);
    this->m_MainWindow = _get_gdk_window(mainWnd);
  } else {
    gtk_status_icon_set_visible (this->m_Tray, TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP nsJSUtils::HideTray() {
  if(this->m_Tray) {
    gtk_status_icon_set_visible (this->m_Tray, FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP nsJSUtils::DoWindowAction(nsIBaseWindow *mainWnd, const char *action) {
  GdkWindow *window = _get_gdk_window(mainWnd);

  if(window) {
    if(strncmp(action, "MIN", 3) == 0) {
      gdk_window_iconify(window);
      //tray_icon_activate(NULL, this);
    } else if(strncmp(action, "SHOW", 4) == 0) {
      GdkWindowState sta = gdk_window_get_state(this->m_MainWindow);
      gpointer data = NULL;
      GValue val = {0};
      gdk_window_get_user_data(this->m_MainWindow, &data);
      g_value_init(&val, G_TYPE_BOOLEAN);
      g_value_set_boolean(&val, TRUE);
      g_object_set_property(G_OBJECT(data), "visible", &val);
      DEBUG("window status = %x, title = %s\n", sta, gtk_window_get_title(GTK_WINDOW(data)));
      gdk_window_show(this->m_MainWindow);
      gdk_window_raise(this->m_MainWindow);
      if(sta & GDK_WINDOW_STATE_ICONIFIED)
        gdk_window_deiconify(this->m_MainWindow);
    } else if(strncmp(action, "TRAYCLICK", 9) == 0) {
      if(gdk_window_is_visible(this->m_MainWindow)) {
        GdkWindowState sta = gdk_window_get_state(window);
        if(sta & GDK_WINDOW_STATE_ICONIFIED) {
          gdk_window_deiconify(window);
        } else {
          gdk_window_iconify(window);
        }
      } else {
        GdkWindowState sta = gdk_window_get_state(this->m_MainWindow);
        gpointer data = NULL;
        GValue val = {0};
        gdk_window_get_user_data(this->m_MainWindow, &data);
        g_value_init(&val, G_TYPE_BOOLEAN);
        g_value_set_boolean(&val, TRUE);
        g_object_set_property(G_OBJECT(data), "visible", &val);
        DEBUG("window status = %x, title = %s\n", sta, gtk_window_get_title(GTK_WINDOW(data)));
        gdk_window_show(this->m_MainWindow);
        gdk_window_raise(this->m_MainWindow);
      }
    }
  }

  return NS_OK;
}

GdkFilterReturn window_event_filter(GdkXEvent *xevent, GdkEvent *event, gpointer data) {
  if(!data || !xevent) return GDK_FILTER_CONTINUE;
  
  XEvent *e=(XEvent *)xevent;
  PRBool ret = TRUE;
  
  if(e->xany.type == ClientMessage ) {
    if(e->xclient.data.l && (unsigned int)(e->xclient.data.l[0])==delete_window) {
       nsJSUtils *obj = (nsJSUtils *)data;
       if(obj->m_EventFilter) {
         obj->m_EventFilter->Call(0, "CLOSE", &ret);
         DEBUG("return from JS.eventFilter = %d\n", ret);
       }
       if(ret == FALSE) {
         if(obj->m_MainWindow && gdk_window_is_visible(obj->m_MainWindow)) {
           //tray_icon_activate(NULL, obj);
           //gdk_window_hide(obj->m_MainWindow);
           gdk_window_iconify(obj->m_MainWindow);
           DEBUG("gdk_window_iconify instead of close window\n");
         }
         return GDK_FILTER_REMOVE;
       }
    }
  } else if(e->xany.type == VisibilityNotify) {
  }
  
  return GDK_FILTER_CONTINUE;
}

NS_IMETHODIMP nsJSUtils::SetTrayPopupMenuCallback(nsITrayPopupMenuCallback *aCallback) {
  this->m_PopupMenuCallback = aCallback;

  DEBUG("C.SetTrayPopupMenuCallback\n");

  return NS_OK;
}

NS_IMETHODIMP nsJSUtils::SetGtkSettingsLongProp(const char *name, PRInt32 val) {
  glong v = val;
  GtkSettings *settings = NULL;

  settings = gtk_settings_get_default();
  gtk_settings_set_long_property(settings, name, v, "XPCOM");
  DEBUG("set Gtksettings::%s = %ld\n", name, v);
  return NS_OK;
}

NS_IMETHODIMP nsJSUtils::SetWindowEventFilter(nsIBaseWindow *aBaseWindow, nsIWindowEventFilterCallback *aCallback) {
  GdkWindow *window = _get_gdk_window(aBaseWindow);
  GdkEventMask m=(GdkEventMask)( GDK_VISIBILITY_NOTIFY_MASK | (long) gdk_window_get_events (window)) ;
  this->m_EventFilter = aCallback;

  DEBUG("C.SetWindowEventFilter\n");
  gdk_window_set_events(window, m);
  gdk_window_set_keep_below(window, TRUE);
  gdk_window_add_filter (window, window_event_filter, this);

  return NS_OK;
}

NS_IMETHODIMP nsJSUtils::Launch(const char *exec, const char *args, PRBool async, nsIAppWatchCallback *aCallback, PRBool *rv) {
  gchar **argv;
  GPid pid;

  DEBUG("exec = %s, args = %s\n", exec, args);
  this->m_AppWatch = aCallback;
  argv = g_strsplit(args, " ", 0);
  if(launch_app_async(argv, &pid, G_SPAWN_DO_NOT_REAP_CHILD)) {
    g_child_watch_add(pid, _app_watch_cb, (gpointer)this);
  }
  g_strfreev(argv);

  return NS_OK;
}

NS_IMETHODIMP nsJSUtils::AddKeyValue(const char *skey, const char *akey) {
  kv_pair_t *kvp = g_new0(kv_pair_t, 1);
  kvp->key = g_strdup(skey);
  kvp->value = g_strdup(akey);

  this->m_KeyValue = g_slist_append(this->m_KeyValue, kvp);

  return NS_OK;
}

NS_IMETHODIMP nsJSUtils::RemoveKeyValue(const char *skey) {
  GSList *elem = NULL;
  kv_pair_t *kvp = NULL;

  for(elem = this->m_KeyValue; elem; elem = g_slist_next(elem)) {
    kvp = (kv_pair_t *)(elem->data);
    if(kvp) {
      if((strcmp(kvp->key, skey) == 0)) {
        DEBUG("found %s in list, try to remove it.\n", skey);
        this->m_KeyValue = g_slist_remove(this->m_KeyValue, kvp);
        if(kvp->key) g_free(kvp->key);
        if(kvp->value) g_free(kvp->value);
        g_free(kvp);
        break;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsJSUtils::DumpKeyValue() {
  g_slist_foreach(this->m_KeyValue, _kv_pair_dump, NULL);
  return NS_OK;
}

NS_IMETHODIMP nsJSUtils::GetValueByKey(const char *skey, char **rv) {
  GSList *elem = NULL;
  kv_pair_t *kvp = NULL;

  *rv = NULL;
  for(elem = this->m_KeyValue; elem; elem = g_slist_next(elem)) {
    kvp = (kv_pair_t *)(elem->data);
    if(kvp) {
      if((strcmp(kvp->key, skey) == 0)) {
        DEBUG("found %s in list, value = %s.\n", skey, kvp->value);
        *rv = g_strdup(kvp->value);
        break;
      }
    }
  }

  return NS_OK;
}

static gboolean _dwn_window_close(GtkWidget *widget, GdkEvent *event, gpointer data) {
  DEBUG("_dwn_window_close\n");
  return FALSE;
}

/**static void _dwn_cancel_clicked (GtkButton *widget, gpointer data) {
  nsJSUtils *obj = (nsJSUtils *)data;

  DEBUG("_dwn_cancel_clicked\n");
  gtk_widget_destroy(obj->m_DWNWindow);
}*/

static void _dwn_ok_clicked (GtkButton *widget, gpointer data) {
  nsJSUtils *obj = (nsJSUtils *)data;
  PRBool ret = TRUE;
  gchar args[1024] = {0};
  gint num = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(obj->m_DWNChkButton));

  DEBUG("_dwn_ok_clicked\n");
  if(obj->m_Cleanup) {
    sprintf(args, "NOTIFY_ME_NEXT_STARTUP,%d", num);
    obj->m_Cleanup->Call(0, args, &ret);
  }

  gtk_widget_destroy(obj->m_DWNWindow);


}

/**static GtkWidget *_dwn_cleanup_message_ui(GtkWidget *box, gint total) {
  GtkWidget *hbox = NULL, *hhbox = NULL;
  GtkWidget *chkbox = NULL;

  hbox = gtk_hbox_new(FALSE, 2);
  hhbox = gtk_hbox_new(FALSE, 2);
  chkbox = gtk_check_button_new_with_label(_("Notify me next startup"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkbox), total);
  gtk_box_pack_start(GTK_BOX(hhbox), chkbox, FALSE, FALSE, 0);

  label = gtk_label_new("");
  gtk_label_set_markup(GTK_LABEL(label), "Delete all but the most recent");
  gtk_box_pack_start(GTK_BOX(hhbox), label, FALSE, FALSE, 0);
  spin = gtk_spin_button_new_with_range(0.0, total, total/100.00);
  gtk_box_pack_start(GTK_BOX(hhbox), spin, FALSE, FALSE, 2);
  label = gtk_label_new("messages");
  gtk_box_pack_start(GTK_BOX(hhbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), hhbox, TRUE, TRUE, 76);

  gtk_box_pack_start(GTK_BOX(box), hbox, TRUE, TRUE, 2);
  return chkbox;
}*/

NS_IMETHODIMP nsJSUtils::GetPO(const char *domain, const char *enstr, PRUnichar **rv) {
  const gchar *mystr = NULL;

  mystr = dgettext(domain, enstr);
  *rv = g_utf8_to_utf16(mystr, -1, NULL, NULL, NULL);

  return NS_OK;
}

NS_IMETHODIMP nsJSUtils::GetPO2(const char *domain, const char *enstr, PRInt32 val, PRUnichar **rv) {
  const gchar *mystr = NULL;
  gchar newstr[2048] = {0};

  mystr = dgettext(domain, enstr);
  g_snprintf(newstr, 2048, mystr, val);
  *rv = g_utf8_to_utf16(newstr, -1, NULL, NULL, NULL);

  return NS_OK;
}

NS_IMETHODIMP nsJSUtils::ShowDiskWarningNotification(nsIBaseWindow *aParent, PRUint32 pcnt, nsIMsgCleanupCallback *aCallback, PRUint32 totalMsg) {
  GdkWindow *parent = _get_gdk_window(aParent);
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  GtkWidget *okbtn = NULL/*, *ccbtn = NULL*/, *label = NULL;
  GtkWidget *vbox = NULL, *vhbox = NULL, *vhhbox = NULL;
  gchar description[1024] = {0};
  gpointer data = NULL;

  this->m_Cleanup = aCallback;
  this->m_DWNWindow = window;
  gtk_window_set_title(GTK_WINDOW(window), _("Disk Warning Notification"));
  gdk_window_get_user_data(parent, &data);
  gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(data)); 
  gtk_container_set_border_width(GTK_CONTAINER(window), 2);
  gtk_window_set_modal(GTK_WINDOW(window), TRUE);
  gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
  g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(_dwn_window_close), this);

  vbox = gtk_vbox_new(FALSE, 0);

  vhbox = gtk_hbox_new(FALSE, 0);
  vhhbox = gtk_hbox_new(FALSE, 4);
  gtk_box_pack_start(GTK_BOX(vhhbox), gtk_image_new_from_stock(GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DIALOG), FALSE, FALSE, 0);
  label = gtk_label_new("");
  {
    gchar tmpbuf[1024] = {0};
    snprintf(tmpbuf, 1024, _("Disk space is less than %d%c, please DELETE some messages."), pcnt, '%');
    snprintf(description, 1024, "<b>%s</b>", tmpbuf);
  }
  gtk_label_set_markup(GTK_LABEL(label), description);
  gtk_box_pack_start(GTK_BOX(vhhbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vhbox), vhhbox, TRUE, TRUE, 16);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, FALSE, TRUE, 4);

  this->m_DWNChkButton = gtk_check_button_new_with_label(_("Show warning dialog next startup"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(this->m_DWNChkButton), totalMsg);
  //gtk_widget_set_can_focus(this->m_DWNChkButton, FALSE);

  gtk_box_pack_start(GTK_BOX(vbox), gtk_hseparator_new(), FALSE, FALSE, 0);

  vhbox = gtk_hbox_new(FALSE, 0);
  vhhbox = gtk_hbox_new(FALSE, 2);
  //ccbtn = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
  okbtn = gtk_button_new_from_stock(GTK_STOCK_OK);
  //gtk_widget_set_can_focus(okbtn, TRUE);
  //gtk_widget_set_can_default(okbtn, TRUE);
  gtk_widget_grab_focus(okbtn);
  gtk_window_set_default(GTK_WINDOW(window), okbtn);
  //gtk_box_pack_start(GTK_BOX(vhhbox), ccbtn, TRUE, TRUE, 0);
  //g_signal_connect(ccbtn, "clicked", G_CALLBACK(_dwn_cancel_clicked), this);
  gtk_box_pack_start(GTK_BOX(vhhbox), this->m_DWNChkButton, TRUE, TRUE, 32);
  gtk_box_pack_start(GTK_BOX(vhhbox), okbtn, TRUE, TRUE, 0);
  g_signal_connect(okbtn, "clicked", G_CALLBACK(_dwn_ok_clicked), this);
  gtk_box_pack_end(GTK_BOX(vhbox), vhhbox, TRUE, TRUE, 16);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, FALSE, TRUE, 8);
  
  gtk_container_add(GTK_CONTAINER(window), vbox);

  gtk_widget_show_all(window);

  return NS_OK;
}

NS_IMETHODIMP nsJSUtils::ShowAccountWarningDialog(nsIBaseWindow *aParent, PRInt32 maxAcct) {
  GdkWindow *parent = _get_gdk_window(aParent);
  gpointer data = NULL;
  gdk_window_get_user_data(parent, &data);

  GtkWidget *window = gtk_dialog_new_with_buttons("", GTK_WINDOW(data), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
  GtkWidget *label = NULL;
  GtkWidget *vbox = NULL, *vhbox = NULL, *vhhbox = NULL;
  gchar description[1024] = {0};

  gtk_window_set_modal(GTK_WINDOW(window), TRUE);
  gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

  vbox = gtk_vbox_new(FALSE, 0);
  vhbox = gtk_hbox_new(FALSE, 0);
  vhhbox = gtk_hbox_new(FALSE, 4);
  gtk_box_pack_start(GTK_BOX(vhhbox), gtk_image_new_from_stock(GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DIALOG), FALSE, FALSE, 0);
  label = gtk_label_new("");
  snprintf(description, 1024, "%s %d", _("You cann't create account more than "), maxAcct);
  gtk_label_set_markup(GTK_LABEL(label), description);
  gtk_box_pack_start(GTK_BOX(vhhbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vhbox), vhhbox, TRUE, TRUE, 16);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, FALSE, TRUE, 4);
  gtk_container_add (GTK_CONTAINER(GTK_DIALOG(window)->vbox), vbox);

  gtk_widget_show_all(window);

  gtk_dialog_run (GTK_DIALOG (window));
  gtk_widget_destroy (window);

  return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsJSUtils, Init)

static NS_METHOD nsJSUtilsRegProc (nsIComponentManager *aCompMgr,
                                  nsIFile *aPath,
                                  const char *registryLocation,
                                  const char *componentType,
                                  const nsModuleComponentInfo *info) {


	DEBUG("nsJSUtilsRegProc\n");
    return NS_OK;
}

static NS_METHOD nsJSUtilsUnregProc (nsIComponentManager *aCompMgr,
                                    nsIFile *aPath,
                                    const char *registryLocation,
                                    const nsModuleComponentInfo *info) {
	DEBUG("nsJSUtilsUnregProc\n");
    return NS_OK;
}


NS_IMPL_ISUPPORTS1(nsJSUtils, nsIJSUtils)

static const nsModuleComponentInfo components[] = {
	{
		NS_IJSUTILS_CLASSNAME, NS_IJSUTILS_CID, NS_IJSUTILS_CONTRACTID,
		nsJSUtilsConstructor, nsJSUtilsRegProc, nsJSUtilsUnregProc
	}
};

NS_IMPL_NSGETMODULE(nsJSUtils, components)
