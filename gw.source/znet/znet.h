#ifndef __ZNET_H__
#define __ZNET_H__
#include <event.h>
#include <sqlite3.h>
#include "ipc.h"

#define GTAG "ZNET"
#include "log.h"

#define UARTDEV  "/dev/ttyO1"

#define ZNETDEVDB "/etc/znetdev.db"

typedef enum {
	ZCMD_SLEEP = 0x80,
	ZCMD_WAKE_UP,
	ZCMD_BIND,
	ZCMD_UNBIND,
	ZCMD_POWERON,
	ZCMD_POWEROFF,
	ZCMD_REPORT_DUMP,
	ZCMD_REPORT_TEMP,
	ZCMD_REPORT_HUM,
	ZCMD_SEND_IR_CODE,
	ZCMD_UPDATE,
	ZCMD_NONE
} gw_zcmd_t;

typedef struct {
	gw_ipc_t *ipc;
	struct event_base *base;
  int uart;
	sqlite3 *db;
} gw_znet_t;

gw_znet_t *gw_znet_new(struct event_base *base);
void gw_znet_free(gw_znet_t *znet);

int gw_znet_exec(gw_znet_t *znet);
#endif
