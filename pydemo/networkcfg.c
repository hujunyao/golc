#include <string.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <gtk/gtk.h>

static gint g_sockfd = 0;

#define DEBUG(...) fprintf(stdout, "[networkcfg.so] "__VA_ARGS__)

static void plug_added(GtkWidget *widget, void *data) {
  gtk_widget_show(GTK_WIDGET(data));
}

static gboolean pycli_read_cb(GIOChannel *channel, GIOCondition cond, gpointer data) {
  GError *err = NULL;
  gchar buf[1024] = {0};
  gint nread = 0, xid = 0;

  if(cond & (G_IO_ERR|G_IO_HUP|G_IO_NVAL)) {
    DEBUG("pyclient socket fd is invalid.\n");
    return FALSE;
  }else if(cond & G_IO_IN) {
    read(g_io_channel_unix_get_fd(channel), buf, 1024);
    DEBUG("new data incoming, buf = %s.\n", buf);
    if(strncmp(buf, "XID:", 4) == 0) {
      xid = strtol (buf+4, NULL, 0);
      if (xid == 0) {
        DEBUG("Invalid window id '%s'.\n", buf);
      } else {
        DEBUG("Window id '%x'.\n", xid);
        gtk_socket_add_id (GTK_SOCKET (data), xid);
      }
    }
  }
}

static gboolean py_server_cb (GIOChannel *channel, GIOCondition cond, gpointer data) {
  GError *err = NULL;

  if(cond & (G_IO_ERR|G_IO_HUP|G_IO_NVAL)) {
    DEBUG("server socket fd is invalid.\n");
    return FALSE;
  }
  if(cond & G_IO_IN) {
    gint clientfd = 0;
    struct sockaddr_in c_addr;
    socklen_t len;

    clientfd = accept(g_io_channel_unix_get_fd(channel), (struct sockaddr *)&c_addr, &len);
    DEBUG("pyclient accept now, clientfd = %x.\n", clientfd);
    channel = g_io_channel_unix_new(clientfd);
    g_io_add_watch(channel, G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL, pycli_read_cb, data);
    g_io_channel_unref(channel);
    g_object_set_data((GObject *)data, "PyClient", (void *)clientfd);
  }

  return TRUE;
}


static void socket_destroyed(GtkWidget *widget, void *data) {
  gint clientfd = (gint)g_object_get_data(G_OBJECT(data), "PyClient");
  DEBUG("GtkSocket sockfd = %x, clientfd = %x\n", g_sockfd, clientfd);
  if(g_sockfd) {
    close(g_sockfd);
    g_sockfd = 0;
  }
  if(clientfd) {
    close(clientfd);
    clientfd = 0;
  }
}

static GtkWidget *_create_networkcfg_ui(const char *exec, void *data) {
  GtkWidget *widget = NULL;
  char *argv[3] = {NULL, NULL, NULL};
  char buf[24] = {0};
  GError *err = NULL;
  GIOChannel *channel;


  widget = gtk_socket_new();

  argv[0] = (char *)exec;
  if(!g_spawn_async("./", argv, NULL, 0, NULL, NULL, NULL, &err)) {
    DEBUG("exec networkcfg process failure\n"); 
    return NULL;
  }
  g_signal_connect (widget, "plug_added", G_CALLBACK(plug_added), widget);
  g_signal_connect (widget, "destroy", G_CALLBACK(socket_destroyed), widget);

  DEBUG("exec %s success, python server fd = %d\n", exec, g_sockfd);
  channel = g_io_channel_unix_new(g_sockfd);
  g_io_add_watch(channel, G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL, py_server_cb, widget);
  g_io_channel_unref(channel);

  return widget;
}

static void _button_clicked(GtkButton *btn, void *data) {
  char *cmd = (char *)data;

  if(g_sockfd) {
    write(g_sockfd, cmd, strlen(cmd));
    DEBUG("send cmd (%s) to pycli success\n", cmd);
  }
}

GtkWidget *cfg_create_ui(GtkWidget *dialog, GtkWidget *ok, GtkWidget *cancel) {
  GtkWidget *socket_widget = NULL;
  gint ret, val = 1;
  struct sockaddr_in s_addr;

  g_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(g_sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  memset(&s_addr, 0, sizeof(struct sockaddr_in));
  s_addr.sin_family = AF_INET;
  s_addr.sin_port = htons(7788);
  s_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  bind(g_sockfd, (struct sockaddr *)&s_addr, sizeof(s_addr));
  ret  = listen(g_sockfd, 1);
  DEBUG("return of listen(sockfd, 1) = %x\n", ret);

  g_signal_connect(ok, "clicked", G_CALLBACK(_button_clicked), "ok.clicked");
  g_signal_connect(cancel, "clicked", G_CALLBACK(_button_clicked), "cancel.clicked");

  socket_widget = _create_networkcfg_ui("/usr/lib/dvm-config/networkcfg", NULL);

  return socket_widget;
}
