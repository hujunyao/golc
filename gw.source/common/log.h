#ifndef __GWAPP_LOG_H__
#define __GWAPP_LOG_H__
#include<stdio.h>

#ifndef GTAG
#error  GTAG must define before include gw-log.h
#endif

#define DD(...) fprintf(stderr, GTAG":[D] "__VA_ARGS__)
#define WW(...) fprintf(stderr, GTAG":[W] "__VA_ARGS__)
#define EE(...) fprintf(stderr, GTAG":[E] "__VA_ARGS__)

#define RAW(...) fprintf(stderr, __VA_ARGS__)

#endif
