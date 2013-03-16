#ifndef __GW_INET_H__
#define __GW_INET_H__

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <net/if.h>
#include "types.h"

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

int  inet_core_init(inet_core_t *ic);
void inet_core_free(inet_core_t *ic);
void if_array_init(if_array_t *ia);
void if_array_free(if_array_t *ia);
void inet_dump_hwaddr(unsigned char *hwaddr);

/**implement in inet-common.c*/
int inet_get_if(inet_core_t *ic, if_array_t *ia);
char *inet_if_get_ipstr(if_info_t *ii);

int inet_if_up(inet_core_t *ic, char *ifname);
int inet_if_down(inet_core_t *ic, char *ifname);

int inet_if_fetch_info(inet_core_t *ic, char *ifname, if_info_t *ii);

#endif
