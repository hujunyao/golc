#ifndef __GW_IPC_H__
#define __GW_IPC_H__
#include <sys/un.h>
#include <sys/socket.h>
#include <event.h>

typedef void (* gw_ipc_callback_t)(char *from, char *membuf, void *data);

typedef struct {
  evutil_socket_t fd;
	struct event_base *base;
	char path[128];
	const char *obj;
	gw_ipc_callback_t ipccb;
	void *data;
}gw_ipc_t;

gw_ipc_t *gw_ipc_new(struct event_base *base);

int gw_ipc_setup(gw_ipc_t *ipc, const char *obj, gw_ipc_callback_t ipccb, void *data);
//int gw_ipc_server_setup(gw_ipc_t *ipc);

int gw_ipc_register(gw_ipc_t *ipc, const char *objname);

int gw_ipc_sendmsg(gw_ipc_t *ipc, const char *dest, unsigned char *membuf, int nsize);
int gw_ipc_set_read_callback(gw_ipc_t *ipc, event_callback_fn ipc_read, void *data);

int gw_ipc_is_available(const char *obj);

void gw_ipc_free(gw_ipc_t *ipc);
#endif
