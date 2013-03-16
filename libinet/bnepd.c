/**compile command: gcc -o bnepd bnepd.c -I /usr/include `pkg-config glib-2.0 --cflags --libs`*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <string.h>
#include <glib.h>

#include "inet-common.c"

#undef DEBUG
#define DEBUG(...) fprintf(stdout, "[bnepd] "__VA_ARGS__)
#define MSG_MAX_SIZE 1024
#define MAX_EVENTS 16

static GPid g_udhcpd_pid[8] = {0};
static gboolean launch_app_async(gchar *argv[], gchar *env[], GPid *pid, gint flag) {
  gboolean ret;
  GError *err = NULL;

  ret = g_spawn_async(NULL, argv, env, G_SPAWN_SEARCH_PATH|flag, NULL, NULL, pid, &err);
  if(ret == FALSE) {
    DEBUG("**LAUNCH APP %s %s failed**\n", argv[0], argv[1]);
  }

  return ret;
}

static void _udhcpd_watch_cb(GPid pid, gint status, gpointer data) {
  time_t t = time(NULL);
  char buf[128] = {0};
  char *str = ctime(&t);
  int len = strlen(str);

  strncpy(buf, str, len - 6);
  fprintf(stdout, "%s - udhcpd %s exit status %d\n", buf, (char *)data, status);
}

static GPid launch_udhcpd_async(char *cfg) {
  GPid pid;
  gchar *argv[] = {"/usr/sbin/udhcpd", "-f", NULL, NULL};
  argv[2] = cfg;
  if(launch_app_async(argv, NULL, &pid, G_SPAWN_DO_NOT_REAP_CHILD)) {
    time_t t = time(NULL);
    char buf[128] = {0};
    char *str = ctime(&t);
    int len = strlen(str);
    strncpy(buf, str, len - 6);

    fprintf(stdout, "%s - udhcpd %s\n", buf, cfg);
    //g_child_watch_add(pid, _udhcpd_watch_cb, cfg);
    return pid;
  }

  return -1;
}

static int do_bnep_up(char *eth) {
  inet_core_t ic ;
  char *ip = NULL;
  char *nm = "255.255.255.0";
  char *bc = NULL;
  char *udhcpdcfg = NULL;
  int idx = 0;
  
  inet_core_init(&ic);
  if(strcmp(eth, "bnep0") == 0) {
  	ip = "192.168.220.1";
  	bc = "192.168.220.255";
    udhcpdcfg = "/etc/udhcpd.bnep0.conf";
    idx = 0;
  } else if(strcmp(eth, "bnep1") == 0) {
  	ip = "192.168.221.1";
  	bc = "192.168.221.255";
    udhcpdcfg = "/etc/udhcpd.bnep1.conf";
    idx = 1;
  } else if(strcmp(eth, "bnep2") == 0) {
  	ip = "192.168.222.1";
  	bc = "192.168.222.255";
    udhcpdcfg = "/etc/udhcpd.bnep2.conf";
    idx = 2;
  } else if(strcmp(eth, "bnep3") == 0) {
  	ip = "192.168.223.1";
  	bc = "192.168.223.255";
    udhcpdcfg = "/etc/udhcpd.bnep3.conf";
    idx = 3;
  } else if(strcmp(eth, "bnep4") == 0) {
  	ip = "192.168.224.1";
  	bc = "192.168.224.255";
    udhcpdcfg = "/etc/udhcpd.bnep4.conf";
    idx = 4;
  } else if(strcmp(eth, "bnep5") == 0) {
  	ip = "192.168.225.1";
  	bc = "192.168.225.255";
    udhcpdcfg = "/etc/udhcpd.bnep5.conf";
    idx = 5;
  }
  
  if(bc != NULL && ip != NULL) {
  	inet_if_set_ip(&ic, eth, ip, nm, bc);
    inet_if_up(&ic, eth);
  } else
  	DEBUG("unsupport network interface %s\n", eth);
  
  inet_core_free(&ic);
  g_udhcpd_pid[idx] = launch_udhcpd_async(udhcpdcfg);
}

int do_bnep_down(char *eth) {
  int idx =0;

  if(strcmp(eth, "bnep1") == 0) {
    idx = 1;
  } else if(strcmp(eth, "bnep2") == 0) {
    idx = 2;
  } else if(strcmp(eth, "bnep3") == 0) {
    idx = 3;
  } else if(strcmp(eth, "bnep4") == 0) {
    idx = 4;
  } else if(strcmp(eth, "bnep5") == 0) {
    idx = 5;
  }
  DEBUG("interface %s down, pid = %d\n", eth, g_udhcpd_pid[idx]);

  if(g_udhcpd_pid[idx] > 0) {
    g_spawn_close_pid(g_udhcpd_pid[idx]);
    kill(g_udhcpd_pid[idx], SIGKILL);
  }

  return 0;
}

int main(int argc, char *argv[]) {
  struct sockaddr_in sa = {0};
  int tcpsock = -1, clisock = -1, opt = 1, idx = 0;
  int epollfd = 0, nfds = 0;
  struct epoll_event ev, events[MAX_EVENTS];

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
  if(listen(tcpsock, MAX_EVENTS) < 0) {
    DEBUG("listen tcpsock failed\n");
  }

  signal(SIGCHLD,SIG_IGN);
  DEBUG("bnepd is running\n");
  epollfd = epoll_create(MAX_EVENTS);
  if(epollfd == -1) {
    DEBUG("epoll created failed\n");
    close(tcpsock);
    return -1;
  }

  ev.events = EPOLLIN;
  ev.data.fd = tcpsock;

  if(epoll_ctl(epollfd, EPOLL_CTL_ADD, tcpsock, &ev) == -1) {
    DEBUG("epoll ctl failed\n");
    close(tcpsock);
    return -2;
  }

  do {
    nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
    if(nfds == -1) {
      //DEBUG("epoll_wait failed\n");
      continue;
    }
    for(idx=0; idx < nfds; idx++) {
      int efd = events[idx].data.fd;
      if(efd == tcpsock) { /**accept new client connection*/
        int len, cli;
        struct sockaddr_in clisa = {0};

        len = sizeof(struct sockaddr_in);
        cli = accept(tcpsock, (struct sockaddr *)&clisa, (socklen_t *) &len);
        if(cli == -1) {
          DEBUG("accept new client failed, strerror = %s\n", strerror(errno));
          continue;
        }
        //setnonblocking(cli);
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = cli;
        if(epoll_ctl(epollfd, EPOLL_CTL_ADD, cli, &ev) == -1) {
          DEBUG("add new client fd to epoll failed\n");
          close(cli);
          continue;
        }
        //DEBUG("add new client %d to epoll\n", cli);
      } else { /**process client command*/
        char buf[MSG_MAX_SIZE] = {0};
        int bytes_read = 0;
        bytes_read = read(efd, buf, MSG_MAX_SIZE);
        //DEBUG("bytes_read = %d, %s\n", bytes_read, buf);
        if(bytes_read <= 0) {
          DEBUG("delete cli(fd= %d) from epoll\n", efd);
          epoll_ctl(epollfd, EPOLL_CTL_DEL, events[idx].data.fd, NULL);
          close(efd);
        } else {
          if(strncmp(buf, "up bnep", 7) == 0) {
            do_bnep_up(buf+3);
          } else if(strncmp(buf, "down bnep", 9) == 0) {
            do_bnep_down(buf+5);
          }
        }
      }/**end of command*/
    }
  } while(1);

  return 0;
}
