/**compile command: gcc -o msgproxy msgproxy.c -I /usr/include `pkg-config glib-2.0 --cflags --libs`*/
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <signal.h>
#include <glib.h>

//#define DEBUG(...) fprintf(stdout, "[msgproxy] **source = " __FILE__ " line = %d func = %s** ", __LINE__, __func__); fprintf(stdout, __VA_ARGS__)
#define DEBUG(...) fprintf(stdout, "[msgproxy] " __VA_ARGS__)
#define MSG_MAX_SIZE 102400
#define DAV_PORT_BASE 10010
#define DAV_PID_MAX 12

enum {
  MSG_TO_TB = 1<<0,
  MSG_TO_MW = 1<<1
};

enum {
  TB_LOG = 1<<0,
  MW_LOG = 1<<1
};

typedef struct {
  GIOChannel *source;
  gchar *davconfig;
  GPid pid;
} dav_process_t;

static GIOChannel *g_tb_source = NULL, *g_mw_source = NULL, *g_cmd_source = NULL, *g_log_source = NULL;
static gchar g_cached_msg[MSG_MAX_SIZE] = {0};
static gint g_cached_msg_mask = 0, g_cached_msg_size = 0;;
static gint g_log_from = 0;
static dav_process_t g_dav_pid[DAV_PID_MAX];

static void io_channel_except(GIOChannel *source) {
  if(g_tb_source == source) {
    if(g_mw_source) {
      gchar msg[] = {0x0b, 0x00, 0x00, 0x00, 0x00};
      gint peerfd = g_io_channel_unix_get_fd(g_mw_source);
      write(peerfd, msg, sizeof(msg));
      DEBUG("send thunderbird exit notify to mail widget\n");
    }
    g_cached_msg_mask = 0;
    g_cached_msg_size = 0;
    memset(g_cached_msg, 0, MSG_MAX_SIZE);
    g_tb_source = NULL;
  }
  if(g_mw_source == source) {
    g_mw_source = NULL;
  }
  if(g_cmd_source == source) {
    g_cmd_source = NULL;
  }
  {
    gint idx = 0;
    for(; idx<DAV_PID_MAX; idx++) {
      if(source == g_dav_pid[idx].source) {
        g_dav_pid[idx].source = NULL;
        break;
      }
    }
  }
  if(g_log_source == source) {
    g_log_source = NULL;
    g_log_from = 0;
  }
}

static gboolean io_channel_processID(GIOChannel *source, gchar *buf) {
  if(! g_tb_source) {
    if(g_strcmp0(buf, "TB3-CLIENT") == 0) {
      g_tb_source = source;
      if(g_mw_source) {
        gchar msg[] = {0x0a, 0x00, 0x00, 0x00, 0x00};
        gint peerfd = g_io_channel_unix_get_fd(g_mw_source);
        write(peerfd, msg, sizeof(msg));
        DEBUG("send thunderbird start notify to mail widget\n");
      }
      return TRUE;
    }
  }
  if(! g_mw_source) {
    if(g_strcmp0(buf, "MW-CLIENT") == 0) {
      g_mw_source = source;
      return TRUE;
    }
  }
  if(! g_cmd_source) {
    if(g_strcmp0(buf, "CMD-CLIENT") == 0) {
      g_cmd_source = source;
      return TRUE;
    }
  }
  if(g_strcmp0(buf, "DAV-CLIENT") == 0) {
    gint idx = 0;
    for(; idx<DAV_PID_MAX; idx++) {
      if(g_dav_pid[idx].source == NULL) {
        g_dav_pid[idx].source = source;
        break;
      }
    }
    return FALSE;
  }
  if(! g_log_source) {
    if(g_strcmp0(buf, "TB-LOG-CLIENT") == 0) {
      g_log_source = source;
      g_log_from = TB_LOG;
      return TRUE;
    } else if(g_strcmp0(buf, "MW-LOG-CLIENT") == 0) {
      g_log_source = source;
      g_log_from = MW_LOG;
      return TRUE;
    }
  }

  return FALSE;
}

static gboolean launch_app_async(gchar *argv[], gchar *env[], GPid *pid, gint flag) {
  gboolean ret;
  GError *err = NULL;

  ret = g_spawn_async(NULL, argv, env, G_SPAWN_SEARCH_PATH|flag, NULL, NULL, pid, &err);
  if(ret == FALSE) {
    DEBUG("**LAUNCH APP %s %s failed**\n", argv[0], argv[1]);
  }

  return ret;
}

static void _app_watch_cb(GPid pid, gint status, gpointer data) {
  DEBUG("thunderbird exit status %d\n", status);
}

static void launch_thunderbird_async() {
  GPid pid;
  gchar *env[]  = {"TB3_HIDEUI_STARTUP=true", NULL};
  gchar *argv[] = {"/usr/bin/TB3-locale", "/opt/thunderbird-3.0.1/lib/thunderbird-3.0.1/thunderbird -UILocale %s", NULL};
  if(launch_app_async(argv, NULL, &pid, G_SPAWN_DO_NOT_REAP_CHILD)) {
    g_child_watch_add(pid, _app_watch_cb, NULL);
  }
}

static GPid launch_davmail_async(gchar *filename) {
  GPid pid;
  gchar *argv[] = {"/usr/bin/davmail", NULL, NULL};
  argv[1] = filename;
  if(launch_app_async(argv, NULL, &pid, G_SPAWN_DO_NOT_REAP_CHILD)) {
    g_child_watch_add(pid, _app_watch_cb, NULL);
  }
  DEBUG("davmail %s pid = %d\n", filename, pid);
  return pid;
}

static void _load_dav_config(gchar *filename) {
  gint idx = 0;
  DEBUG("load davconfig %s\n", filename);

  for(; idx<DAV_PID_MAX; idx++) {
    if(g_dav_pid[idx].davconfig == NULL) {
      g_dav_pid[idx].davconfig = g_strdup(filename);
      g_dav_pid[idx].pid = launch_davmail_async(filename);
      break;
    }
  }
}

static void _unload_dav_config(gchar *filename) {
  gint idx = 0;
  DEBUG("unload davconfig %s\n", filename);

  for(; idx<DAV_PID_MAX; idx++) {
    gchar *str = g_dav_pid[idx].davconfig;
    if(str && strcmp(str, filename)==0) {
      g_free(g_dav_pid[idx].davconfig);
      //g_spawn_close_pid(g_dav_pid[idx].pid);
      kill(g_dav_pid[idx].pid, 9);
      break;
    }
  }
}

static void _restart_dav_mail(gchar *filename) {
  gint idx = 0;
  DEBUG("restart davmail %s\n", filename);

  for(; idx<DAV_PID_MAX; idx++) {
    gchar *str = g_dav_pid[idx].davconfig;
    if(str && strcmp(str, filename)==0) {
      //g_spawn_close_pid(g_dav_pid[idx].pid);
      kill(g_dav_pid[idx].pid, 9);
      g_dav_pid[idx].pid = launch_davmail_async(filename);
      break;
    }
  }
}

gboolean _cli_watch (GIOChannel *source, GIOCondition cond, gpointer data) {
  if(cond & (G_IO_ERR|G_IO_HUP|G_IO_NVAL)) {
    DEBUG("TCP socket fd is INVALID.\n");
    return FALSE;
  }

  if(cond & G_IO_IN) {
    int sockfd = g_io_channel_unix_get_fd(source);
    gchar buf[MSG_MAX_SIZE] = {0};
    gsize len = MSG_MAX_SIZE, bytes_read;

    bytes_read = read(sockfd, buf, MSG_MAX_SIZE); 
    if(*buf > ' ')
      DEBUG("buf = %s, bytes_read = %d\n", buf, bytes_read);

    if(bytes_read <= 0) {
      DEBUG("errno = %x, msg = %s\n", errno, strerror(errno));
      close(sockfd);
      io_channel_except(source);
      return FALSE;
    } else {
      //DEBUG("g_tb_source = %x, g_mw_source = %x\n", g_tb_source, g_mw_source);
      if(strncmp(buf, "DAV_RESTART", 11) == 0) {
        _restart_dav_mail(buf+12);
      } else if(strncmp(buf, "DAV_LOAD_CONFIG", 15) == 0) {
        _load_dav_config(buf+16);
      } else if(strncmp(buf, "DAV_UNLOAD_CONFIG", 17) == 0) {
        _unload_dav_config(buf+18);
      } else if(io_channel_processID(source, buf)) {
        GIOChannel *peer = g_tb_source;
        if(g_cached_msg_mask == MSG_TO_MW) {
          //peer = g_mw_source;
          peer = NULL; /**DOn't send cached message to MailWidget*/
        }
        if(peer && g_cached_msg_mask == MSG_TO_TB) {
          int peerfd = g_io_channel_unix_get_fd(peer);
          sleep(1);
          write(peerfd, g_cached_msg, g_cached_msg_size);
          g_cached_msg_mask = 0;
        }
        return TRUE;
      } else {
        GIOChannel *peer = g_tb_source;
        gint mask = MSG_TO_TB;

        if(g_tb_source == source) {
          peer = g_mw_source;
          mask = MSG_TO_MW;
        }

        if(peer) {
          gint peerfd = g_io_channel_unix_get_fd(peer);
          write(peerfd, buf, bytes_read);
          //DEBUG("send %x,%x,%x,%x %s to %x mask = %x, bytes_read = %d\n", *buf, *(buf+1), *(buf+2), *(buf+3), buf+4, peer, mask, bytes_read);
        } else {
          /*only cache message from mailwidget*/
          if(g_mw_source == source) {
            g_cached_msg_mask = mask;
            g_cached_msg_size = bytes_read;
            memset(g_cached_msg, 0, MSG_MAX_SIZE);
            strncpy(g_cached_msg, buf, bytes_read);
          } else if(g_cmd_source == source) {
            if(g_strcmp0(buf+1, "_REMOTE_CMD,TRAYCLICK") == 0) {
              launch_thunderbird_async();
            } else if(g_strcmp0(buf+1, "_REMOTE_CMD,CLEANSTORAGE") == 0) {
              if(g_mw_source) {
                gchar msg[] = {0x0c, 0x00, 0x00, 0x00, 0x00};
                gint peerfd = g_io_channel_unix_get_fd(g_mw_source);
                write(peerfd, msg, sizeof(msg));
                DEBUG("send clean storage notify to mail widget\n");
              }
            }
          }
        }
        if(g_log_source == source) {
          gint logfd = g_io_channel_unix_get_fd(g_log_source);
          if(g_log_from == TB_LOG && mask == MSG_TO_TB) {
            write(logfd, buf, bytes_read);
          } else if(g_log_from == MW_LOG && mask == MSG_TO_MW) {
            write(logfd, buf, bytes_read);
          }
        }/**just for log command*/
      }
    }
  }

  return TRUE;
}

gboolean _new_connection_watch (GIOChannel *source, GIOCondition cond, gpointer data) {
  gint svrfd = g_io_channel_unix_get_fd(source);

  if(cond & (G_IO_ERR|G_IO_HUP|G_IO_NVAL)) {
    DEBUG("TCP socket fd is INVALID.\n");
    return FALSE;
  }

  if(cond & G_IO_IN) {
    struct sockaddr_in sa = {0};
    int len, cli;
    int opt = 1, ret;

    DEBUG("accept new client connection \n");
    cli = accept(svrfd, (struct sockaddr *)&sa, (socklen_t *) &len);
    ret = setsockopt(cli, IPPROTO_TCP,TCP_NODELAY,(char *) &opt, sizeof(int));
    if(ret < 0) {
      DEBUG("set socket option TCP_NODELAY failed\n");
    }
    if(cli > 0) {
      GIOChannel *channel = NULL;

      channel = g_io_channel_unix_new(cli);
      g_io_add_watch(channel, G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL, _cli_watch, NULL);
      g_io_channel_unref(channel);
      DEBUG("add client fd %x to mainloop\n\n", cli);
    }
  }

  return TRUE;
}

int main(int argc, char *argv[]) {
  struct sockaddr_in sa = {0};
  int tcpsock = -1, clisock = -1, opt = 1, idx = 0;
  GIOChannel *channel = NULL;
  GMainLoop *loop = NULL;
  gchar mypath[1024] = {0};

  DEBUG("BUILD %s %s\n", __DATE__, __TIME__);
  tcpsock = socket(AF_INET, SOCK_STREAM, 0);
  if(tcpsock < 0) {
    DEBUG("create socket failed\n");
    return -1;
  }

  sa.sin_family = AF_INET;
  sa.sin_port = htons((u_short) 10086);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  setsockopt (tcpsock, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(opt));

  if(bind(tcpsock, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
    DEBUG("bind tcp socket failed\n");
  }
  if(listen(tcpsock, 32) < 0) {
    DEBUG("listen tcpsock failed\n");
  }

  DEBUG("msgproxy is ready now!\n");

  channel = g_io_channel_unix_new(tcpsock);
  g_io_add_watch(channel, G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL, _new_connection_watch, NULL);
  g_io_channel_unref(channel);

  g_snprintf(mypath, 1024, "%s/.davmail", getenv("HOME"));
  if(! g_file_test(mypath, G_FILE_TEST_EXISTS)) {
    g_mkdir(mypath, 0777);
  } else {
    for(; idx < DAV_PID_MAX; idx++) {
      gchar path[1024] = {0};
      g_snprintf(path, 1024, "%s/.davmail/davconfig.%d", getenv("HOME"), idx);
      if(g_file_test(path, G_FILE_TEST_EXISTS)) {
        DEBUG("load davconfig %s\n", path);
        g_dav_pid[idx].davconfig = g_strdup(path);
        g_dav_pid[idx].pid = launch_davmail_async(path);
      }
    }
  }

  loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(loop);

  return 0;
}
