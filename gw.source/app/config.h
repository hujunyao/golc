#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "blist.h"

#define GW_CONFIG_PATH "/etc/gw.cfg"

typedef struct {
	int id;
	char passwd[128];

	gw_blist_t *blist;
} gw_config_t;

gw_config_t *gw_config_new(gw_session_t *s);
void gw_config_free(gw_config_t *cfg);

int gw_config_load(gw_config_t *cfg);
int gw_config_sync(gw_config_t *cfg);
int gw_config_save(gw_config_t *cfg);

#endif
