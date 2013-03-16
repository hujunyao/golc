#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "znet.h"
#include "base.h"

#define BRATE  B115200

//static unsigned char *gw_znet_zcmd2zbuf(gw_znet_t *znet, char *zcmd, int *size);
//static char *gw_znet_zbuf2zcmd(gw_znet_t *znet, unsigned char *zbuf);
static void gw_znet_dozcmd(gw_znet_t *znet, char *xmlbuf, int *size);
static void dump_zbuf(unsigned char *zbuf, int size);
static char mydevid[32] ={0};

char *xmlfmt = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><SLUP ProtoVer = \"1.0\" ><Trans TransId = \"9\" TransType = \"ZNET_STATUS_RPT \"> <Report RptType = \"Update\"><HACInfo AccessIp = \"*.*.*.*\" Port = \"9783\" > <ZNet PanFreq =\"\" AccessCode = \"\" > <ZDev DevId = \"%s\"> <Status StaType = \"%s\" StaName = \"%s\" StaValue = \"%d\"/> </ZDev> </ZNet> </HACInfo> </Report> </Trans></SLUP>";

unsigned char DEV120_EUI[] = {0x00, 0x80, 0xe1, 0x02, 0x00, 0x21, 0xa5, 0xf6};
unsigned char DEV121_EUI[] = {0x00, 0x80, 0xe1, 0x02, 0x00, 0x1b, 0xe2, 0xff};
unsigned char DEV122_EUI[] = {0x00, 0x80, 0xe1, 0x02, 0x00, 0x1b, 0xe2, 0x6d};

static char *gw_znet_parse_zcmd(unsigned char *zcmd, int nsize) {
	char *xmlbuf = NULL;
	unsigned char *eui = zcmd + 2;

	xmlbuf = calloc(1024, sizeof(char));

	memset(mydevid, 0, 32);
	if(memcmp(eui, DEV120_EUI, 8) == 0) {
		strcpy(mydevid, "120");
	} else if(memcmp(eui, DEV121_EUI, 8) == 0) {
		strcpy(mydevid, "121");
	} else if(memcmp(eui, DEV122_EUI, 8) == 0) {
		strcpy(mydevid, "122");
	}

	if(*zcmd == ZCMD_REPORT_TEMP) {
		unsigned char sta = *(zcmd+1);
		unsigned char temp = *(zcmd+10);

		DD("temp = %d\n", temp);
		sprintf(xmlbuf, xmlfmt, mydevid, "Event", "Temperature", temp);
		DD("znet.outxml: %s\n", xmlbuf);
	} else if(*zcmd == ZCMD_REPORT_DUMP) {
		unsigned char sta = *(zcmd + 1);
		unsigned char dump = *(zcmd + 10);

		DD("dump = %d\n", dump);
		sprintf(xmlbuf, xmlfmt, mydevid, "Event", "Coulometer", dump);
		DD("znet.outxml: %s\n", xmlbuf);
	} else if(*zcmd == ZCMD_POWERON) {
		unsigned char sta = *(zcmd + 1);
		unsigned char flag = *(zcmd + 10);

		DD("flag = %d\n", flag);
		sprintf(xmlbuf, xmlfmt, mydevid, "Operation", "ON", sta);
		DD("znet.outxml: %s\n", xmlbuf);
	} else if(*zcmd == ZCMD_POWEROFF) {
		unsigned char sta = *(zcmd + 1);
		unsigned char flag = *(zcmd + 10);

		DD("flag = %d\n", flag);
		sprintf(xmlbuf, xmlfmt, mydevid, "Operation", "OFF", sta);
		DD("znet.outxml: %s\n", xmlbuf);
	}

	return xmlbuf;
}

static void _gw_znet_uart_fn(evutil_socket_t fd, short flag, void *data) {
	gw_znet_t *znet = (gw_znet_t *)data;

	if(flag & EV_READ) { /**read notification from UART*/
		unsigned char zcmdbuf[2048] = {0};
		int nsize = 0;

		nsize = read(fd, zcmdbuf, 2048);
		//printf("uart.read.nsize = %d\n", nsize);
		dump_zbuf(zcmdbuf, nsize);
		if(*zcmdbuf >= 0x80) {
			//DD("ZCMDBUF: %x,%x,%x,%x,%x,%x,%x,%x\n", zcmdbuf[0], zcmdbuf[1], zcmdbuf[2], zcmdbuf[3], zcmdbuf[4], zcmdbuf[5], zcmdbuf[6], zcmdbuf[7]);
			//char *outbuf = gw_znet_zbuf2zcmd(znet, zcmdbuf);
			char *xmlbuf = gw_znet_parse_zcmd(zcmdbuf, nsize);
			gw_ipc_sendmsg(znet->ipc, PROTOIPC, xmlbuf, strlen(xmlbuf));
			free(xmlbuf);
		}
	}
}

static void dump_zbuf(unsigned char *zbuf, int size) {
	int i;

	for(i=0; i<size + 2; i++) {
		printf("%x,", *zbuf);
		zbuf ++;
	}
	printf("\n");
}

static void _gw_znet_read(char *from, char *membuf, void *data) {
	gw_znet_t *znet = (gw_znet_t *)data;
	int size = 0;

	if(strcmp(from, PROTOIPC) == 0) {
		/**process zcmd from proto engine*/
		DD("zcmdbuf from proto = %s\n", membuf);
		size = strlen(membuf);
		if(size > 0) {
			gw_znet_dozcmd(znet, membuf, &size);
			//dump_zbuf(zbuf, size);
			//write(znet->uart, zbuf, size);
			//free(zbuf);
		}
	}
}

static void uart_attr_setup(int fd) {
	struct termios newtio;
	tcgetattr(fd, &newtio);
	bzero(&newtio, sizeof(newtio));

	newtio.c_cflag |= (CLOCAL|CREAD);
	newtio.c_cflag &= ~PARODD;
	newtio.c_cflag &= ~CSIZE;
	//newtio.c_oflag |= OPOST;
	newtio.c_oflag &= ~OPOST;                          /*Output*/
	newtio.c_iflag &= ~(IXON|IXOFF|IXANY);
	newtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); /*Input*/

	newtio.c_iflag &= ~(INLCR | ICRNL | IGNCR);
	newtio.c_oflag &= ~(ONLCR | OCRNL);

	newtio.c_cflag |= CS8;
	newtio.c_cflag &= ~PARENB;
	newtio.c_iflag &= ~INPCK;  /* Enable parity checking */
	newtio.c_cflag &= ~CSTOPB;

	tcflush(fd, TCIFLUSH);
  newtio.c_cc[VTIME] = 150; /* Set timeout 15 seconds*/
  newtio.c_cc[VMIN] = 0;    /* Update the options and do it NOW */

	cfsetispeed(&newtio, BRATE);
	cfsetospeed(&newtio, BRATE);
	tcsetattr(fd, TCSANOW, &newtio);
}

gw_znet_t *gw_znet_new(struct event_base *base) {
	gw_znet_t *znet = NULL;
	struct event *e = NULL;
	int ret;

	znet = calloc(1, sizeof(gw_znet_t));
	znet->ipc = gw_ipc_new(base);
	znet->base = base;

	znet->uart = open(UARTDEV, O_RDWR | O_NOCTTY | O_NDELAY);
	if(znet->uart < 0) {
		EE("open UARTDEV(%s) failed\n", UARTDEV);
	} else {
		e = event_new(base, znet->uart, EV_READ|EV_PERSIST, _gw_znet_uart_fn, znet);
		event_add(e, NULL);
		uart_attr_setup(znet->uart);
	}

	ret = sqlite3_open_v2(ZNETDEVDB, &znet->db, SQLITE_OPEN_NOMUTEX, NULL);
	if(ret != SQLITE_OK) {
		EE("open ZNET database(%s) failed\n", ZNETDEVDB);
	}

	gw_ipc_setup(znet->ipc, ZNETIPC, _gw_znet_read, znet);

	return znet;
}

void gw_znet_free(gw_znet_t *znet) {
	if(znet != NULL) {
		if(znet->db) {
			sqlite3_close(znet->db);
		}

		if(znet->ipc)
			gw_ipc_free(znet->ipc);

		if(znet->uart)
			close(znet->uart);

		xmlbuf_cleanup();
	}
}

int gw_znet_exec(gw_znet_t *znet) {
	if(znet->base) {
		event_base_dispatch(znet->base);
	}
}

#define SQLCMD_GET_IRDA "select ircode from 'gw.irda' where cmd='%s'"
static int gw_get_irdabuf(char *cmd, unsigned char *zbuf) {
	sqlite3 *cmddb = NULL;
	sqlite3_stmt *stmt = NULL;
	char sqlbuf[1024] = {0};
	int ret;
	const char *unused;
	char path[1024] = {0};
	unsigned char irbuf[512] = {0};

	snprintf(path, 1024, "/home/key.ircode/%s.bin", cmd);
	DD("get ircode %s\n", path);
	int fd = open(path, O_RDONLY);
	if(fd > 0) {
		int nsize;
		nsize = read(fd, zbuf+1, 512);
		*zbuf = nsize >> 1;
		close(fd);
		return nsize;
	}
	printf("open %s failed\n", path);
#if 0
	ret = sqlite3_open("/home/bin/ircode.db", &cmddb);
	if(ret != SQLITE_OK) {
		EE("open ircode.db failed (%s)\n", sqlite3_errmsg(cmddb));
	}

	snprintf(sqlbuf, 1024, SQLCMD_GET_IRDA, cmd);
	ret = sqlite3_prepare(cmddb, sqlbuf, strlen(sqlbuf), &stmt, &unused);
	if(ret == SQLITE_OK) {
		do {
			ret = sqlite3_step(stmt);
			if(ret == SQLITE_ROW) {
				const void *data = sqlite3_column_blob(stmt, 0);
				int size = sqlite3_column_bytes(stmt, 0);
				DD("irda code size = %d\n", size);
				*zbuf = 67, zbuf++;
				memcpy(zbuf, irCodeBuffer, 67*2);
				return 67*2;
			}
		} while(ret == SQLITE_ROW);
		sqlite3_finalize(stmt);
		sqlite3_close(cmddb);
	}

#endif
	return -1;
}

static void node_foreach_dump(xmlNodePtr node, void *data) {
#if 0
	char *buf = (char *)data;
	char *id = NULL, *cmd = NULL, *arg = NULL, *devid = NULL;
	xmlNodePtr parent = NULL;
	char tmpbuf[1024] = {0};
	char tmpcmd[32] = {0};

	id  = (char *)xmlbuf_get_prop(node, "CtrlId");
	cmd = (char *)xmlbuf_get_prop(node, "Command");
	arg = (char *)xmlbuf_get_prop(node, "Argu");

	parent = node->parent;
	devid = (char *)xmlbuf_get_prop(parent, "DeviceId");
	if(strcmp(cmd, "CHANNEL") == 0) {
		tmpcmd[0] = 'K';
		tmpcmd[1] = arg[0];
		cmd = tmpcmd;
	}
	sprintf(tmpbuf, "%s,%s,%s,%s;", devid, id, cmd,arg);
	buf = strcat(buf, tmpbuf);
#endif
	unsigned char *zbuf = NULL, *buf = NULL;
	char *cmd = NULL, *arg = NULL, *devid = NULL;
	gw_znet_t *znet = (gw_znet_t *)data;
	xmlNodePtr parent = NULL;
	int size = 0;
	char tmpcmd[32] = {0};

	buf = zbuf = calloc(1024, sizeof(unsigned char));
	*zbuf = ZCMD_SEND_IR_CODE, zbuf++;

	parent = node->parent;
	devid = (char *)xmlbuf_get_prop(parent, "DeviceId");

	if(strcmp(devid, "121") == 0) {
	*zbuf=0x00, zbuf++;
	*zbuf=0x80, zbuf++;
	*zbuf=0xe1, zbuf++;
	*zbuf=0x02, zbuf++;
	*zbuf=0x00, zbuf++;
	*zbuf=0x1b, zbuf++;
	*zbuf=0xe2, zbuf++;
	*zbuf=0xff, zbuf++;
	} else if(strcmp(devid, "120") == 0) {
	*zbuf=0x00, zbuf++;
	*zbuf=0x80, zbuf++;
	*zbuf=0xe1, zbuf++;
	*zbuf=0x02, zbuf++;
	*zbuf=0x00, zbuf++;
	*zbuf=0x21, zbuf++;
	*zbuf=0xa5, zbuf++;
	*zbuf=0xf6, zbuf++;
	} else if(strcmp(devid, "122") == 0) {
	*zbuf=0x00, zbuf++;
	*zbuf=0x80, zbuf++;
	*zbuf=0xe1, zbuf++;
	*zbuf=0x02, zbuf++;
	*zbuf=0x00, zbuf++;
	*zbuf=0x1b, zbuf++;
	*zbuf=0xe2, zbuf++;
	*zbuf=0x6d, zbuf++;
	}

	cmd = (char *)xmlbuf_get_prop(node, "Command");
	arg = (char *)xmlbuf_get_prop(node, "Argu");

	if(strcmp(cmd, "CHANNEL") == 0) {
		tmpcmd[0] = 'K';
		tmpcmd[1] = arg[0];
		cmd = tmpcmd;
	}

	if(strcmp(cmd, "ON") == 0) {
		*buf = ZCMD_POWERON;
		size = 1 + 8 + 1;
	} else if(strcmp(cmd, "OFF") == 0) {
		*buf = ZCMD_POWEROFF;
		size = 1 + 8 + 1;
	} else {
		size = gw_get_irdabuf(cmd, zbuf) + 1 + 8 + 1;
	}

	DD("write buf = %x, size = %d to uart\n", buf, size);
	write(znet->uart, buf, size);
	free(buf);
}

static void node_query_foreach(xmlNodePtr node, void *data) {
	unsigned char *zbuf = NULL, *buf = NULL;
	gw_znet_t *znet = (gw_znet_t *)data;
	char *devid = NULL, *cmd = NULL;
	int size = 0;

	buf = zbuf = calloc(1024, sizeof(unsigned char));
	zbuf++;

	devid = (char *)xmlbuf_get_prop(node, "DeviceId");
	if(strcmp(devid, "121") == 0) {
	*zbuf=0x00, zbuf++;
	*zbuf=0x80, zbuf++;
	*zbuf=0xe1, zbuf++;
	*zbuf=0x02, zbuf++;
	*zbuf=0x00, zbuf++;
	*zbuf=0x1b, zbuf++;
	*zbuf=0xe2, zbuf++;
	*zbuf=0xff, zbuf++;
	} else if(strcmp(devid, "120") == 0) {
	*zbuf=0x00, zbuf++;
	*zbuf=0x80, zbuf++;
	*zbuf=0xe1, zbuf++;
	*zbuf=0x02, zbuf++;
	*zbuf=0x00, zbuf++;
	*zbuf=0x21, zbuf++;
	*zbuf=0xa5, zbuf++;
	*zbuf=0xf6, zbuf++;
	} else if(strcmp(devid, "122") == 0) {
	*zbuf=0x00, zbuf++;
	*zbuf=0x80, zbuf++;
	*zbuf=0xe1, zbuf++;
	*zbuf=0x02, zbuf++;
	*zbuf=0x00, zbuf++;
	*zbuf=0x1b, zbuf++;
	*zbuf=0xe2, zbuf++;
	*zbuf=0x6d, zbuf++;
	}

	cmd = (char *)xmlbuf_get_prop(node, "QueryName");
	if(strcmp(cmd, "Temperature") == 0) {
		DD("Query.ZCMD_REPORT_TEMP\n");
		*buf = ZCMD_REPORT_TEMP;
		size = 1 + 8 + 1;

		memset(mydevid, 0, 32);
		strcpy(mydevid,devid);
	} else if(strcmp(cmd, "Coulometer") == 0) {
		DD("Query.ZCMD_REPORT_DUMP\n");
		*buf = ZCMD_REPORT_DUMP;
		size = 1 + 8 + 1;

		memset(mydevid, 0, 32);
		strcpy(mydevid,devid);
	}

	DD("query.write buf = %x, size = %d to uart\n", buf, size);
	write(znet->uart, buf, size);
	free(buf);
}

void gw_znet_dozcmd(gw_znet_t *znet, char *xmlbuf, int *size) {
	xmlNodeSetPtr ctrl = NULL;
	xmlDocPtr doc = xmlbuf_new_doc_from_buffer(xmlbuf);

	ctrl = (xmlNodeSetPtr)xmlbuf_new_xpath(doc, "//Control");
	if(ctrl->nodeNr > 0) {
		xmlbuf_nodeset_foreach(ctrl, node_foreach_dump, znet);
	}

	ctrl = (xmlNodeSetPtr)xmlbuf_new_xpath(doc, "//Query");
	if(ctrl->nodeNr > 0) {
		xmlbuf_nodeset_foreach(ctrl, node_query_foreach, znet);
	}

	xmlbuf_free_doc(doc);
#if 0
	unsigned char *buf = NULL, irdabuf[1024], *zbuf = NULL;
	char *ptr = zcmd, *p = NULL;
	char *cmd = NULL, *arg = NULL;
	int irdatype = 0;
	*zbuf = ZCMD_SEND_IR_CODE, zbuf++;

	//zbuf += 8; //skip EUISIZE
	*zbuf=0x00, zbuf++;
	*zbuf=0x80, zbuf++;
	*zbuf=0xe1, zbuf++;
	*zbuf=0x02, zbuf++;
	*zbuf=0x00, zbuf++;
	*zbuf=0x1b, zbuf++;
	*zbuf=0xe2, zbuf++;
	*zbuf=0xff, zbuf++;

	//command
	ptr = p;
	p = strchr(ptr, ',');
	*p = '\0', p++;
	cmd = ptr;

	ptr = p;
	p = strchr(ptr, ';');
	*p = '\0', p++;
	arg = ptr;
	
	DD("cmd = %s, arg = %s\n", cmd, arg);
	if(strcmp(cmd, "Temperature") == 0) {
		*buf = ZCMD_REPORT_TEMP;
		*size = 1 + 8 + 1;
	} else {
		*size = gw_get_irdabuf(cmd, zbuf) + 1 + 8 + 1;
	}

	return buf;
#endif
}
