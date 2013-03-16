#include <event.h>
#include "znet.h"

int main(int argc, char *argv[]) {
	gw_znet_t *znet = NULL;
	struct event_base *base = NULL;
	int ret = -1;

	base = event_base_new();
	if(!base) {
		EE("libevent initialized failed\n");
		return ret;
	}

	znet = gw_znet_new(base);
	if(znet) {
		ret = gw_znet_exec(znet);
		gw_znet_free(znet);
	}

	event_base_free(base);

	return ret;
}
