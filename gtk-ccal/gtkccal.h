#ifndef __GTKCCAL_H__
#define __GTKCCAL_H__
#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_CCAL (gtk_ccal_get_type())
#define GTK_CCAL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CCAL, GtkCCal))
#define GTK_IS_CCAL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CCAL))
#define GTK_CCAL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CCAL, GtkCCalClass))
#define GTK_IS_CCAL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CCAL))
#define GTK_CCAL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CCAL, GtkCCalClass))

typedef struct _GtkCCal GtkCCal;
typedef struct _GtkCCalClass GtkCCalClass;

typedef struct _GtkCCalPrivate GtkCCalPrivate;

struct _GtkCCal {
  GtkCalendar widget;

  GtkCCalPrivate *priv;

  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

struct _GtkCCalClass {
  GtkCalendarClass parent_class;
};

GType gtk_ccal_get_type(void);

GtkWidget *gtk_ccal_new(void);

G_END_DECLS

#endif
