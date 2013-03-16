
#include <stdlib.h>
#include <gtk/gtk.h>

int main( int   argc, char *argv[] ) {
    gchar msg[1024] = {0};
    GtkWidget *dialog;
    gtk_init (&argc, &argv);

    snprintf(msg, 1024, "Skype Number\t\t\t\t\t\ncallto://<b>%s</b>", argv[1]+9);
    dialog = gtk_message_dialog_new_with_markup(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,GTK_BUTTONS_CLOSE, msg);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ALWAYS);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    return 0;
}
