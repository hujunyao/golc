#ifndef __GTK_IMAGE_TAB_H__
#define __GTK_IMAGE_TAB_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _GtkImageTab GtkImageTab;
typedef struct _GtkImageTabClass GtkImageTabClass;

struct _GtkImageTab {
  GtkWidget parent;
};

struct _GtkImageTabClass {
  GtkWidgetClass parent_class;

  gboolean (*tab_select) (GtkImageTab *self, gint index);
};

#define GTK_TYPE_IMAGE_TAB          (gtk_image_tab_get_type())
#define GTK_IMAGE_TAB(obj)          (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_IMAGE_TAB, GtkImageTab))
#define GTK_IMAGE_TAB_CLASS(obj)    (G_TYPE_CHECK_CLASS_CAST((obj), GTK_IMAGE_TAB, GtkImageTabClass))
#define GTK_IS_IMAGE_TAB(obj)       (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_IMAGE_TAB))
#define GTK_IS_IMAGE_TAB_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), GTK_TYPE_IMAGE_TAB))
#define GTK_IMAGE_TAB_GET_CLASS     (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_IMAGE_TAB, GtkImageTabClass))

GtkType gtk_image_tab_get_type(void);
GtkWidget *gtk_image_tab_new(void);
void gtk_image_tab_set_background_pixbuf(GtkImageTab *widget, GdkPixbuf *backgroung);
gint gtk_image_tab_append(GtkImageTab *widget, const char *iconset);
void gtk_image_tab_set_selected (GtkImageTab *self, gint index);
gint gtk_image_tab_get_selected (GtkImageTab *self);

G_END_DECLS

#endif
