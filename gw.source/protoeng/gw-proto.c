#include <event.h>
#include "protoeng.h"

int main(int argc, char *argv[]) {
	proto_eng_t *eng = NULL;
	struct event_base *base = NULL;
	int ret = -1;

	base = event_base_new();
	if(!base) {
		EE("libevent initialized failed\n");
		return ret;
	}

	eng = proto_eng_new(base);
	if(eng) {
		ret = proto_eng_exec(eng);
		proto_eng_free(eng);
	}

	event_base_free(base);

	return ret;
}
