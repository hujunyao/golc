#ifndef __GTKGALLERY_H__
#define __GTKGALLERY_H__
#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_GALLERY (gtk_gallery_get_type())
#define GTK_GALLERY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_GALLERY, GtkGallery))
#define GTK_IS_GALLERY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_GALLERY))
#define GTK_GALLERY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_GALLERY, GtkGalleryClass))
#define GTK_IS_GALLERY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_GALLERY))
#define GTK_GALLERY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_GALLERY, GtkGalleryClass))

typedef struct _GtkGallery GtkGallery;
typedef struct _GtkGalleryClass GtkGalleryClass;

typedef struct _GtkGalleryPrivate GtkGalleryPrivate;

struct _GtkGallery {
  GtkWidget widget;

  GtkGalleryPrivate *priv;
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

struct _GtkGalleryClass {
  GtkWidgetClass parent_class;
};

GType gtk_gallery_get_type(void);

GtkWidget *gtk_gallery_new(void);
void gtk_gallery_append_photo(GtkGallery *widget, gchar *photo, gpointer data);
void gtk_gallery_set_background(GtkGallery *widget, GdkPixbuf *background);
void gtk_gallery_set_rotate(GtkGallery *widget, double angle);
void gtk_gallery_set_translate(GtkGallery *widget, double dx, double dy);
void gtk_gallery_set_size(GtkGallery *widget, gint w, gint h);

G_END_DECLS

#endif
