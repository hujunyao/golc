#include <gtk/gtk.h>
#include "gtkccal.h"

int main( int   argc, char *argv[] ) {
    GtkWidget *window;
    GtkWidget *ccal;

    gtk_init (&argc, &argv);

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), "CCal Test");

    g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (gtk_main_quit), NULL);
    g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (gtk_main_quit), NULL);

    gtk_container_set_border_width (GTK_CONTAINER (window), 10);

    ccal = gtk_ccal_new ();
    gtk_widget_set_name(GTK_WIDGET(ccal), "my-test-cal");
    gtk_container_add (GTK_CONTAINER (window), ccal);
    gtk_widget_show (ccal);
    gtk_widget_set_size_request(ccal, 230, 260);

    gtk_widget_show (window);

    gtk_main ();

    return 0;
}
