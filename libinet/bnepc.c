/**compile command: gcc -o bnepc bnepc.c */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define MSG_MAX_SIZE 102400
#define DEBUG(...) fprintf(stdout, "[bnepc] "__VA_ARGS__)

int tcpsock = -1;

int main(int argc, char *argv[]) {
  struct sockaddr_in sa = {0};
  int opt = 1;
  char buf[1024] = {0};

  tcpsock = socket(AF_INET, SOCK_STREAM, 0);
  if(tcpsock < 0) {
    DEBUG("create socket failed\n");
    return -1;
  }

  sa.sin_family = AF_INET;
  sa.sin_port = htons((u_short) 10086);
  sa.sin_addr.s_addr = inet_addr("127.0.0.1");

  int ret = setsockopt(tcpsock, IPPROTO_TCP,TCP_NODELAY,(char *) &opt, sizeof(int));
  if(ret < 0) {
    DEBUG("set tcp socket option failed\n");
  }
  if(connect(tcpsock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
    DEBUG("Connect to bnepd failed!\n");
    close(tcpsock);
    return -1;
  } else {
    DEBUG("Connected to bnepd\n");
  }

  if(argv[1]) {
    char buf[32] = {0};
    if(strcmp(argv[0], "/usr/bin/bnepup") == 0) {
      snprintf(buf, 32, "up %s", argv[1]);
    } else if(strcmp(argv[0], "/usr/bin/bnepdown") == 0) {
      snprintf(buf, 32, "down %s", argv[1]);
    }
    DEBUG("write buf = %s\n", buf);
    write(tcpsock, buf, strlen(buf));
  }

  usleep(3000);
  close(tcpsock);

  return 0;
}
