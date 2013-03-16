#include "session.h"

static int _session_status_cb(gw_session_t *s, gw_session_status_t sta) {

	return 0;
}

int main(int argc, char *argv[]) {
	struct event_base *base = NULL;
	gw_session_t *s = NULL;

	base = event_base_new();
	if(!base) {
		EE("libevent initialized failed\n");
		return -1;
	}

	s = gw_session_new(base, _session_status_cb);

	if(s != NULL) {
		gw_session_startup(s);
		gw_session_exec(s);

		gw_session_shutdown(s);
		gw_session_free(s);
	}

	event_base_free(base);
	return 0;
}
