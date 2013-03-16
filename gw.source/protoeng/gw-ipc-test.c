#include <unistd.h>
#include "ipc.h"
#include "base.h"

#define GTAG "GW.IPC.TEST"
#include "log.h"

static void _gw_ipc_test_stdin_fn(evutil_socket_t fd, short flag, void *data) {
	gw_ipc_t *ipc = (gw_ipc_t *)data;
	char membuf[1024] = {0};

	if(flag & EV_READ) {
		char *ptr = NULL;
		read(fd, membuf, 1024);
		ptr = membuf;
		if(ptr != NULL) {
			gw_ipc_sendmsg(ipc, PROTOIPC, (unsigned char *)ptr, strlen(ptr));
		}
	}
}

static void _ipccb(char *from, char *membuf, void *data) {
	RAW("%s, %s\n", from, membuf);
}

int main(int argc, char *argv[]) {
	struct event_base *base = NULL;
	struct event *e = NULL;
	gw_ipc_t *ipc = NULL;
	
	evutil_socket_t fd = (evutil_socket_t)fileno(stdin);

	base = event_base_new();
	ipc = gw_ipc_new(base);
	gw_ipc_setup(ipc, GWAPPIPC, _ipccb, NULL);

	RAW("ipc is available = %d\n", gw_ipc_is_available("gw.ipc.proto"));
	e = event_new(base, fd, EV_READ|EV_PERSIST, _gw_ipc_test_stdin_fn, ipc);
	event_add(e, NULL);

	event_base_dispatch(base);

	gw_ipc_free(ipc);
	event_base_free(base);

	return 0;
}

