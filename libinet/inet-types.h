#ifndef __LIBINET_H__
#define __LIBINET_H__

#define DEBUG(...) printf("[i]" __VA_ARGS__)
#define ERROR(...) printf("[*]" __VA_ARGS__)

#define RETOK      return 0
#define RETERR     return -__LINE__

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

typedef struct {
  char name[IFNAMSIZ];
  short type;
  short flags;

  struct sockaddr addr;
  struct sockaddr broadaddr;
  struct sockaddr netmask;

  int has_ip;
  char hwaddr[32];
} if_info_t;

typedef struct {
  char name[IFNAMSIZ];
  int idx;
} if_elem_t;

typedef struct {
  if_elem_t *array;
  int cnt;
} if_array_t;

typedef struct {
  int fd;
} inet_core_t;

/**common inline function entry*/
inline int inet_core_init(inet_core_t *ic) {
  ic->fd = socket(AF_INET, SOCK_DGRAM, 0);
  return ic->fd;
}

inline void inet_core_free(inet_core_t *ic) {
  if(ic->fd)
    close(ic->fd);
}

inline void inet_dump_hwaddr(unsigned char *hwaddr) { 
  printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]); 
}

inline void if_array_init(if_array_t *ia) {
  ia->array = NULL;
  ia->cnt = 0;
}

inline void if_array_free(if_array_t *ia) {
  int i = 0;

  if(ia->array) {
    free(ia->array);
  }
}

/**implement in inet-common.c*/
int inet_get_if(inet_core_t *ic, if_array_t *ia);

int inet_if_up(inet_core_t *ic, char *ifname);
int inet_if_down(inet_core_t *ic, char *ifname);

int inet_if_fetch_info(inet_core_t *ic, char *ifname, if_info_t *ii);

#endif
