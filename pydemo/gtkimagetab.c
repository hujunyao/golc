#include "gtkimagetab.h"

#define DEBUG(...) fprintf(stdout, "[GTK_IMAGE_TAB] "__VA_ARGS__)
#define EF         //DEBUG("enter function %s\n", __func__)
#define LF         //DEBUG("leave function %s\n", __func__)

#define GTK_IMAGE_TAB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), GTK_TYPE_IMAGE_TAB, GtkImageTabPrivate))

static void _gtk_image_tab_realize (GtkWidget *self);
static void _gtk_image_tab_unrealize (GtkWidget *self);

static gboolean _gtk_image_tab_expose(GtkWidget *, GdkEventExpose *);
static gboolean _gtk_image_tab_button_press(GtkWidget *, GdkEventButton *);
static gboolean _gtk_image_tab_button_release(GtkWidget *self, GdkEventButton *event);
static gboolean _gtk_image_tab_motion_notify(GtkWidget *, GdkEventMotion *);
static gboolean _gtk_image_tab_leave_notify(GtkWidget *, GdkEventCrossing *);
void gtk_image_tab_set_selected (GtkImageTab *self, gint index);

static void _gtk_image_tab_size_request(GtkWidget *self, GtkRequisition *requisition);
static void _gtk_image_tab_size_allocate(GtkWidget *self, GtkAllocation *allocation);

enum {
  ICON_STA_UP,
  ICON_STA_DOWN,
  ICON_STA_OVER,
  ICON_STA_SELECTED
};

typedef struct {
  GdkPixbuf *iconset[4];
} GtkImageTabChild;

typedef struct {
  GdkPixbuf *backgroung;
  gint selected, hovered, pressed;
  GSList *child;
} GtkImageTabPrivate;

enum {
  PROP_0,
  PROP_SELECTED,
  PROP_HOVERED,
  PROP_PIXBUF
};

enum {
  TAB_ADDED,
  TAB_SWITCHED,
  TAB_CHANGED,
  TAB_SELECT,
  TAB_HOVERED,
  LAST_SIGNAL
};

G_DEFINE_TYPE (GtkImageTab, gtk_image_tab, GTK_TYPE_WIDGET);

static guint gtk_image_tab_signals[LAST_SIGNAL] = {0};

static void gtk_image_tab_get_property (GObject *object, guint id, GValue *value, GParamSpec *pspec) {
  switch(id) {
    case PROP_SELECTED:
    g_value_set_int(value, gtk_image_tab_get_selected(GTK_IMAGE_TAB(object)));
    break;

    case PROP_HOVERED:
    g_value_set_int(value, gtk_image_tab_get_hovered(GTK_IMAGE_TAB(object)));
    break;

    default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

static void gtk_image_tab_set_property (GObject *object, guint id, const GValue *value, GParamSpec *pspec) {
  switch(id) {
    case PROP_SELECTED:
    gtk_image_tab_set_selected(GTK_IMAGE_TAB(object), g_value_get_int(value));
    break;

    default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }
}

#if 0
GtkType gtk_image_tab_get_type(void) {
  static GtkType gtk_image_tab_type = 0;

  if(! gtk_image_tab_type) {
    static const GtkTypeInfo gtk_image_tab_info = {
      "GtkImageTab", sizeof(GtkImageTab), sizeof(GtkImageTabClass),
      (GtkClassInitFunc) gtk_image_tab_class_init,
      (GtkObjectInitFunc) gtk_image_tab_init,
      NULL, NULL, (GtkClassInitFunc) NULL
    };
    gtk_image_tab_type = gtk_type_unique(GTK_TYPE_DRAWING_AREA, &gtk_image_tab_info);
  }

  return gtk_image_tab_type;
}
#endif

static void gtk_image_tab_dispose (GObject *object) {
  EF;
  if(G_OBJECT_CLASS(gtk_image_tab_parent_class)->dispose)
    G_OBJECT_CLASS(gtk_image_tab_parent_class)->dispose(object);
  LF;
}

static void gtk_image_tab_finalize (GObject *object) {
  G_OBJECT_CLASS(gtk_image_tab_parent_class)->finalize(object);
}

static void gtk_image_tab_class_init(GtkImageTabClass *kclass) {
  GObjectClass *obj_class;
  GtkWidgetClass *widget_class;

  EF;
  g_type_class_add_private(kclass, sizeof(GtkImageTabPrivate));

  obj_class = G_OBJECT_CLASS(kclass);
  widget_class = GTK_WIDGET_CLASS(kclass);

  obj_class->set_property = gtk_image_tab_set_property;
  obj_class->get_property = gtk_image_tab_get_property;

  obj_class->dispose = gtk_image_tab_dispose;
  obj_class->finalize = gtk_image_tab_finalize;

  widget_class->size_request          = _gtk_image_tab_size_request;
  widget_class->size_allocate         = _gtk_image_tab_size_allocate;
  widget_class->realize               = _gtk_image_tab_realize;
  widget_class->unrealize             = _gtk_image_tab_unrealize;
  widget_class->expose_event          = _gtk_image_tab_expose;
  widget_class->button_press_event    = _gtk_image_tab_button_press;
  widget_class->button_release_event  = _gtk_image_tab_button_release;
  widget_class->motion_notify_event   = _gtk_image_tab_motion_notify;
  widget_class->leave_notify_event    = _gtk_image_tab_leave_notify;

  gtk_image_tab_signals[TAB_ADDED] = g_signal_new ("tab-added",
    G_OBJECT_CLASS_TYPE(obj_class), G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION,
    0, NULL, NULL,
    g_cclosure_marshal_VOID__OBJECT,
    G_TYPE_NONE, 1, GTK_TYPE_WIDGET);

  gtk_image_tab_signals[TAB_SWITCHED] = g_signal_new ("tab-switched",
    G_OBJECT_CLASS_TYPE(obj_class), G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION,
    0, NULL, NULL,
    g_cclosure_marshal_VOID__INT,
    G_TYPE_NONE, 1, G_TYPE_INT);

  gtk_image_tab_signals[TAB_HOVERED] = g_signal_new ("tab-hovered",
    G_OBJECT_CLASS_TYPE(obj_class), G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION,
    0, NULL, NULL,
    g_cclosure_marshal_VOID__INT,
    G_TYPE_NONE, 1, G_TYPE_INT);

  gtk_image_tab_signals[TAB_SELECT] = g_signal_new ("tab-select",
    G_OBJECT_CLASS_TYPE(obj_class), G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION,
    G_STRUCT_OFFSET(GtkImageTabClass, tab_select), NULL, NULL,
    g_cclosure_marshal_VOID__INT,
    G_TYPE_NONE, 1, G_TYPE_INT);


  g_object_class_install_property(obj_class, PROP_SELECTED, 
                                  g_param_spec_int("selected", "Selected", "The index of the selected tab",
                                                   -1, G_MAXINT, -1, G_PARAM_READWRITE));

  g_object_class_install_property (obj_class, PROP_HOVERED,
                                   g_param_spec_int ("hovered", "Hovered", "The index of the hovered widget",
                                                     -1, G_MAXINT, -1, G_PARAM_READABLE));

  g_object_class_install_property(obj_class, PROP_PIXBUF,
                                  g_param_spec_object("backgroung", "Pixbuf", 
                                  "A GdkPixbuf Background of ImageTab", GDK_TYPE_PIXBUF, G_PARAM_READWRITE));
  LF;
}

static void gtk_image_tab_init(GtkImageTab *self) {
  GtkImageTabPrivate *priv = GTK_IMAGE_TAB_GET_PRIVATE(self);

  EF;
  priv->backgroung= NULL;
  priv->child = NULL;
  priv->hovered = -1;
  priv->pressed = -1;
  LF;
}

gint gtk_image_tab_append(GtkImageTab *self, const char *iconset) {
  GtkImageTabPrivate *priv = GTK_IMAGE_TAB_GET_PRIVATE(self);
  GtkImageTabChild *new_child= NULL;
  GError *err = NULL;
  GString *path = NULL;
  gchar *subdirs[4] = {"up", "down", "over", "selected"};
  gint i = 0;

  path = g_string_new("icons/");

  new_child = (GtkImageTabChild *)g_malloc0(sizeof(GtkImageTabChild));
  for(; i < 4; i++, err=NULL) { 
    g_string_printf(path, "/usr/lib/dvm-config/rc/%s/%s", subdirs[i], iconset);
    //g_string_printf(path, "icons/%s/%s", subdirs[i], iconset);
    new_child->iconset[i] = gdk_pixbuf_new_from_file(path->str, &err);
    //DEBUG("append icon %s to GtkImageTab, iconset = %x\n", path->str, new_child->iconset[i]);
    g_string_erase(path, 0, -1);
  }

  priv->child = (GSList *)g_slist_append(priv->child, new_child);
  return g_slist_length(priv->child);
}

gint gtk_image_tab_get_selected (GtkImageTab *self) {
  return GTK_IMAGE_TAB_GET_PRIVATE(self)->selected;
}

void gtk_image_tab_set_selected (GtkImageTab *self, gint index) {
  GtkImageTabPrivate *priv = GTK_IMAGE_TAB_GET_PRIVATE(self);

  EF;
  //g_signal_emit(self, gtk_image_tab_signals[TAB_SELECT], 0, index);
  priv->selected = index;
  gtk_widget_queue_draw(GTK_WIDGET(self));
  g_signal_emit(self, gtk_image_tab_signals[TAB_SWITCHED], 0, index);
  LF;
}

gint gtk_image_tab_get_hovered (GtkImageTab *self) {
  return GTK_IMAGE_TAB_GET_PRIVATE(self)->hovered;
}

static void _gtk_image_tab_realize (GtkWidget *self) {
#define EVENT_MASK (GDK_EXPOSURE_MASK|GDK_BUTTON_PRESS_MASK|GDK_POINTER_MOTION_MASK|GDK_LEAVE_NOTIFY_MASK|GDK_BUTTON_RELEASE_MASK)
  GtkImageTab *imagetab = NULL;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkImageTabPrivate *priv;

  EF;
  priv = GTK_IMAGE_TAB_GET_PRIVATE(self);
  imagetab = GTK_IMAGE_TAB(self);
  GTK_WIDGET_SET_FLAGS(self, GTK_REALIZED);
  self->window = gtk_widget_get_parent_window(self);
  g_object_ref(self->window);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = self->allocation.x;
  attributes.y = self->allocation.y;
  attributes.width = self->allocation.width;
  attributes.height = self->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.event_mask = EVENT_MASK;
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  self->window = gdk_window_new (gtk_widget_get_parent_window (self), &attributes, attributes_mask);
  gdk_window_set_user_data (self->window, imagetab);

  self->style = gtk_style_attach (self->style, self->window);
  gtk_style_set_background(self->style, self->window, GTK_STATE_NORMAL);

  LF;
}

static void _gtk_image_tab_unrealize (GtkWidget *self) {
  GtkImageTabPrivate *priv;

  EF;
  priv = GTK_IMAGE_TAB_GET_PRIVATE (self);
  if(priv->child)
    g_slist_free(priv->child);

  if (GTK_WIDGET_CLASS (gtk_image_tab_parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (gtk_image_tab_parent_class)->unrealize) (self);
  LF;
}

static void _gtk_image_tab_set_hovered (GtkImageTab *self, gint index) {
  GtkImageTabPrivate *priv = GTK_IMAGE_TAB_GET_PRIVATE(self);

  if(priv->hovered != index) {
    EF;
    priv->hovered = index;
    gtk_widget_queue_draw(GTK_WIDGET(self));
    g_signal_emit(self, gtk_image_tab_signals[TAB_HOVERED], 0, index);
    LF;
  }
}

GtkWidget *gtk_image_tab_new(void) {
  return g_object_new(GTK_TYPE_IMAGE_TAB, NULL);
}

void gtk_image_tab_set_background_pixbuf(GtkImageTab *self, GdkPixbuf *backgroung) {
  GtkImageTabPrivate *priv;

  EF;
  priv = GTK_IMAGE_TAB_GET_PRIVATE(self);
  priv->backgroung= backgroung;
  LF;
}

static void _gtk_image_tab_size_request(GtkWidget *self, GtkRequisition *requisition) {
  GtkImageTabPrivate *priv;

  EF;
  priv = GTK_IMAGE_TAB_GET_PRIVATE(self);
  requisition->width  = 640;//gdk_pixbuf_get_width(priv->backgroung);
  requisition->height = 58;//gdk_pixbuf_get_height(priv->backgroung);
  LF;
}

static void _gtk_image_tab_size_allocate(GtkWidget *self, GtkAllocation *allocation) {
  self->allocation = *allocation;

  EF;
  if(GTK_WIDGET_REALIZED(self)) {
    gdk_window_move_resize(self->window, allocation->x, allocation->y, allocation->width, allocation->height);
  }
  LF;
}

static void _gtk_image_tab_draw(GtkImageTabPrivate *priv, GdkGC *gc, GdkWindow *window, GdkRectangle *r) {
  GSList *ptr = priv->child;
  GtkImageTabChild *child;
  gint x = 16, y = 0, i = 0, sta = 0;

  if(g_slist_length(priv->child)) {
    do {
      sta = ICON_STA_UP;
      if(priv->selected == i)
        sta = ICON_STA_SELECTED;
      else if(priv->hovered == i)
        sta = ICON_STA_OVER;
      else if(priv->pressed == i)
        sta = ICON_STA_DOWN;
      
      child = (GtkImageTabChild *)ptr->data;
      gdk_draw_pixbuf(window, gc, child->iconset[sta], 0, 0, x, y, -1, -1, GDK_RGB_DITHER_NORMAL, 0, 0);
      x += gdk_pixbuf_get_width(child->iconset[sta]);
      i++;
    } while(ptr = g_slist_next(ptr));
  }
}

static gboolean _gtk_image_tab_expose(GtkWidget *self, GdkEventExpose *event) {
  GtkImageTabPrivate *priv = GTK_IMAGE_TAB_GET_PRIVATE(self);
  GdkRectangle *r = NULL;

  gdk_window_begin_paint_rect(event->window, &event->area);

  if(GTK_WIDGET_DRAWABLE(self)) {
    r = (GdkRectangle *)&event->area;
    _gtk_image_tab_draw(priv, self->style->black_gc, event->window, r);
    //gdk_draw_pixbuf(event->window, self->style->black_gc, priv->backgroung, 0, 0, 0, 0, -1, -1, GDK_RGB_DITHER_NORMAL, 0, 0);
  }
  gdk_window_end_paint (event->window);

  return TRUE;
}

static gboolean _gtk_image_tab_motion_notify(GtkWidget *self, GdkEventMotion *event) {
  GtkImageTabPrivate *priv = GTK_IMAGE_TAB_GET_PRIVATE(self);
  gdouble child_size;
  int idx, n;

  n = g_slist_length(priv->child);
  if(n <= 0)
    return FALSE;

  child_size = self->allocation.width/n;
  if(child_size > 0)
    idx = event->x / child_size;

  if(idx >= 0 && n)
    _gtk_image_tab_set_hovered(GTK_IMAGE_TAB(self), idx);

  return FALSE;
}

static gboolean _gtk_image_tab_leave_notify(GtkWidget *self, GdkEventCrossing *event) {
  EF;
  if(event->mode == GDK_CROSSING_NORMAL) {
    GTK_IMAGE_TAB_GET_PRIVATE(self)->hovered = -1;
    gtk_widget_queue_draw(self);
  }
  LF;

  return FALSE;
}

static gboolean _gtk_image_tab_button_release(GtkWidget *self, GdkEventButton *event) {
  GtkImageTabPrivate *priv = GTK_IMAGE_TAB_GET_PRIVATE(self);

  EF;
  if(priv->hovered >= 0 && priv->hovered != priv->selected) {
    priv->pressed = -1;
    gtk_image_tab_set_selected(GTK_IMAGE_TAB(self), priv->hovered);
  }

  LF;
  return FALSE;
}

static gboolean _gtk_image_tab_button_press(GtkWidget *self, GdkEventButton *event) {
  GtkImageTabPrivate *priv = GTK_IMAGE_TAB_GET_PRIVATE(self);

  EF;
  if(priv->hovered >= 0 && priv->pressed != priv->selected) {
    priv->pressed = priv->hovered;
    DEBUG("pressed on %d button\n", priv->pressed);
    gtk_widget_queue_draw(GTK_WIDGET(self));
  }

  LF;
  return FALSE;
}
