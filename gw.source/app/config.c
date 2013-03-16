#include "config.h"
#include "session.h"

typedef struct {
	gw_config_t base;
	gw_session_t *s;
} gw_config_instance_t;

gw_config_t *gw_config_new(gw_session_t *s) {
	gw_config_instance_t *r = NULL;
	gw_config_t *cfg = NULL;

	r = calloc(1, sizeof(gw_config_instance_t));

	cfg = (gw_config_t *)r;
	cfg->blist = gw_blist_new();

	cfg->s = s;

	return cfg;
}

void gw_config_free(gw_config_t *cfg) {
	gw_config_instance_t *r = (gw_config_instance_t *)cfg;

	if(cfg) {
		gw_blist_free(cfg->blist);
		free(cfg);
	}
}

int gw_config_load(gw_config_t *cfg) {
	gw_config_instance_t *r = (gw_config_instance_t *)cfg;
}

int gw_config_sync(gw_config_t *cfg) {
	gw_config_instance_t *r = (gw_config_instance_t *)cfg;
}

int gw_config_save(gw_config_t *cfg) {
	gw_config_instance_t *r = (gw_config_instance_t *)cfg;
}
