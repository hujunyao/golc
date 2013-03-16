#ifndef SRCTOOL_BASE_H
#define SRCTOOL_BASE_H
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

typedef enum {
	CMT_STA_NULL,
	CMT_STA_START,
	CMT_STA_END
} cmt_sta_t;

#define SET_FLAG(v, flag) v |= flag
#define CHK_FLAG(v, flag) (v & flag)
#define CLR_FLAG(v, flag) v &= ~flag;

typedef struct {
	int pure_line, pure_char;
} baseinfo_t;

baseinfo_t *baseinfo_new();
void baseinfo_free(baseinfo_t *bi);


#endif
