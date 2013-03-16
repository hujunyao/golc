/** Compile command line : gcc -o qscp qscp.c `pkg-config libssh2 --cflags --libs`*/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <libssh2.h>

#define TRUE 1
#define FALSE 0

#define BLKSIZE 2048

#define DEFAULT_USER "root"
#define DEFAULT_PASS "123456"

#define ERROR(...) fprintf(stderr, "[ERROR] qscp: "__VA_ARGS__)
#define DEBUG(...) //fprintf(stdout, "[DEBUG] qscp: "__VA_ARGS__)

typedef struct {
  char *username, *password;
  char *scppath, *sshserver;
} remote_info_t;

static void usage(void) {
  printf("\t\t\tqscp v0.1 (write by WuRuXu)\n\n\tqscp [OPTIONS] localfile user:password@sshserver:/scppath\n \t\t-h \tshow help information\n\t\t-R, -r \tdirectory recursive copy\n");
}

static int parse_info(remote_info_t *ri, char *arg) {
  char *up = arg, *ptr = NULL, *hp = arg;

  ptr = strchr(arg, '@');
  if(NULL == ptr) {
    ri->username = NULL;
    ri->password = NULL;
  } else {
    *ptr++ = '\0', hp = ptr;

    ptr = strchr(up, ':');
    if(NULL == ptr) {
      ri->username = NULL;
      ri->password = NULL;
    } else {
      *ptr++ = '\0';
      ri->username = up;
      ri->password = ptr;
    }
  }

  ptr = strchr(hp, ':');
  if(NULL == ptr) {
    return -1;
  }
  *ptr++ = '\0';

  ri->sshserver = hp;
  ri->scppath  = ptr;
  if(ri->username == NULL) {
    ri->username = DEFAULT_USER;
    ri->password = DEFAULT_PASS;
  }

  return 0;
}

int send_file(LIBSSH2_SESSION *session, char *scppath, struct stat *pinfo, FILE *fp) {
  int ret, rc;
  char buff[BLKSIZE];
  LIBSSH2_CHANNEL *channel;
  char *ptr = NULL;

  channel = libssh2_scp_send(session, scppath, 0x1ff&pinfo->st_mode, (unsigned long)pinfo->st_size);
  if(!channel) {
    ERROR("unable to open a channel with ssh server.\n");
    return -1;
  }

  while((ret = fread(buff, 1, BLKSIZE, fp)) > 0) {
    ptr = buff;

    do {
      rc = libssh2_channel_write(channel, ptr, ret);
      ptr += rc, ret -= rc;
    } while(rc > 0);
  }

  libssh2_channel_send_eof(channel);
  libssh2_channel_wait_eof(channel);
  libssh2_channel_wait_closed(channel);
  libssh2_channel_free(channel);

  return 0;
}

int main(int argc, char *argv[]) {
  LIBSSH2_SESSION *session;

  FILE *fp;
  int ret, opt, sock;
  struct stat info;
  int rflag = FALSE;
  char *srcpath, *destpath;
  struct sockaddr_in sin;
  remote_info_t ri;

  if(argc <= 1) {
    usage();
    exit(0);
  }
  while((opt = getopt(argc, argv, "rRh")) != -1) {
    switch(opt) {
      case 'r':
      case 'R':
        rflag = TRUE; break;
      case 'h':
      default:
        usage();
        exit(0);
    }
  }

  srcpath = argv[optind++], destpath = argv[optind];
  DEBUG("opt = %d, argc = %d, srcpath = %s, destpath = %s\n", optind, argc, srcpath, destpath);

  stat(srcpath, &info);
  if(S_ISDIR(info.st_mode) && rflag==FALSE) {
    printf("%s is directory, append option -R for recursive copy\n");
    usage();
    exit(1);
  }
  if((ret = parse_info(&ri, destpath)) < 0) {
    usage();
    exit(0);
  }
  DEBUG("username=%s, password=%s, host = %s, scppath = %s\n", ri.username, ri.password, ri.sshserver, ri.scppath);

  sock = socket(AF_INET, SOCK_STREAM, 0);

  sin.sin_family = AF_INET;
  sin.sin_port = htons(22);
  sin.sin_addr.s_addr = inet_addr(ri.sshserver);
  if(connect(sock, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0) {
    ERROR("connect to %s failed!\n", ri.sshserver);
    return -1;
  }

  session = libssh2_session_init();
  if(!session)
    return -1;

  ret = libssh2_session_startup(session, sock);
  if(ret) {
    ERROR("establishing SSH session failed: ret = %d\n", ret);
    return -1;
  }


  if(libssh2_userauth_password(session, ri.username, ri.password)) {
    ERROR("authentication with ssh server failed.\n");
    return -1;
  }

  fp = fopen(srcpath, "r");
  if(fp != NULL) {
    send_file(session, ri.scppath, &info, fp);
    fclose(fp);
  }

  libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing");
  libssh2_session_free(session);
  //sleep(1);
  close(sock);

  return 0;
}
