#include <libxml/parser.h>
#include <libxml/xpath.h>
#include "protoeng.h"
#include "utils.h"

typedef struct {
	proto_eng_module_t base;

	int fd;
} pem_zcmd_t;

PEM_INIT(zcmd) (proto_eng_module_t *pem) {
	pem_zcmd_t *obj = (pem_zcmd_t *)pem;

	obj->fd = (int)stdout;
}

PEM_UNINIT(zcmd) (proto_eng_module_t *pem) {
}

PEM_PREPARSE(zcmd) (proto_eng_module_t *pem, pem_xmlbuf_type_t type) {
}

PEM_DOPARSE(zcmd) (proto_eng_module_t *pem, pem_xmlbuf_type_t type) {
}

PEM_POSTPARSE(zcmd) (proto_eng_module_t *pem, pem_xmlbuf_type_t type) {
	xmlDocPtr doc = NULL;

	if(pem != NULL) {
		if(type == PEM_XMLBUF_IN) {
			proto_eng_t *e = pem->e;
			char *xmlbuf = NULL;
			int size = 0;

			doc = e->idoc;
			//xml2zcmd(doc, zcmdbuf);
			xmlDocDumpMemory(doc, (xmlChar **)&xmlbuf, &size);
			gw_ipc_sendmsg(e->ipc, "gw.ipc.znet", xmlbuf, size);
			printf("send '%s' to znet\n", xmlbuf);
			if(xmlbuf)
				free(xmlbuf);

			return PEM_STATUS_CONTINUE;
		} else if(type == PEM_XMLBUF_OUT) {
		}
	}
}

proto_eng_module_t* pem_zcmd_new(proto_eng_t *e) {
	pem_zcmd_t *obj = NULL;

  obj = calloc(1, sizeof(pem_zcmd_t));

	PEMOBJ(obj)->e = e;
	PEMOBJ(obj)->name = "zcmd";

	PEMOBJ_SET_PROC(obj, zcmd, init);
	PEMOBJ_SET_PROC(obj, zcmd, uninit);
	//PEMOBJ_SET_PROC(obj, log, preparse);
	//PEMOBJ_SET_PROC(obj, log, doparse);
	PEMOBJ_SET_PROC(obj, zcmd, postparse);
}

void pem_zcmd_free(proto_eng_module_t *zcmd) {
	pem_zcmd_t *obj = (pem_zcmd_t *)zcmd;

	if(obj != NULL) {
		free(obj);
	}
}
