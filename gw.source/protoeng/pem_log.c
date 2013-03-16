#include <libxml/parser.h>
#include <libxml/xpath.h>
#include "protoeng.h"

typedef struct {
	proto_eng_module_t base;

	int fd;
} pem_log_t;

PEM_INIT(log) (proto_eng_module_t *pem) {
	pem_log_t *obj = (pem_log_t *)pem;

	obj->fd = (int)stdout;
}

PEM_UNINIT(log) (proto_eng_module_t *pem) {
}

PEM_PREPARSE(log) (proto_eng_module_t *pem, pem_xmlbuf_type_t type) {
}

PEM_DOPARSE(log) (proto_eng_module_t *pem, pem_xmlbuf_type_t type) {
}

static void node_foreach_dump(xmlNodePtr node, void *data) {
	pem_log_t *obj = (pem_log_t *)data;
	char *id = NULL, *cmd = NULL, *arg = NULL;

	id  = (char *)xmlbuf_get_prop(node, "CtrlId");
	cmd = (char *)xmlbuf_get_prop(node, "Command");
	arg = (char *)xmlbuf_get_prop(node, "Argu");

	DD("LOG:(id = %s, cmd = %s, arg = %s)\n", id, cmd, arg);
}

PEM_POSTPARSE(log) (proto_eng_module_t *pem, pem_xmlbuf_type_t type) {
	xmlDocPtr doc = NULL;

	if(pem != NULL) {
		if(type == PEM_XMLBUF_IN) {
			xmlNodeSetPtr ctrl = NULL;
			proto_eng_t *e = pem->e;

			doc = e->idoc;
			ctrl = (xmlNodeSetPtr)xmlbuf_new_xpath(doc, "//Control");
			xmlbuf_nodeset_foreach(ctrl, node_foreach_dump, pem);
		}
	}
	return PEM_STATUS_CONTINUE;
}

proto_eng_module_t* pem_log_new(proto_eng_t *e) {
	pem_log_t *obj = NULL;

  obj = calloc(1, sizeof(pem_log_t));

	PEMOBJ(obj)->e = e;
	PEMOBJ(obj)->name = "log";

	PEMOBJ_SET_PROC(obj, log, init);
	PEMOBJ_SET_PROC(obj, log, uninit);
	//PEMOBJ_SET_PROC(obj, log, preparse);
	//PEMOBJ_SET_PROC(obj, log, doparse);
	PEMOBJ_SET_PROC(obj, log, postparse);
}

void pem_log_free(proto_eng_module_t *log) {
	pem_log_t *obj = (pem_log_t *)log;

	if(obj != NULL) {
		free(obj);
	}
}

