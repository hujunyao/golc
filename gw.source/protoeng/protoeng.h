#ifndef __PROTOENG_H__
#define __PROTOENG_H__

#include<libxml/parser.h>
#include<event.h>
#include "ipc.h"
#include "utils.h"

#define GTAG "PROTO"
#include "log.h"

#define ZCMDSIZE 2048
typedef enum {
	PEM_XMLBUF_IN,
	PEM_XMLBUF_OUT
} pem_xmlbuf_type_t;

typedef enum {
	PEM_STATUS_CONTINUE,
	PEM_STATUS_FAILED,
	PEM_STATUS_SUCCESSS
} pem_status_t;

#define PEM_INIT(modulename) static int pem_##modulename##_init 
#define PEM_UNINIT(modulename) static void pem_##modulename##_uninit
#define PEM_PREPARSE(modulename) static int pem_##modulename##_preparse
#define PEM_DOPARSE(modulename) static int pem_##modulename##_doparse
#define PEM_POSTPARSE(modulename) static int pem_##modulename##_postparse

#define PEMOBJ(obj) ((proto_eng_module_t *)obj)
#define PEMOBJ_SET_PROC(obj, modulename, func) ((proto_eng_module_t *)obj)->pem_##func = pem_##modulename##_##func

typedef void (* node_foreach_t)(xmlNodePtr node, void *data);
typedef struct _proto_eng_module proto_eng_module_t;

typedef struct {
	xmlDocPtr idoc, odoc;

	gw_ipc_t *ipc;
	struct event_base *base;
} proto_eng_t;

struct _proto_eng_module{
	const char *name;
	int (* pem_init)(proto_eng_module_t *m);
	void (* pem_uninit)(proto_eng_module_t *m);

	int (* pem_preparse)(proto_eng_module_t *m, pem_xmlbuf_type_t t);
	int (* pem_doparse)(proto_eng_module_t *m, pem_xmlbuf_type_t t);
	int (* pem_postparse)(proto_eng_module_t *m, pem_xmlbuf_type_t t);

	proto_eng_t *e;
};

typedef struct {
	const char *name;
	proto_eng_module_t* pem;
} pem_array_t;

proto_eng_t* proto_eng_new(struct event_base *base);
void proto_eng_free(proto_eng_t *e);

int proto_eng_exec(proto_eng_t *e);

#endif
