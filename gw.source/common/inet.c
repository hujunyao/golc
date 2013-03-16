#include <stdio.h>
#include <alloca.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>

#include "inet.h"

#define GTAG "GW.INET"
#include "log.h"

#define INET_PROC_NETDEV_PATH  "/proc/net/dev"

int inet_core_init(inet_core_t *ic) {
  ic->fd = socket(AF_INET, SOCK_DGRAM, 0);
  return ic->fd;
}

void inet_core_free(inet_core_t *ic) {
  if(ic->fd)
    close(ic->fd);
}

void if_array_init(if_array_t *ia) {
  ia->array = NULL;
  ia->cnt = 0;
}

void if_array_free(if_array_t *ia) {
  if(ia->array) {
    free(ia->array);
  }
}

void inet_dump_hwaddr(unsigned char *hwaddr) {
	printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);
}

static int inet_get_if_by_ioctl(inet_core_t *ic, if_array_t *ia) {
  struct ifconf ifc = {0};
  int rq_len = 0, last_len, cnt = 0, i = 0;
  struct ifreq *ifr = NULL, *end =NULL , *cur = NULL;
  int len = sizeof(struct sockaddr);

  ifc.ifc_buf = NULL;

  if(ic->fd > 0) {
    rq_len = 4* sizeof (struct ifreq);
    do {
      last_len = ifc.ifc_len;
      ifc.ifc_buf = realloc(ifc.ifc_buf, ifc.ifc_len = rq_len);
      if (ifc.ifc_buf == NULL || ioctl (ic->fd, SIOCGIFCONF, &ifc) < 0) {
        close(ic->fd);
        return -1;
      }
      rq_len = rq_len >> 1;
    } while(ifc.ifc_len != last_len);

    ifr = (struct ifreq *)ifc.ifc_req;
    end = (struct ifreq *)((caddr_t)ifr + ifc.ifc_len);
    while(ifr < end) {
      cur = ifr;
      ifr = (struct ifreq *)((caddr_t)ifr + len + IFNAMSIZ);
      if(cur->ifr_addr.sa_family != AF_INET)
        continue;
      cnt ++;
    }

    ia->array = calloc(cnt + 1, sizeof(if_elem_t));
    ifr = (struct ifreq *)ifc.ifc_req;
    for(i=0; ifr < end; i++) {
      cur = ifr;
      ifr = (struct ifreq *)((caddr_t)ifr + len + IFNAMSIZ);
      if(cur->ifr_addr.sa_family != AF_INET)
        continue;
      strncpy(ia->array[i].name, cur->ifr_name, IFNAMSIZ);
      ioctl (ic->fd, SIOCGIFINDEX, cur);
      ia->array[i].idx  = cur->ifr_ifindex;
    }

    free(ifc.ifc_buf);
    ia->cnt = i;
  }

  return 0;
}

static char *get_name(char *name, char *p) {
  while (isspace(*p))
    p++;

  while (*p) {
    if (isspace(*p))
      break;
    if (*p == ':') {        /* could be an alias */
      char *dot = p, *dotname = name;
      *name++ = *p++;
      while (isdigit(*p))
        *name++ = *p++;
      if (*p != ':') {    /* it wasn't, backup */
        p = dot;
        name = dotname;
      }
      if (*p == '\0')
        return NULL;
      p++;
      break;
    }
    *name++ = *p++;
  }
  *name++ = '\0';
  return p;
}

int inet_get_if(inet_core_t *ic, if_array_t *ia) {
  FILE *fp = NULL;
  char buf[512];
  int ndver = 1, memsize = 0;

  fp = fopen(INET_PROC_NETDEV_PATH, "r");
  if(! fp) {
    DD("cannot open %s, output limited.\n", INET_PROC_NETDEV_PATH);
    return inet_get_if_by_ioctl(ic, ia);
  }

  fgets(buf, sizeof buf, fp);
  fgets(buf, sizeof buf, fp);

  if(strstr(buf, "compressed"))
    ndver = 3;
  else if(strstr(buf, "bytes"))
    ndver = 2;

  while(fgets(buf, sizeof buf, fp)) {
    ia->array = realloc(ia->array, memsize + sizeof(if_elem_t));
    get_name(ia->array[ia->cnt].name, buf);
    memsize += sizeof(if_elem_t);
    ia->cnt ++;
  }

  fclose(fp);

  return 0;
}

static int set_flag(int fd, char *ifname, short flag) {
  struct ifreq ifr;

  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  if(ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
    EE(" %s interface: %s\n", strerror(errno), ifname);
    return -1;
  }

  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  ifr.ifr_flags |= flag;
  if(ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
    EE("set flag %x to interface %s failure\n", flag, ifname);
    return -1;
  }

  return 0;
}

static int clr_flag(int fd, char *ifname, short flag) {
  struct ifreq ifr;

  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  if(ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
    EE(" %s interface: %s\n", strerror(errno), ifname);
    return -1;
  }

  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  ifr.ifr_flags &= ~flag;
  if(ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
    EE("set flag %x to interface %s failure\n", flag, ifname);
    return -1;
  }

  return 0;
}

int inet_if_up(inet_core_t *ic, char *ifname) {
  if(ic->fd) {
    return set_flag(ic->fd, ifname, (IFF_UP | IFF_RUNNING));
  }
  return -1;
}

int inet_if_down(inet_core_t *ic, char *ifname) {
  if(ic->fd) {
    return clr_flag(ic->fd, ifname, IFF_UP);
  }
  return -1;
}

int inet_if_fetch_info(inet_core_t *ic, char *ifname, if_info_t *ii) {
  struct ifreq ifr;

  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  if (ioctl(ic->fd, SIOCGIFFLAGS, &ifr) < 0)
		return -1;
  ii->flags = ifr.ifr_flags;

  strncpy(ii->name, ifname, IFNAMSIZ);

  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  if (ioctl(ic->fd, SIOCGIFHWADDR, &ifr) < 0) {
    memset(ii->hwaddr, 0, 32);
		return -1;
  } else {
    memcpy(ii->hwaddr, ifr.ifr_hwaddr.sa_data, 8);
  }
  ii->type = ifr.ifr_hwaddr.sa_family; /*ref HARDWARE identifiers from net/if_arp.h*/

  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  ifr.ifr_addr.sa_family = AF_INET;
  if (ioctl(ic->fd, SIOCGIFADDR, &ifr) != 0) {
    memset(&ii->addr, 0, sizeof(struct sockaddr));
		return -1;
  } else {
    ii->has_ip = 1;
    ii->addr = ifr.ifr_addr;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    if(ioctl(ic->fd, SIOCGIFBRDADDR, &ifr) < 0) {
      memset(&ii->broadaddr, 0, sizeof(struct sockaddr));
    } else {
      ii->broadaddr = ifr.ifr_broadaddr;
    }

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    if(ioctl(ic->fd, SIOCGIFNETMASK, &ifr) < 0) {
      memset(&ii->netmask, 0, sizeof(struct sockaddr));
    } else {
      ii->netmask = ifr.ifr_netmask;
    }
  }

	return 0;
}

static int if_set_address(int fd, char *ifname, int flag, in_addr_t ip) {
  struct ifreq ifr;
  struct sockaddr_in sin;

  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  memset(&sin, 0, sizeof(struct sockaddr));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = ip;
  memcpy(&ifr.ifr_addr, &sin, sizeof(struct sockaddr));

  return ioctl(fd, flag, &ifr);
}

char *inet_if_get_ipstr(if_info_t *ii) {
  struct sockaddr_in *ip = NULL;

  ip = (struct sockaddr_in *)&(ii->addr);

  return inet_ntoa(ip->sin_addr);
}

int inet_if_set_ip (inet_core_t *ic, char *ifname, char *ip, char *nm, char *bc) {
  if(ip && if_set_address(ic->fd, ifname, SIOCSIFADDR, inet_addr(ip)) < 0) {
    EE("set ip %s for %s failed!\n", ip, ifname);
    return -1;
  }

  if(nm && if_set_address(ic->fd, ifname, SIOCSIFNETMASK, inet_addr(nm)) < 0) {
    EE("set netmask %s for %s failed!\n", nm, ifname);
    return -1;
  }

  if(bc && if_set_address(ic->fd, ifname, SIOCSIFBRDADDR, inet_addr(bc)) < 0) {
    EE("set broadcast %s for %s failed!\n", bc, ifname);
    return -1;
  }
  return 0;
}

int inet_if_type_is_ether(if_info_t *ii) {
  return ii->type == ARPHRD_ETHER;
}

int inet_if_type_is_ppp(if_info_t *ii) {
  return ii->type == ARPHRD_PPP;
}
