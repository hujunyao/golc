#ifndef __UTILS_H__
#define __UTILS_H__
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "ipc.h"

void xml2zcmd(xmlDocPtr doc, char *zcmdbuf);
xmlDocPtr zcmd2xml(gw_ipc_t *ipc, unsigned char *zcmdbuf);

#endif
