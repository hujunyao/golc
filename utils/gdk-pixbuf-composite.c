#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

int main(int argc, char *argv[]) {
  GdkPixbuf *src = NULL, *dest = NULL;
  GError *err = NULL;

  gtk_init(&argc, &argv);

  dest = gdk_pixbuf_new_from_file("myicon.png", NULL);
  src = gdk_pixbuf_new_from_file("mark.png", NULL); 
  printf("dest = %x, src = %x\n,", dest, src); 

  gdk_pixbuf_composite(src, dest, 0, 20, 34, 17, 4, 20, 1.0, 1.0, GDK_INTERP_NEAREST, 255);
  gdk_pixbuf_save(dest, "test.jpg", "jpeg", &err, "quality", "100", NULL);

  return 0;
}

