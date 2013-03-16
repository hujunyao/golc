#include "protoeng.h"
#include "base.h"
#include "utils.h"

static pem_array_t g_pem_array[] = {
	{"log", NULL},
	{"zcmd", NULL}
};

static void proto_eng_free_module(proto_eng_t *e) {
}

static void _proto_eng_read(char *from, char *membuf, void *data) {
	proto_eng_t *e = (proto_eng_t *)data;
	proto_eng_module_t *ptr = NULL;
	pem_xmlbuf_type_t type = PEM_XMLBUF_IN;
	int i = 0, dx = 1, j;
	int ret = PEM_STATUS_CONTINUE;
	int size = sizeof(g_pem_array)/sizeof(pem_array_t);

	DD("from %s, membuf = %s\n", from, membuf);
	if(strcmp(from, ZNETIPC) == 0) {
		/**zcmd buffer from znet*/

		i = size - 1 , dx = -1;
		type = PEM_XMLBUF_OUT;
		gw_ipc_sendmsg(e->ipc, GWAPPIPC, membuf, strlen(membuf));
#if 0
		e->odoc = zcmd2xml(e->ipc, membuf);
		if(e->odoc == NULL) {
			EE("create xml(%s) doc from ZNET failed! \n", membuf);
		}
#endif
		return;
	} else if(strcmp(from, GWAPPIPC) == 0) {
		/**xml buffer from app*/
		e->idoc = (xmlDocPtr)xmlbuf_new_doc_from_buffer(membuf);
		if(e->idoc == NULL) {
			EE("create xml(%s) doc from APP failed! \n", membuf);
		}
	}

	for(j = i, ptr = g_pem_array[j].pem; ptr != NULL && ret == PEM_STATUS_CONTINUE; j=j+dx, ptr=g_pem_array[j].pem) {

		if(type == PEM_XMLBUF_OUT && j < 0) break;
		if(type == PEM_XMLBUF_IN  && j >=size) break;

		if(ptr->pem_preparse) {
			ret = ptr->pem_preparse(ptr, type);
		}
	}/**end of module prepare*/
	if(ret == PEM_STATUS_FAILED) {
		EE("preparse failed \n");
	}

	for(j = i, ptr=g_pem_array[j].pem; ptr != NULL && ret == PEM_STATUS_CONTINUE; j=j+dx, ptr=g_pem_array[j].pem) {

		if(type == PEM_XMLBUF_OUT && j < 0) break;
		if(type == PEM_XMLBUF_IN  && j >=size) break;

		if(ptr->pem_doparse) {
			ret = ptr->pem_doparse(ptr, type);
		}
	}/**end of module doparse*/
	if(ret == PEM_STATUS_FAILED) {
		EE("doparse failed \n");
	}

	for(j = i, ptr=g_pem_array[j].pem; ptr != NULL && ret == PEM_STATUS_CONTINUE; j=j+dx, ptr=g_pem_array[j].pem) {
		if(type == PEM_XMLBUF_OUT && j < 0) break;
		if(type == PEM_XMLBUF_IN  && j >=size) break;

			DD("enter %s->pem_postparse = %x\n", ptr->name, ptr->pem_postparse);
		if(ptr->pem_postparse) {
			ret = ptr->pem_postparse(ptr, type);
		}
	}/**end of module postparse*/
	if(ret == PEM_STATUS_FAILED) {
		EE("postparse failed \n");
	}
}

static int proto_eng_init(proto_eng_t *e) {
	proto_eng_module_t *log = NULL;
	int idx = 0;

	g_pem_array[idx++].pem = (proto_eng_module_t *)pem_log_new(e);
	g_pem_array[idx++].pem = (proto_eng_module_t *)pem_zcmd_new(e);
}

static void proto_eng_uninit(proto_eng_t *e) {
	if(e != NULL) {
		proto_eng_free_module(e);
	}
}

proto_eng_t* proto_eng_new(struct event_base *base) {
	proto_eng_t *e = NULL;

	e = calloc(1, sizeof(proto_eng_t));
	e->ipc = gw_ipc_new(base);
	e->base = base;

	gw_ipc_setup(e->ipc, PROTOIPC, _proto_eng_read, e);
	proto_eng_init(e);
	return e;
}

void  proto_eng_free(proto_eng_t *e) {
	if(e != NULL) {
		proto_eng_uninit(e);
		if(e->ipc) {
			gw_ipc_free(e->ipc);
		}
		free(e);
	}
}

int proto_eng_exec(proto_eng_t *e) {

	if(e->base) {
		event_base_dispatch(e->base);
	}
}
