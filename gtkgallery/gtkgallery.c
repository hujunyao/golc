#include "gtkgallery.h"

static void gtk_gallery_destroy(GtkObject *obj);

static void gtk_gallery_realize (GtkWidget *widget);
static void gtk_gallery_unrealize (GtkWidget *widget);

static void gtk_gallery_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void gtk_gallery_size_allocate (GtkWidget *widget, GtkAllocation *allocation);

static gboolean gtk_gallery_expose (GtkWidget *widget, GdkEventExpose *event);
static gboolean gtk_gallery_button_press (GtkWidget *widget, GdkEventButton *event);
static gboolean gtk_gallery_motion_notify (GtkWidget	   *widget, GdkEventMotion *event);
static gboolean gtk_gallery_leave_notify (GtkWidget *widget, GdkEventCrossing *event);

typedef struct {
  cairo_surface_t *pixbuf;
  gint w, h;
  gpointer data;
} photo_item_t;

struct _GtkGalleryPrivate{
  GList *pl;
  gint alpha;
  gint maxW, maxH;
  gint tid, effect_tid;
  photo_item_t *curr, *next;
  GdkWindow *window;
  GdkPixbuf *background;
  gboolean fill_in;
  double angle, dx, dy;
  gint ss_width, ss_height;
};

#define SLIDESHOW_MAXW 240
#define SLIDESHOW_MAXH 180

#define INNER_BORDER 4

#define OFFSET_X 30
#define OFFSET_Y 20

#define GALLERY_EFFECT_ALPHA_STEP 64

#define GTK_GALLERY_GET_PRIVATE(widget)  (GTK_GALLERY(widget)->priv)

#define DD(...) fprintf(stdout, __VA_ARGS__)

G_DEFINE_TYPE (GtkGallery, gtk_gallery, GTK_TYPE_WIDGET)

static void gtk_gallery_class_init (GtkGalleryClass *class) {
  GObjectClass     *gobject_class;
  GtkObjectClass   *object_class;
  GtkWidgetClass   *widget_class;

  DD("class init\n");
  gobject_class = (GObjectClass*)  class;
  object_class = (GtkObjectClass*)  class;
  widget_class = (GtkWidgetClass*) class;
  
  object_class->destroy = gtk_gallery_destroy;

  widget_class->realize = gtk_gallery_realize;
  widget_class->unrealize = gtk_gallery_unrealize;

  widget_class->expose_event = gtk_gallery_expose;
  widget_class->size_request = gtk_gallery_size_request;
  widget_class->size_allocate = gtk_gallery_size_allocate;
  widget_class->button_press_event = gtk_gallery_button_press;
  widget_class->motion_notify_event = gtk_gallery_motion_notify;
  widget_class->leave_notify_event = gtk_gallery_leave_notify;

  g_type_class_add_private (gobject_class, sizeof (GtkGalleryPrivate));
}

static gboolean photo_switch_effect_cb(gpointer data) {
  GtkGalleryPrivate *priv = (GtkGalleryPrivate *)data;
  gboolean ret = TRUE;

  priv->alpha += GALLERY_EFFECT_ALPHA_STEP;
  if(priv->alpha > 255) {
    priv->alpha = 255;
    DD("finish it, alpha = %d\n", priv->alpha);
    ret = FALSE; /**destroy timer*/
  }

  DD("update slideshow alpha = %d\n", priv->alpha);
  if(gdk_window_is_visible(priv->window)) {
    gdk_window_invalidate_rect(priv->window, NULL, FALSE);
  }
  return ret;
}

static gboolean gallery_photo_switch_cb (gpointer data) {
  GtkGalleryPrivate *priv = (GtkGalleryPrivate *)data;

  if(priv->effect_tid > 0) {
    DD("need remove effect timerid\n");
    g_source_remove(priv->effect_tid);
  }
  DD("start effect alpha = %d\n", priv->alpha);
  priv->effect_tid = g_timeout_add(150, photo_switch_effect_cb, priv);
  return TRUE;
}

static void init_priv(GtkGalleryPrivate *priv) {
  priv->maxW = priv->maxH = 0;
  priv->curr = priv->next = NULL;
  priv->fill_in = FALSE;
  priv->alpha = 0;
  priv->effect_tid = -1;
  priv->angle = priv->dx = priv->dy = 0.0;
  priv->ss_width = SLIDESHOW_MAXW, priv->ss_height = SLIDESHOW_MAXH;
  priv->tid = g_timeout_add_seconds(6, gallery_photo_switch_cb, priv);
}

static void gtk_gallery_init (GtkGallery *gallery) {
  GtkGalleryPrivate *priv = NULL;
  priv = G_TYPE_INSTANCE_GET_PRIVATE (gallery, GTK_TYPE_GALLERY, GtkGalleryPrivate);

  gallery->priv = priv;
  init_priv(gallery->priv);
}

static void gtk_gallery_realize (GtkWidget *widget) {
  GtkGalleryPrivate *priv = GTK_GALLERY_GET_PRIVATE (widget);
  GdkWindowAttr attributes;
  gint attributes_mask, width, height;
  GdkPixmap *pixmap = NULL;
  cairo_matrix_t mtx;
  double x, y;

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  //DD("widget realize \n");
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask =  (gtk_widget_get_events (widget) 
          | GDK_EXPOSURE_MASK |GDK_KEY_PRESS_MASK | GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  DD("realize, x=%d, y=%d, w=%d, h=%d\n", attributes.x, attributes.y, attributes.width, attributes.height);
  widget->window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);
  widget->style = gtk_style_attach (widget->style, widget->window);

  priv->window = widget->window;
  width = gdk_pixbuf_get_width(priv->background) - attributes.x, height = gdk_pixbuf_get_height(priv->background) - attributes.y;
  pixmap = gdk_pixmap_new(widget->window, width, height, -1);
  gdk_draw_pixbuf(pixmap, NULL, priv->background, attributes.x, attributes.y, 0, 0, width, height, GDK_RGB_DITHER_NONE, 0, 0);
  gdk_window_set_back_pixmap(widget->window, pixmap, FALSE);
  gdk_window_set_user_data (widget->window, widget);
}

static void gtk_gallery_destroy(GtkObject *obj) {
  GtkGalleryPrivate *priv = GTK_GALLERY_GET_PRIVATE (GTK_GALLERY(obj));

  DD("gallery destroy\n");
  if(priv->effect_tid) {
    g_source_remove(priv->effect_tid);
  }

  GTK_OBJECT_CLASS (gtk_gallery_parent_class)->destroy (obj);
}

static void gtk_gallery_unrealize (GtkWidget *widget) {
  GtkGalleryPrivate *priv = GTK_GALLERY_GET_PRIVATE (widget);

  gdk_window_destroy (priv->window);

  if (GTK_WIDGET_CLASS (gtk_gallery_parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (gtk_gallery_parent_class)->unrealize) (widget);
}

static gboolean gtk_gallery_button_press (GtkWidget *widget, GdkEventButton *event) {
  gboolean ret;

  DD("button press:\n");

  return ret;
}

static gboolean gtk_gallery_leave_notify (GtkWidget *widget, GdkEventCrossing *event) {
  GtkGalleryPrivate *priv = GTK_GALLERY_GET_PRIVATE (widget);
  gint w, h;
  GdkRectangle day_rect;

  /*if (event->window) {
    gdk_drawable_get_size(event->window, &w, &h);
    day_rect.x = 0, day_rect.y = 0, day_rect.width = w, day_rect.height = h;
    gdk_window_invalidate_rect (event->window, &day_rect, FALSE);
  }*/
  DD("___________________\n");
  priv->fill_in = FALSE;
  gtk_widget_queue_draw(widget);
  
  return FALSE;
}

static gboolean gtk_gallery_motion_notify (GtkWidget  *widget, GdkEventMotion *event) {
  GtkGalleryPrivate *priv = GTK_GALLERY_GET_PRIVATE (widget);
  gboolean ret = FALSE;
  cairo_t *cr;
  photo_item_t *pi = NULL;
  gboolean fill_in = FALSE;
  
  pi = priv->curr;
  if(pi) {
    cr = gdk_cairo_create (widget->window);
    cairo_save(cr);
    cairo_rotate(cr, priv->angle);
    cairo_scale(cr, 1.0, 1.0);
    cairo_translate(cr, priv->dx, priv->dy);
    cairo_rectangle(cr, 0.0, 0.0, pi->w, pi->h);
    cairo_restore(cr);

    if(cairo_in_fill(cr, event->x, event->y)) {
      DD("check ok\n");
      fill_in = TRUE;
    }
    DD("************** fill_in = %d\n", fill_in);

    if(priv->fill_in != fill_in) {
      GdkRectangle rect;
      DD("need updated, change fill_in status\n");
      priv->fill_in = fill_in;
      rect.x = rect.y = 0;
      gdk_drawable_get_size(event->window, &rect.width, &rect.height);
      gdk_window_invalidate_rect(event->window, &rect, TRUE);
    }
    cairo_destroy(cr);
  }

  return TRUE;
}

static gboolean gtk_gallery_expose (GtkWidget *widget, GdkEventExpose *event) {
  GtkGalleryPrivate *priv = GTK_GALLERY_GET_PRIVATE (widget);
  gboolean ret = FALSE;
  cairo_t *cr;
  photo_item_t *pi = NULL;
  gint w, h;

  if (GTK_WIDGET_DRAWABLE (widget)) {
    cairo_pattern_t *pat = NULL;

    gdk_drawable_get_size(widget->window, &w, &h);

    pi = priv->curr;

    cr = gdk_cairo_create (widget->window);
    pat = cairo_pattern_create_for_surface(pi->pixbuf);
    cairo_rotate(cr, priv->angle);
    cairo_scale(cr, 1.0, 1.0);
    cairo_translate(cr, priv->dx, priv->dy);
    cairo_set_source(cr, pat);

    cairo_save(cr);
    cairo_rectangle(cr, 0.0, 0.0, pi->w, pi->h);
    cairo_clip(cr);
    DD("expose event, alpha = %d\n", priv->alpha);
    cairo_paint_with_alpha(cr, 1.00 - priv->alpha/255.00);
    cairo_restore(cr);

    if(priv->effect_tid > 0) {
      pi = priv->next;
      pat = cairo_pattern_create_for_surface(pi->pixbuf);
      cairo_set_source(cr, pat);

      cairo_save(cr);
      cairo_rectangle(cr, 0.0, 0.0, pi->w, pi->h);
      cairo_clip(cr);
      //DD("expose event, alpha = %f, effect_tid = %d\n", priv->alpha/255.00, priv->effect_tid);
      cairo_paint_with_alpha(cr, priv->alpha/255.00);
      cairo_restore(cr);

      if(priv->alpha >= 255) {
        DD("change to next photo\n");
        GList *ptr = g_list_find(priv->pl, priv->next);
        priv->curr = pi;
        ptr = g_list_next(ptr);
        DD("ptr = %x\n", ptr);
        if(! ptr) {
          ptr = priv->pl;
          priv->next = ptr->data;
        } else 
          priv->next = ptr->data;
        priv->alpha = 0;
        priv->effect_tid = -1;
      }
    }

    cairo_save(cr);
    cairo_rectangle(cr, 0.0, 0.0, pi->w, pi->h);
    if(priv->fill_in)
      cairo_set_source_rgba(cr, 1.0, 0.8, 0.6, 1.00);
    else
      cairo_set_source_rgba(cr, 0.6, 0.4, 0.2, 1.00);
    cairo_set_line_width(cr, 3.0);
    cairo_stroke(cr);
    cairo_restore(cr);

    cairo_destroy(cr);
  }

  return TRUE;
}

static void gtk_gallery_size_request(GtkWidget *widget, GtkRequisition *requisition) {
  GtkGalleryPrivate *priv = GTK_GALLERY_GET_PRIVATE (widget);
  cairo_matrix_t mtx;
  double x0, y0, x1, y1, x2, y2, x3, y3;
  double maxY = 0.0, maxX = 0.0, minX = 0.0, minY = 0.0;


  cairo_matrix_init_rotate(&mtx, priv->angle);
  x1 = 0, y1 = priv->maxH;
  cairo_matrix_transform_point(&mtx, &x1, &y1);
  maxX = MAX(maxX, x1), maxY = MAX(maxY, y1);
  minX = MIN(minX, x1), minY = MIN(minY, y1);
  
  x2 = priv->maxW, y2 = 0;
  cairo_matrix_transform_point(&mtx, &x2, &y2);
  maxX = MAX(maxX, x2), maxY = MAX(maxY, y2);
  minX = MIN(minX, x2), minY = MIN(minY, y2);

  x3 = priv->maxW, y3 = priv->maxH;
  cairo_matrix_transform_point(&mtx, &x3, &y3);
  maxX = MAX(maxX, x3), maxY = MAX(maxY, y3);
  minX = MIN(minX, x3), minY = MIN(minY, y3);

  requisition->width = maxX - minX + INNER_BORDER;
  requisition->height = maxY - minY + INNER_BORDER;
  if(minX < 0)
    priv->dx = - minX;
  if(minY < 0)
    priv->dy = -minY;

  DD("dx = %f, dy = %f, minX = %f, minY = %f, maxX = %f, maxY = %f\n", priv->dx, priv->dy, minX, minY, maxX, maxY);
  DD("size request: maxW = %d, maxH = %d\n", requisition->width, requisition->height);
}

static void gtk_gallery_size_allocate (GtkWidget	  *widget, GtkAllocation *allocation) {
  DD("size allocate: (x = %d, y = %d, width = %d, height = %d)\n", 
     allocation->x, allocation->y, allocation->width, allocation->height);
  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget)) {
    DD("window resize needed\n");
    gdk_window_move_resize(widget->window, allocation->x, allocation->y,allocation->width, allocation->height);
  }
}

GtkWidget*
gtk_gallery_new (void)
{
  return g_object_new (GTK_TYPE_GALLERY, NULL);
}

void gtk_gallery_append_photo(GtkGallery *widget, gchar *path, gpointer data) {
  GtkGalleryPrivate *priv = GTK_GALLERY_GET_PRIVATE (widget);
  cairo_surface_t *surface = NULL;
  GdkPixbuf *pixbuf = NULL;
  gchar pngpath[1024] = {0};

  pixbuf = gdk_pixbuf_new_from_file_at_scale(path, priv->ss_width, priv->ss_height, FALSE, NULL);
  snprintf(pngpath, 1024, "/tmp/%x.png", pixbuf);
  gdk_pixbuf_save(pixbuf, pngpath, "png", NULL, NULL);

  surface = cairo_image_surface_create_from_png(pngpath);
  if(surface) {
    photo_item_t *pi = NULL;

    pi = g_new0(photo_item_t, 1);
    pi->data = data;
    pi->pixbuf = surface;
    pi->w = cairo_image_surface_get_width(surface);
    pi->h = cairo_image_surface_get_height(surface);
    DD("append photo: w = %d, h = %d\n", pi->w, pi->h);

    priv->maxW = MAX(priv->maxW, pi->w);
    priv->maxH = MAX(priv->maxH, pi->h);

    priv->pl = g_list_append(priv->pl, pi);

    if(priv->next == NULL) {
      if(priv->curr == NULL) {
        priv->curr = pi;
      } else {
        priv->next = pi;
      }
    }
  }
}

void gtk_gallery_set_background(GtkGallery *widget, GdkPixbuf *background) {
  GtkGalleryPrivate *priv = GTK_GALLERY_GET_PRIVATE (widget);

  priv->background = background;
}

void gtk_gallery_set_rotate(GtkGallery *widget, double angle) {
  GtkGalleryPrivate *priv = GTK_GALLERY_GET_PRIVATE (widget);

  priv->angle = angle;
}

void gtk_gallery_set_translate(GtkGallery *widget, double dx, double dy) {
  GtkGalleryPrivate *priv = GTK_GALLERY_GET_PRIVATE (widget);

  priv->dy = dy;
  priv->dx = dx;
}

void gtk_gallery_set_size(GtkGallery *widget, gint w, gint h) {
  GtkGalleryPrivate *priv = GTK_GALLERY_GET_PRIVATE (widget);

  priv->ss_width = w;
  priv->ss_height = h;
}
