#include "base.h"

baseinfo_t *baseinfo_new() {
	return calloc(1, sizeof(baseinfo_t));
}

void baseinfo_free(baseinfo_t *bi) {
	if(bi != NULL)
		free(bi);
}
