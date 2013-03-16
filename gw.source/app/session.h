#include "session.h"

static void _app_session_ipc_read(char *from, char *membuf, void *data) {
	gw_session_t *s = (gw_session_t *)data;
}

static void _app_session_inet_read(evutil_socket_t fd, short flag, void *data) {
}

gw_session_t* gw_session_new(struct event_base *base, gw_session_status_callback_t sscb) {
	gw_session_t *s = NULL;
	struct event *e = NULL;

	s = calloc(1, sizeof(gw_session_t));
	s->ipc = gw_ipc_new(base);
	gw_ipc_setup(s->ipc, "gw.ipc.app", _app_session_ipc_read, s);
	gw_config_load(&(s->cfg));
	s->sscb = sscb;

	s->base = base;
	s->fd = gw_udp_new();
	e = event_new(base, s->fd,  EV_READ|EV_PERSIST, _app_session_inet_read, s);
	event_add(e, NULL);

	return s;
}

void gw_session_free(gw_session_t *s) {
	if(s != NULL) {

		gw_ipc_free(s->ipc);
		if(s->fd)
			close(s->fd);
		free(s);
	}
}

int gw_session_startup(gw_session_t *s) {
	if(s->sscb) {
		s->sscb(s, GW_SESSION_STATUS_IDLE);
	}
}

int gw_session_exec(gw_session_t *s) {
	if(s->base) {
		event_base_dispatch(e->base);
	}
}

int gw_session_shutdown(gw_session_t *s) {
	if(s->sscb)
		s->sscb(s, GW_SESSION_STATUS_DISCONNECT);
	if(s->cfg.blist.sta == GW_BUDDY_STATUS_UPDATE) {
		gw_config_sync(s, &s->cfg);
		gw_config_save(&s->cfg);
	}
}

