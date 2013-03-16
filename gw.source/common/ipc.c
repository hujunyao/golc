#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "ipc.h"
#include "base.h"

#define GTAG "GW.IPC"
#include "log.h"

#define GW_IPC_PATH "/tmp/gw.ipcd"

gw_ipc_t *gw_ipc_new(struct event_base *base) {
  gw_ipc_t *ipc = calloc(1, sizeof(gw_ipc_t));

  ipc->fd = gw_ipcsock_new();
	ipc->base = base;

	return ipc;
}

void gw_ipc_free(gw_ipc_t *ipc) {
  if(ipc != NULL) {
		if(ipc->fd)
			close(ipc->fd);

		unlink(ipc->path);
		free(ipc);
	}
}

static void _signal_cb(evutil_socket_t fd, short flag, void *data) {
	gw_ipc_t *ipc = (gw_ipc_t *)data;
	DD("_signal_cb enter SIGINT\n");
	event_base_loopexit(ipc->base ,NULL);
}

static void _ipc_server_read(evutil_socket_t fd, short flag, void *data) {
	if(flag & EV_READ) {
		int nread = 0;
		socklen_t len;
		struct sockaddr_un caddr = {0};
		char membuf[2048] = {0};
		gw_ipc_t *ipc = (gw_ipc_t *)data;
		char *bufptr = NULL;

		nread = recvfrom(fd, membuf, 2048, 0, (struct sockaddr *)&caddr, &len);
		bufptr = strchr(membuf, ':');
		if(bufptr) {
			*bufptr = '\0';
			if(ipc->ipccb)
				(* (ipc->ipccb))(membuf, bufptr+1, ipc->data);
		}
	}
}

int gw_ipc_setup(gw_ipc_t *ipc, const char *obj, gw_ipc_callback_t ipccb, void *data) {
  struct sockaddr_un saddr;
	struct event *e = NULL;
	int ret;
	struct event *sigevt = NULL;

	snprintf(ipc->path, 128, "/tmp/%s", obj);
	ipc->obj = obj;

  bzero(&saddr, sizeof(struct sockaddr_un));
  saddr.sun_family = AF_UNIX;
  strncpy(saddr.sun_path, ipc->path, sizeof(saddr.sun_path)-1);
  ret = bind(ipc->fd, (struct sockaddr *)&saddr, sizeof(saddr));

	if(ret == 0) {
		RAW("***IPC %s ***\n", obj);
		sigevt = event_new(ipc->base, SIGINT, EV_SIGNAL|EV_PERSIST, _signal_cb, ipc);
		event_add(sigevt, NULL);
	} else {
		DD("***IPC %s failed ***\n", obj);
	}
	ipc->ipccb = ipccb;
	ipc->data  = data;

	if(ipccb) {
		e = event_new(ipc->base, ipc->fd, EV_READ|EV_PERSIST, _ipc_server_read, ipc);
	} else {
		e = event_new(ipc->base, ipc->fd, EV_READ|EV_PERSIST, _ipc_server_read, ipc);
	}
	event_add(e, NULL);

  return ret;
}

#if 0
int gw_ipc_server_setup(gw_ipc_t *ipc, gw_ipc_callback_t ipccb) {
  struct sockaddr_un saddr;
	struct event *e = NULL;
	struct event *sigevt = NULL;
	int ret;

	strncpy(ipc->path, GW_IPC_PATH, 128);
  bzero(&saddr, sizeof(struct sockaddr_un));
  saddr.sun_family = AF_UNIX;
  strncpy(saddr.sun_path, GW_IPC_PATH, sizeof(saddr.sun_path)-1);
  ret = bind(ipc->fd, (struct sockaddr *)&saddr, sizeof(saddr));
	if(ret == 0) {
		RAW("***IPC %s ***\n", "gw.ipcd");
		sigevt = event_new(ipc->base, SIGINT, EV_SIGNAL|EV_PERSIST, _signal_cb, ipc);
		event_add(sigevt, NULL);
	}

	if(ipccb) {
		e = event_new(ipc->base, ipc->fd, EV_READ|EV_PERSIST, _ipc_server_read, ipccb);
	} else {
		e = event_new(ipc->base, ipc->fd, EV_READ|EV_PERSIST, _ipc_server_read, ipccb);
	}
	event_add(e, NULL);

  return ret;
}
#endif

int gw_ipc_register(gw_ipc_t *ipc, const char *objname) {
  if(ipc != NULL && ipc->fd > 0) {
    struct sockaddr_un saddr;
		char membuf[1024] = {0};
		int ret;

    bzero(&saddr, sizeof(struct sockaddr_un));
    saddr.sun_family = AF_UNIX;
		strncpy(saddr.sun_path, GW_IPC_PATH, sizeof(saddr.sun_path) -1);
		snprintf(membuf, 1024, "REGOBJ:%s", objname);
    ret = sendto(ipc->fd, membuf, strlen(membuf), 0, (struct sockaddr *)&saddr, sizeof(saddr));
		return ret;
	}

	return -1;
}

int gw_ipc_sendmsg(gw_ipc_t *ipc, const char *dest, unsigned char *membuf, int nsize) {
	int ret;
	char path[128] = {0};
  struct sockaddr_un saddr;
	char *newbuf = NULL;
	int len = strlen(ipc->obj);
	int bufsize = nsize + len + 1;

	snprintf(path, 128, "/tmp/%s", dest);
  bzero(&saddr, sizeof(struct sockaddr_un));
  saddr.sun_family = AF_UNIX;
  strncpy(saddr.sun_path, path, sizeof(saddr.sun_path)-1);
	newbuf = calloc(1, bufsize);
	memcpy(newbuf, ipc->obj, len);
	*(newbuf+len) = ':';
	memcpy(newbuf+len+1, membuf, nsize);

	ret = sendto(ipc->fd, newbuf, bufsize, 0, (struct sockaddr *)&saddr, sizeof(saddr));

	free(newbuf);

	return ret;
}

int gw_ipc_set_read_callback(gw_ipc_t *ipc, event_callback_fn ipc_read, void *data) {
	if(ipc != NULL) {
		struct event *e = NULL;

		e = event_new(ipc->base, ipc->fd, EV_READ|EV_PERSIST, ipc_read, data);
		event_add(e, NULL);
		return 0;
	}
	return -1;
}

int gw_ipc_is_available(const char *obj) {
	int ret;
	char path[1024] = {0};

	snprintf(path, 1024, "/tmp/%s", obj);
	ret = access(path, X_OK);

	return ret;
}

