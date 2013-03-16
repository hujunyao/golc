#include <string.h>
#include "utils.h"

#define GTAG " "
#include "log.h"

static void node_foreach_dump(xmlNodePtr node, void *data) {
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
}

static void node_query_foreach(xmlNodePtr node, void *data) {
	char *buf = (char *)data;
	char *devid = NULL, cmd = NULL;
	char tmpbuf[1024] = {0};

	devid = (char *)xmlbuf_get_prop(node, "DeviceId");
	cmd = (char *)xmlbuf_get_prop(node, "QueryName");

	sprintf(tmpbuf, "%s,%s,%s,%s;", devid, "", cmd, "");
	buf = strcat(buf, tmpbuf);
}

void xml2zcmd(xmlDocPtr doc, char *zcmdbuf) {
	xmlNodeSetPtr ctrl = NULL;

	ctrl = (xmlNodeSetPtr)xmlbuf_new_xpath(doc, "//Control");
	if(ctrl->nodeNr > 0) {
		xmlbuf_nodeset_foreach(ctrl, node_foreach_dump, zcmdbuf);
	}

	ctrl = (xmlNodeSetPtr)xmlbuf_new_xpath(doc, "//Query");
	if(ctrl->nodeNr > 0) {
		xmlbuf_nodeset_foreach(ctrl, node_query_foreach, zcmdbuf);
	}

	DD("zcmdbuf = %s\n", zcmdbuf);
}
