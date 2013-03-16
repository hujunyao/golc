#include "gtkccal.h"

enum {
  MONTH_PREV,
  MONTH_CURRENT,
  MONTH_NEXT
};

static void gtk_ccal_realize (GtkWidget *widget);
static void gtk_ccal_unrealize (GtkWidget *widget);

static void gtk_ccal_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void gtk_ccal_size_allocate (GtkWidget *widget, GtkAllocation *allocation);

static gboolean gtk_ccal_expose (GtkWidget *widget, GdkEventExpose *event);
static gboolean gtk_ccal_button_press (GtkWidget *widget, GdkEventButton *event);
static gboolean gtk_ccal_motion_notify (GtkWidget	   *widget, GdkEventMotion *event);

typedef struct {
  double x, y;
  double w, h;
} myrect_t;

struct _GtkCCalPrivate{
  guint main_h;
  guint main_w;

  guint	     max_month_width;
  guint	     max_year_width;
  
  guint day_width, day_height;

  gint week_start;
};

typedef struct _GtkCalendarPrivate
{
  GdkWindow *header_win;
  GdkWindow *day_name_win;
  GdkWindow *main_win;
  GdkWindow *week_win;
  GdkWindow *arrow_win[4];

  guint header_h;
  guint day_name_h;
  guint main_h;

  guint      arrow_state[4];
  guint      arrow_width;
  guint      max_month_width;
  guint      max_year_width;

  guint day_width;
  guint week_width;

  guint min_day_width;
  guint max_day_char_width;
  guint max_day_char_ascent;
  guint max_day_char_descent;
  guint max_label_char_ascent;
  guint max_label_char_descent;
  guint max_week_char_width;
} GtkMyCalendarPrivate;

/* Spacing around day/week headers and main area, inside those windows */
#define CALENDAR_MARGIN    0
/* Spacing around day/week headers and main area, outside those windows */
#define INNER_BORDER     4
/* Separation between day headers and main area */
#define CALENDAR_YSEP    4
/* Separation between week headers and main area */
#define CALENDAR_XSEP    4

#define CCAL_MAIN_TOP    57
#define CCAL_MAIN_XPOS   9
#define CCAL_MAIN_WIDTH  220
#define CCAL_MAIN_HEIGHT 183

#define DAY_W 30
#define DAY_H 30

#define GTK_CCAL_GET_PRIVATE(widget)  (GTK_CCAL(widget)->priv)
#define BACKGROUND_COLOR(widget)   (& (widget)->style->base[GTK_WIDGET_STATE (widget)])

#define DD(...) fprintf(stdout, __VA_ARGS__)

G_DEFINE_TYPE (GtkCCal, gtk_ccal, GTK_TYPE_CALENDAR)

static void gtk_ccal_class_init (GtkCCalClass *class) {
  GObjectClass   *gobject_class;
  GtkObjectClass   *object_class;
  GtkWidgetClass *widget_class;

  gobject_class = (GObjectClass*)  class;
  object_class = (GtkObjectClass*)  class;
  widget_class = (GtkWidgetClass*) class;
  
  widget_class->realize = gtk_ccal_realize;

  widget_class->expose_event = gtk_ccal_expose;
  widget_class->size_allocate = gtk_ccal_size_allocate;
  widget_class->button_press_event = gtk_ccal_button_press;
  widget_class->motion_notify_event = gtk_ccal_motion_notify;

  g_type_class_add_private (gobject_class, sizeof (GtkCCalPrivate));
}

static void gtk_ccal_init (GtkCCal *cal) {
  GtkCCalPrivate *priv;

  priv = cal->priv = G_TYPE_INSTANCE_GET_PRIVATE (cal, GTK_TYPE_CCAL, GtkCCalPrivate);
}

static void gtk_ccal_realize (GtkWidget *widget) {
  GtkCCal *cal = GTK_CCAL(widget);
  GtkCCalPrivate *priv = GTK_CCAL_GET_PRIVATE (widget);
  GtkMyCalendarPrivate *mypriv = G_TYPE_INSTANCE_GET_PRIVATE(widget, GTK_TYPE_CALENDAR, GtkMyCalendarPrivate);
  gint x, y, w, h, header_w;


  (* GTK_WIDGET_CLASS (gtk_ccal_parent_class)->realize) (widget);

  gdk_window_get_position(mypriv->header_win, &x, &y);
  gdk_drawable_get_size(mypriv->header_win, &w, &h);
  gdk_window_move_resize(mypriv->header_win, x+4, y, 220, h + 4 + 10);
  header_w = 220 - 8;

  /**hide year change button*/
  gdk_window_hide(mypriv->arrow_win[0]);
  gdk_window_hide(mypriv->arrow_win[1]);

  gdk_window_get_position(mypriv->arrow_win[3], &x, &y);
  gdk_drawable_get_size(mypriv->arrow_win[3], &w, &h);
  gdk_window_move_resize(mypriv->arrow_win[3], header_w - 12*0.85, y + 8, 12*0.85, 12);

  gdk_window_get_position(mypriv->arrow_win[2], &x, &y);
  gdk_drawable_get_size(mypriv->arrow_win[2], &w, &h);
  gdk_window_move_resize(mypriv->arrow_win[2], x + 8 , y + 8, 12*0.85, 12);
  mypriv->max_year_width = 130;
  mypriv->max_month_width = 130;

  gdk_window_get_position(mypriv->day_name_win, &x, &y);
  gdk_drawable_get_size(mypriv->day_name_win, &w, &h);
  gdk_window_move_resize(mypriv->day_name_win, x, y+6, 220, h);

  gdk_window_get_position(mypriv->main_win, &x, &y);
  gdk_drawable_get_size(mypriv->main_win, &w, &h);
  gdk_window_move_resize(mypriv->main_win, x, y, 220, 189);
  mypriv->main_h = 189;
  mypriv->header_h += 14;
  mypriv->day_width = 30;
}

static void get_day_rectangle (GtkMyCalendarPrivate  *mypriv, gint col, gint row, myrect_t *rect) {
  gint w, h;

  gdk_drawable_get_size(mypriv->main_win, &w, &h);
  DD("get_rectangle: w = %d, h = %d\n", w, h);
  rect->x = (w*col)/7.0 + 1.0;
  rect->y = (row*h)/6.0 + 1.0;
  rect->h = h/6.0 - 2.0;
  rect->w = w/7.0 - 2.0;
}

static void get_rectangle_by_day_num (GtkCalendar *calendar, gint day, myrect_t *rect) {
  gint r, c, row, col;
  GtkMyCalendarPrivate *mypriv = G_TYPE_INSTANCE_GET_PRIVATE(GTK_WIDGET(calendar), GTK_TYPE_CALENDAR, GtkMyCalendarPrivate);

  row = -1;
  col = -1;
  for (r = 0; r < 6; r++)
    for (c = 0; c < 7; c++)
      if (calendar->day_month[r][c] == MONTH_CURRENT && calendar->day[r][c] == day) {
    row = r;
    col = c;
  }

  g_return_if_fail (row != -1);
  g_return_if_fail (col != -1);

  get_day_rectangle(mypriv, col, row, rect);
}

static void gtk_ccal_unrealize (GtkWidget *widget) {
  GtkCCalPrivate *priv = GTK_CCAL_GET_PRIVATE (widget);

  if (GTK_WIDGET_CLASS (gtk_ccal_parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (gtk_ccal_parent_class)->unrealize) (widget);
}

static gboolean gtk_ccal_button_press (GtkWidget *widget, GdkEventButton *event) {
  GtkCCal *cal = GTK_CCAL(widget);
  GtkCCalPrivate *priv = GTK_CCAL_GET_PRIVATE (widget);
  GtkCalendar *mycal = GTK_CALENDAR(widget);
  GtkMyCalendarPrivate *mypriv = G_TYPE_INSTANCE_GET_PRIVATE(widget, GTK_TYPE_CALENDAR, GtkMyCalendarPrivate);
  gboolean ret = FALSE;
  GdkRectangle day_rect;

  ret = (* GTK_WIDGET_CLASS (gtk_ccal_parent_class)->button_press_event) (widget, event);

  if (event->window == mypriv->main_win) {
    gint w, h;
    gdk_drawable_get_size(event->window, &w, &h);
    day_rect.x = 0, day_rect.y = 0, day_rect.width = w, day_rect.height = h;
    gdk_window_invalidate_rect (mypriv->main_win, &day_rect, FALSE);
  }

  return ret;
}

static gboolean gtk_ccal_motion_notify (GtkWidget	   *widget, GdkEventMotion *event) {
  GtkCCal *cal = GTK_CCAL(widget);
  GtkCCalPrivate *priv = GTK_CCAL_GET_PRIVATE (widget);
  GtkCalendar *mycal = GTK_CALENDAR(widget);
  GtkMyCalendarPrivate *mypriv = G_TYPE_INSTANCE_GET_PRIVATE(widget, GTK_TYPE_CALENDAR, GtkMyCalendarPrivate);
  gboolean ret = FALSE;
  cairo_t *cr = NULL;
  guint width = mypriv->day_width;
  GdkRectangle day_rect;
  
  ret = (* GTK_WIDGET_CLASS (gtk_ccal_parent_class)->motion_notify_event) (widget, event);

  if (event->window == mypriv->main_win) {
    gint w, h;
    gdk_drawable_get_size(event->window, &w, &h);
    day_rect.x = 0, day_rect.y = 0, day_rect.width = w, day_rect.height = h;
    gdk_window_invalidate_rect (mypriv->main_win, &day_rect, FALSE);
  }

  return ret;
}

static gboolean gtk_ccal_expose (GtkWidget *widget, GdkEventExpose *event) {
  GtkCCal *cal = GTK_CCAL(widget);
  GtkCalendar *mycal = GTK_CALENDAR(widget);
  GtkCCalPrivate *priv = GTK_CCAL_GET_PRIVATE (widget);
  GtkMyCalendarPrivate *mypriv = G_TYPE_INSTANCE_GET_PRIVATE(widget, GTK_TYPE_CALENDAR, GtkMyCalendarPrivate);
  gboolean ret = FALSE;
  gint focus_col;

  if (GTK_WIDGET_DRAWABLE (widget)) {
    cairo_t *cr;
    myrect_t day_rect;
    gint w, h;
    gint i, j, x = 0, y = 0;

    if(event->window == mypriv->header_win) {
      PangoContext* pc = NULL;
      PangoFontDescription* pfd = NULL;

      pc = gtk_widget_get_pango_context(widget);
      pfd = pango_context_get_font_description(pc);
      if(pc && pfd) {
        pango_font_description_set_weight(pfd, PANGO_WEIGHT_LIGHT);
      }
    } else if(event->window == mypriv->arrow_win[3]) {
      cr = gdk_cairo_create (event->window);

      gdk_drawable_get_size(event->window, &w, &h);
      cairo_move_to(cr, 0, 0);
      cairo_line_to(cr, 0, h);
      cairo_line_to(cr, w, h/2.0);
      cairo_line_to(cr, 0, 0);
      cairo_set_source_rgb(cr, 0.76, 0.64, 0.27);
      cairo_fill(cr);

      cairo_destroy (cr);
      return TRUE;
    } else if(event->window == mypriv->arrow_win[2]) {
      cr = gdk_cairo_create (event->window);

      gdk_drawable_get_size(event->window, &w, &h);
      cairo_move_to(cr, w, 0);
      cairo_line_to(cr, w, h);
      cairo_line_to(cr, 0, h/2.0);
      cairo_line_to(cr, w, 0);
      cairo_set_source_rgb(cr, 0.76, 0.64, 0.27);
      cairo_fill(cr);

      cairo_destroy (cr);
      return TRUE;
    } else if(event->window == mypriv->day_name_win) {
      PangoContext* pc = NULL;
      PangoFontDescription* pfd = NULL;

      pc = gtk_widget_get_pango_context(widget);
      pfd = pango_context_get_font_description(pc);
      if(pc && pfd) {
        pango_font_description_set_weight(pfd, PANGO_WEIGHT_LIGHT);
        pango_font_description_set_size(pfd, pango_font_description_get_size(pfd) - PANGO_SCALE*1);
      }
    }

    focus_col = mycal->focus_col;
    mycal->focus_col = -1;
    ret = (* GTK_WIDGET_CLASS (gtk_ccal_parent_class)->expose_event) (widget, event);
    mycal->focus_col = focus_col;
  
    if(event->window == mypriv->day_name_win) {
      PangoContext* pc = NULL;
      PangoFontDescription* pfd = NULL;

      cr = gdk_cairo_create (event->window);
      gdk_drawable_get_size(event->window, &w, &h);

      cairo_move_to(cr, 0, h);
      cairo_line_to(cr, 0, 0);
      cairo_move_to(cr, w, h);
      cairo_line_to(cr, w, 0);

      cairo_set_line_width (cr, 4.0);
      cairo_set_source_rgb(cr, 0.56, 0.74, 0.87);
      cairo_stroke(cr);

      cairo_destroy (cr);

      pc = gtk_widget_get_pango_context(widget);
      pfd = pango_context_get_font_description(pc);
      if(pc && pfd) {
        pango_font_description_set_size(pfd, pango_font_description_get_size(pfd) + PANGO_SCALE*1);
        pango_font_description_set_weight(pfd, PANGO_WEIGHT_NORMAL);
      }
    } else if(event->window == mypriv->header_win) {
      PangoContext* pc = NULL;
      PangoFontDescription* pfd = NULL;

      cr = gdk_cairo_create (event->window);
      gdk_drawable_get_size(event->window, &w, &h);

      cairo_move_to(cr, 0, h);
      cairo_line_to(cr, 0, 0);
      cairo_line_to(cr, w, 0);
      cairo_line_to(cr, w, h);
      cairo_set_line_width (cr, 4.0);
      cairo_set_source_rgb(cr, 0.56, 0.74, 0.87);
      cairo_stroke(cr);

      pc = gtk_widget_get_pango_context(widget);
      pfd = pango_context_get_font_description(pc);
      if(pc && pfd) {
        pango_font_description_set_weight(pfd, PANGO_WEIGHT_NORMAL);
      }
      cairo_destroy (cr);
    } else if(event->window == mypriv->main_win) {
      double dx, dy, myX, myY;
      cr = gdk_cairo_create (event->window);

      gdk_drawable_get_size(event->window, &w, &h);

      for(dy =h/6.0, i=1, myY+=dy; i<6; i++, myY+=dy) {
        cairo_move_to(cr, 0, myY);
        cairo_line_to(cr, w, myY);
        cairo_set_line_width (cr, 1.0);
        cairo_set_source_rgb(cr, 0.56, 0.74, 0.87);
      }
      for(dx = w/7.0, i=1, myX+=dx; i<7; i++, myX+=dx) {
        cairo_move_to(cr, myX, 0);
        cairo_line_to(cr, myX, h);
        cairo_set_line_width (cr, 1.0);
        cairo_set_source_rgb(cr, 0.56, 0.74, 0.87);
      }
      cairo_stroke(cr);

      cairo_move_to(cr, 0, 0);
      cairo_line_to(cr, w, 0);
      cairo_set_line_width (cr, 4.0);
      cairo_set_source_rgb(cr, 0.56, 0.74, 0.87);
      cairo_stroke(cr);

      cairo_rectangle(cr, 0, 0, w, h);
      cairo_set_line_width (cr, 4.0);
      cairo_set_source_rgb(cr, 0.56, 0.74, 0.87);
      cairo_stroke(cr);

      if(mycal->selected_day >= 0) {
        get_rectangle_by_day_num(mycal, mycal->selected_day, &day_rect);
        cairo_rectangle(cr, day_rect.x, day_rect.y, day_rect.w, day_rect.h);
        cairo_set_source_rgba(cr, 0.5, 0.5, 0.1, 0.6);
        cairo_stroke_preserve(cr);
        cairo_fill(cr);
      } else if(mycal->focus_col >= 0 && mycal->focus_row >=0) {
        get_day_rectangle(mypriv, mycal->focus_col, mycal->focus_row, &day_rect);
        cairo_rectangle(cr, day_rect.x, day_rect.y, day_rect.w, day_rect.h);
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.1);
        cairo_set_line_width (cr, 2.0);
        cairo_stroke(cr);
      }

      if(mycal->highlight_row >= 0 && mycal->highlight_col >= 0) {
        get_day_rectangle(mypriv, mycal->highlight_col, mycal->highlight_row, &day_rect);
        cairo_rectangle(cr, day_rect.x, day_rect.y, day_rect.w, day_rect.h);
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.1);
        cairo_set_line_width (cr, 2.0);
        cairo_stroke(cr);
      }
      cairo_destroy (cr);
    }
  }

  return ret;
}

static void gtk_ccal_size_allocate (GtkWidget	  *widget, GtkAllocation *allocation) {
  GtkCCal *cal = GTK_CCAL(widget);
  GtkMyCalendarPrivate *mypriv = G_TYPE_INSTANCE_GET_PRIVATE(widget, GTK_TYPE_CALENDAR, GtkMyCalendarPrivate);
  gint xthickness = widget->style->xthickness;
  gint ythickness = widget->style->xthickness;
  guint x, y, w, h, header_w;
  
  widget->allocation = *allocation;
    
  if (GTK_WIDGET_REALIZED (widget)) {
      gdk_window_hide(mypriv->arrow_win[3]);
      gdk_window_hide(mypriv->arrow_win[2]);
      (* GTK_WIDGET_CLASS (gtk_ccal_parent_class)->size_allocate) (widget, allocation);

      gdk_drawable_get_size(mypriv->main_win, &w, &h);
      DD("allocation. width = %d, height = %d\n", w, h);

      gdk_window_get_position(mypriv->header_win, &x, &y);
      gdk_drawable_get_size(mypriv->header_win, &w, &h);
      gdk_window_move_resize(mypriv->header_win, x+4, y, w-8, h + 4 + 10);
      header_w = w - 8;

      gdk_window_get_position(mypriv->arrow_win[2], &x, &y);
      gdk_drawable_get_size(mypriv->arrow_win[2], &w, &h);
      gdk_window_move_resize(mypriv->arrow_win[2], x + 8 , y+8, 12*0.85, 12);

      gdk_window_get_position(mypriv->arrow_win[3], &x, &y);
      gdk_drawable_get_size(mypriv->arrow_win[3], &w, &h);
      gdk_window_move_resize(mypriv->arrow_win[3], header_w -8 - 12, y+8, 12*0.85, 12);

      gdk_window_show(mypriv->arrow_win[3]);
      gdk_window_show(mypriv->arrow_win[2]);
    } else {
      (* GTK_WIDGET_CLASS (gtk_ccal_parent_class)->size_allocate) (widget, allocation);
    }
}

GtkWidget*
gtk_ccal_new (void)
{
  return g_object_new (GTK_TYPE_CCAL, NULL);
}
