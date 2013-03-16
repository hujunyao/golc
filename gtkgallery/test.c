#include <gtk/gtk.h>
#include "gtkgallery.h"

static gboolean main_window_draw_background(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
  gint width = widget->allocation.width, height = widget->allocation.height;

  printf("draw background\n");
  gdk_draw_pixbuf(widget->window, NULL, data, 0, 0, 0, 0, width, height, GDK_RGB_DITHER_NORMAL, 0, 0);

  return FALSE;
}

int main( int   argc, char *argv[] ) {
    GtkWidget *window;
    GtkWidget *gallery;
    GdkPixbuf *background = NULL;

    gtk_init (&argc, &argv);

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), "Test");

    background = gdk_pixbuf_new_from_file_at_scale("logo.jpg", 640, 480, TRUE, NULL);
    //g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (gtk_main_quit), NULL);
    g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (gtk_main_quit), NULL);
    g_signal_connect(G_OBJECT(window), "expose-event", G_CALLBACK(main_window_draw_background), background);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    gtk_widget_set_app_paintable(window, TRUE);

    gallery = gtk_gallery_new ();
    gtk_gallery_append_photo(GTK_GALLERY(gallery), "/home/user/photos/20081018(024).jpg", NULL);
    gtk_gallery_append_photo(GTK_GALLERY(gallery), "/home/user/photos/20080720(009).jpg", NULL);
    gtk_gallery_append_photo(GTK_GALLERY(gallery), "/home/user/photos/20080720(007).jpg", NULL);
    gtk_gallery_append_photo(GTK_GALLERY(gallery), "/home/user/photos/20080611(008).jpg", NULL);
    gtk_gallery_append_photo(GTK_GALLERY(gallery), "/home/user/photos/20081018(019).jpg", NULL);
    gtk_gallery_set_background(GTK_GALLERY(gallery), background);
    //gtk_widget_set_name(GTK_WIDGET(gallery), "my-test-cal");
    gtk_gallery_set_rotate(GTK_GALLERY(gallery), 0.1);
    gtk_container_add (GTK_CONTAINER (window), gallery);
    //gtk_widget_show (gallery);
    //gtk_widget_set_size_request(gallery, 320, 280);

    //gtk_widget_realize(window);
    //gdk_window_set_back_pixmap (window->window, NULL, FALSE);
    gtk_widget_show_all (window);

    gtk_main ();

    g_object_unref(background);

    return 0;
}
