/**compile command: gcc -o msgcli msgcli.c -I /usr/include `pkg-config glib-2.0 --cflags --libs`*/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <glib.h>

#define MSG_MAX_SIZE 102400
#define DEBUG(...) fprintf(stdout, "[msgcli] "__VA_ARGS__)

int tcpsock = -1;

gboolean _input_callback(GIOChannel *source) {
  gchar *buf = NULL;
  GIOStatus sta;
  gsize len , writen;

  sta = g_io_channel_read_line(source, &buf, &len, NULL, NULL);
  if(buf) {
    if(write(tcpsock, buf, len-1) <= 0) {
      DEBUG("Send to msgproxy failed!\n");
    }
  }

  return TRUE;
}

gboolean _data_watch (GIOChannel *source, GIOCondition cond, gpointer data) {
  if(cond & (G_IO_ERR|G_IO_HUP|G_IO_NVAL)) {
    DEBUG("TCP socket fd is INVALID.\n");
    return FALSE;
  }

  if(cond & G_IO_IN) {
    int sockfd = g_io_channel_unix_get_fd(source);
    gchar buf[MSG_MAX_SIZE] = {0};
    GIOStatus sta;
    gsize len = MSG_MAX_SIZE, bytes_read;

    bytes_read = read(sockfd, buf, MSG_MAX_SIZE);
    if(bytes_read <= 0) {
      DEBUG("Disconnected by msgproxy.\n");
      close(sockfd);
      return FALSE;
    } else {
      //DEBUG("buf = %s, bytes_read = %d\n", buf, bytes_read);
      printf("<===\n");
      printf(buf);
    }
  }

  return TRUE;
}

int main(int argc, char *argv[]) {
  struct sockaddr_in sa = {0};
  GIOChannel *channel = NULL;
  GMainLoop *loop = NULL;
  GSource *source = NULL;

  tcpsock = socket(AF_INET, SOCK_STREAM, 0);
  if(tcpsock < 0) {
    DEBUG("create socket failed\n");
    return -1;
  }

  sa.sin_family = AF_INET;
  sa.sin_port = htons((u_short) 10086);
  sa.sin_addr.s_addr = inet_addr("127.0.0.1");

  if(connect(tcpsock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
    DEBUG("Connect to msgproxy failed!\n");
    close(tcpsock);
    return -1;
  } else {
    DEBUG("Connected to msgproxy.\n");
    write(tcpsock, "MW-CLIENT", 9);
  }

  channel = g_io_channel_unix_new(tcpsock);
  g_io_add_watch(channel, G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL, _data_watch, NULL);
  g_io_channel_unref(channel);

  channel = g_io_channel_unix_new((gint)1);
  source = g_io_create_watch(channel, G_IO_IN);
  g_io_channel_unref(channel);
  g_source_set_callback(source, (GSourceFunc)_input_callback, channel, NULL);
  g_source_attach(source, NULL);
  g_source_unref(source);

  loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);

  return 0;
}
