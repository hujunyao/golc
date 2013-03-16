#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <dirent.h>

#include "microhttpd.h"

#define DEBUG(...) fprintf(stdout, "[III] "__VA_ARGS__)
#define EF fprintf(stdout, "enter %s\n", __func__)
#define LF fprintf(stdout, "leave %s\n", __func__)

#define TRUE 1
#define FALSE 0

#define BLKSIZE 2048
#define DNASIZE 128
#define POST_BUFSIZE 2048

#define APPEND_BUF(buf) \
  _ptr = buf; \
  priv->len += strlen(_ptr); \
  if(priv->len > priv->size) { \
    priv->size = priv->len+BLKSIZE; \
    priv->page = realloc(priv->page, priv->size); \
    if(NULL == priv->page) { \
      priv->len = strlen(mem_err_page); \
      strncpy(priv->page, mem_err_page, priv->len); \
    } \
  } \
  strcat(priv->page, _ptr)

#define PORT 80
#define UPLOAD_FORM "<form action='%s' method='post' enctype='multipart/form-data'><table><tr><td class='f'><input type='submit' value='Upload'/></td><td><input name='file' type='file'/></td></tr></table></form>"
#define WEBFS_CSS "<style type='text/css'> a, a:active {text-decoration: none; color: blue;} a:visited {color: #48468F;} a:hover, a:focus {text-decoration: underline; color: red;} body {background-color: #F5F5F5;} h2 {margin-bottom: 12px;} table {margin-left: 12px;} th,td { font: monospace; text-align: left;} th { font: 120% font-weight: bold; padding-right: 14px; padding-bottom: 3px;} td {font: 100% ;padding-right: 14px;} td.s, th.s {text-align: right; padding-right:36px;} td.f {padding-right:36px} td.d {font: 100% font-weight bold;} div.list { font: 120% monospace; background-color: white; border-top: 1px solid #646464; border-bottom: 1px solid #646464; padding-top: 10px; padding-bottom: 14px;} div.foot { font: 120% monospace; color: #787878; padding-top: 4px;} </style>"

#define AHC_ARG const char *url, const char *method, const char *version, const char *udata, unsigned int *udata_size

const char *mem_err_page = "<html><head><title>WebFS</title></head><body>An internal server error(memory alloc failed) has occured.</body></html>";
const char *file_err_page = "<html><head><title>WebFS</title></head><body>An internal server error(open file failed) has occured.</body></html>";
const char *file_exist_err_page = "<html><head><title>WebFS</title></head><body>An internal server error(file already exist) has occured.</body></html>";
const char *no404_err_page  = "<html><head><title>WebFS</title></head><body><h2>404 - Not Found</h2></body></html>";

const char* upload_complete_page = "<?xml version='1.0' encoding='utf-8'?>\n<!DOCTYPE html PUBLIC '-//W3C//DTD XHTML 1.1//EN' 'http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd'>\n<html xmlns='http://www.w3.org/1999/xhtml' xml:lang='en'><body>Upload <b>%s</b> to %s has been completed.<a href='%s'>Go Back</a></body></html>";

static const char hex_chars[] = "0123456789abcdef";

typedef struct {
  char *name;
  struct stat info;
} dir_node_t;

typedef struct {
  dir_node_t *dnp;
  int size, len;
} dir_node_array_t;

typedef struct {
  char *path;
  const char *url;
  char *page;
  int len, size;
  dir_node_array_t dna;
  struct MHD_PostProcessor *postproc; 
  FILE *postfp;
  int respcode;
} webfs_t;

typedef enum {
	ENCODING_REL_URI,
	ENCODING_REL_URI_PART,
	ENCODING_HTML,
	ENCODING_MINIMAL_XML,
	ENCODING_HEX,
	ENCODING_HTTP_HEADER,
	ENCODING_UNSET
} encoding_type_t;

const char encoded_chars_rel_uri_part[] = {
	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  00 -  0F control chars */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  10 -  1F */
	1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1,  /*  20 -  2F space " # $ % & ' + , / */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,  /*  30 -  3F : ; = ? @ < > */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  40 -  4F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  50 -  5F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  60 -  6F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,  /*  70 -  7F DEL */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  80 -  8F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  90 -  9F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  A0 -  AF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  B0 -  BF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  C0 -  CF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  D0 -  DF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  E0 -  EF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  F0 -  FF */
};

const char encoded_chars_rel_uri[] = {
	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  00 -  0F control chars */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  10 -  1F */
	1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0,  /*  20 -  2F space " # $ % & ' + , / */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,  /*  30 -  3F : ; = ? @ < > */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  40 -  4F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  50 -  5F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  60 -  6F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,  /*  70 -  7F DEL */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  80 -  8F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  90 -  9F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  A0 -  AF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  B0 -  BF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  C0 -  CF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  D0 -  DF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  E0 -  EF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  F0 -  FF */
};

const char encoded_chars_html[] = {
	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  00 -  0F control chars */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  10 -  1F */
	0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  20 -  2F & */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,  /*  30 -  3F < > */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  40 -  4F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  50 -  5F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  60 -  6F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,  /*  70 -  7F DEL */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  80 -  8F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  90 -  9F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  A0 -  AF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  B0 -  BF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  C0 -  CF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  D0 -  DF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  E0 -  EF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  F0 -  FF */
};

const char encoded_chars_minimal_xml[] = {
	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  00 -  0F control chars */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  10 -  1F */
	0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  20 -  2F & */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,  /*  30 -  3F < > */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  40 -  4F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  50 -  5F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  60 -  6F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,  /*  70 -  7F DEL */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  80 -  8F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  90 -  9F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  A0 -  AF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  B0 -  BF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  C0 -  CF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  D0 -  DF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  E0 -  EF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  F0 -  FF */
};

const char encoded_chars_hex[] = {
	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  00 -  0F control chars */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  10 -  1F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  20 -  2F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  30 -  3F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  40 -  4F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  50 -  5F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  60 -  6F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  70 -  7F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  80 -  8F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  90 -  9F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  A0 -  AF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  B0 -  BF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  C0 -  CF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  D0 -  DF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  E0 -  EF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  F0 -  FF */
};

const char encoded_chars_http_header[] = {
	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,  /*  00 -  0F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  10 -  1F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  20 -  2F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  30 -  3F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  40 -  4F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  50 -  5F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  60 -  6F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  70 -  7F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  80 -  8F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  90 -  9F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  A0 -  AF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  B0 -  BF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  C0 -  CF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  D0 -  DF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  E0 -  EF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  F0 -  FF */
};

char *string_encoded(const char *s, size_t s_len, encoding_type_t encoding) {
	unsigned char *ds, *d;
	size_t d_len, ndx;
	const char *map = NULL;
  char *newbuf = NULL;

	if (s_len == 0) return NULL;

	switch(encoding) {
	case ENCODING_REL_URI:
		map = encoded_chars_rel_uri;
		break;
	case ENCODING_REL_URI_PART:
		map = encoded_chars_rel_uri_part;
		break;
	case ENCODING_HTML:
		map = encoded_chars_html;
		break;
	case ENCODING_MINIMAL_XML:
		map = encoded_chars_minimal_xml;
		break;
	case ENCODING_HEX:
		map = encoded_chars_hex;
		break;
	case ENCODING_HTTP_HEADER:
		map = encoded_chars_http_header;
		break;
	case ENCODING_UNSET:
		break;
	}

  if(map == NULL) return NULL;

	/* count to-be-encoded-characters */
	for (ds = (unsigned char *)s, d_len = 0, ndx = 0; ndx < s_len; ds++, ndx++) {
		if (map[*ds]) {
			switch(encoding) {
			case ENCODING_REL_URI:
			case ENCODING_REL_URI_PART:
				d_len += 3;
				break;
			case ENCODING_HTML:
			case ENCODING_MINIMAL_XML:
				d_len += 6;
				break;
			case ENCODING_HTTP_HEADER:
			case ENCODING_HEX:
				d_len += 2;
				break;
			case ENCODING_UNSET:
				break;
			}
		} else {
			d_len ++;
		}
	}
  newbuf = calloc(1, d_len*sizeof(char *));

	for (ds = (unsigned char *)s, d = (unsigned char *)newbuf, d_len = 0, ndx = 0; ndx < s_len; ds++, ndx++) {
		if (map[*ds]) {
			switch(encoding) {
			case ENCODING_REL_URI:
			case ENCODING_REL_URI_PART:
				d[d_len++] = '%';
				d[d_len++] = hex_chars[((*ds) >> 4) & 0x0F];
				d[d_len++] = hex_chars[(*ds) & 0x0F];
				break;
			case ENCODING_HTML:
			case ENCODING_MINIMAL_XML:
				d[d_len++] = '&';
				d[d_len++] = '#';
				d[d_len++] = 'x';
				d[d_len++] = hex_chars[((*ds) >> 4) & 0x0F];
				d[d_len++] = hex_chars[(*ds) & 0x0F];
				d[d_len++] = ';';
				break;
			case ENCODING_HEX:
				d[d_len++] = hex_chars[((*ds) >> 4) & 0x0F];
				d[d_len++] = hex_chars[(*ds) & 0x0F];
				break;
			case ENCODING_HTTP_HEADER:
				d[d_len++] = *ds;
				d[d_len++] = '\t';
				break;
			case ENCODING_UNSET:
				break;
			}
		} else {
			d[d_len++] = *ds;
		}
	}

	return newbuf;
}

static int file_read_cb (void *cls, size_t pos, char *buf, int max)
{
  FILE *file = cls;

  fseek (file, pos, SEEK_SET);
  return fread (buf, 1, max, file);
}

char hex2int(unsigned char hex) {
  hex = hex - '0';
  if (hex > 9) {
    hex = (hex + '0' - 1) | 0x20;
    hex = hex - 'a' + 11;
  }
  if (hex > 15)
    hex = 0xFF;

  return hex;
}

static int urldecode(char *dst, const char *src, int is_query) {
  unsigned char high, low;

  while ((*src) != '\0') {
    if (is_query && *src == '+') {
      *dst = ' ';
    } else if (*src == '%') {
      *dst = '%';

      high = hex2int(*(src + 1));
      if (high != 0xFF) {
        low = hex2int(*(src + 2));
        if (low != 0xFF) {
          high = (high << 4) | low;

          /* map control-characters out */
          if (high < 32 || high == 127) high = '_';

          *dst = high;
          src += 2;
        }
      }
    } else {
      *dst = *src;
    }

    dst++;
    src++;
  }

  *dst = '\0';

  return 0;
}

static char *gen_link_path(const char *url, int append) {
  char *lpath = NULL;
  const char *ptr = NULL, *prev = NULL;

  lpath = calloc(1, BLKSIZE);
  ptr = prev = url+1;

  strcpy(lpath, "<a href='/'>/</a>");

  while(*ptr != '\0') {
    if(*ptr == '/') {
      strcat(lpath, "<a href='");
      strncat(lpath, url, ptr-url+1);
      strcat(lpath, "'>");
      strncat(lpath, prev, ptr-prev+1);
      strcat(lpath, "</a>");
      prev = ptr+1;
    }
    ptr++;
  }

  if (append) {
    strcat(lpath, "<a href='");
    strcat(lpath, url);
    strcat(lpath, "/'>");
    strcat(lpath, prev);
    strcat(lpath, "</a>");
  }
  DEBUG("link path = %s\n", lpath);
  return lpath;
}

static int dn_cmp(const void *n1, const void *n2) {
  dir_node_t *dn1, *dn2;
  int val1, val2;

  dn1 = (dir_node_t *)n1, dn2 = (dir_node_t *)n2;

  val1 = S_ISREG(dn1->info.st_mode), val2 = S_ISREG(dn2->info.st_mode);

  if(val1 == val2) {
    return strcmp(dn1->name, dn2->name);
  }
  return val1 - val2;
}

static void fmt_size(unsigned int size, char *sstr, int len) {

  if(size < 1024*1024) {
    snprintf(sstr, len, "%0.1f K", size/1024.0);
  } else if (size < 1024*1024*1024) {
    snprintf(sstr, len, "%0.1f M", size/(1024*1024.0));
  } else {
    snprintf(sstr, len, "%0.1f G", size/(1024.0*1024*1024));
  }
}

static void gen_dir_page(struct MHD_Connection *conn, void *data, const char *url, int append) {
  webfs_t *priv = (webfs_t *)data;
  char *_ptr = NULL, *lpath = NULL, *newurl = NULL;
  DIR *dp = NULL;
  struct dirent *dent;
  dir_node_t *dn = NULL;
  int idx;
  char sizestr[64];

  lpath = gen_link_path(url, append);
  dp = opendir(priv->path);

  APPEND_BUF("<?xml version='1.0' encoding='utf-8'?>\n<!DOCTYPE html PUBLIC '-//W3C//DTD XHTML 1.1//EN' 'http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd'>\n<html xmlns='http://www.w3.org/1999/xhtml' xml:lang='en'><head><title>Index of ");
  APPEND_BUF((char *)url);
  APPEND_BUF("</title>"WEBFS_CSS"</head>");
  APPEND_BUF("<body><h2>Index of ");
  APPEND_BUF(lpath);
  APPEND_BUF("</h2><div class='list'><table cellpadding='0' cellspacing='0'>");
  APPEND_BUF("<thead><tr><th class='n'>Name</th><th class='s'>Size</th><th class='m'>Last Modified</th></tr></thead>");
  while((dent = readdir(dp)) != NULL) {
    struct stat info;

    if(dent->d_name[0] == '.') {
      continue;
    }
    if(priv->dna.len >= priv->dna.size) {
      priv->dna.size += DNASIZE;
      priv->dna.dnp = realloc(priv->dna.dnp, priv->size*sizeof(dir_node_t));
    }
    priv->dna.dnp[priv->dna.len].name = strdup(dent->d_name);
    snprintf(lpath, BLKSIZE, "%s%s", priv->path, dent->d_name);
    stat(lpath, &(priv->dna.dnp[priv->dna.len].info));
    priv->dna.len ++;
    //DEBUG("path = %s, len = %d\n", lpath, priv->dna.len);
  }
  qsort(priv->dna.dnp, priv->dna.len, sizeof(dir_node_t), dn_cmp);
  for(idx = 0; idx<priv->dna.len; idx++) {
    char tmp_path[1024] = {0};
    struct stat *pinfo = &(priv->dna.dnp[idx].info);

    if(append) {
      char *ptr = strrchr(url, '/');
      snprintf(tmp_path, 1024, "%s/%s", ptr+1, priv->dna.dnp[idx].name);
    } else
      strcpy(tmp_path, priv->dna.dnp[idx].name);

    newurl = string_encoded(tmp_path, strlen(tmp_path), ENCODING_REL_URI_PART);
    if(S_ISDIR(pinfo->st_mode)) {
      snprintf(lpath, BLKSIZE, "<tr><td class='d'><a href='%s/'>%s</a>/</td><td class='s'></td><td class='m'>%s</td></tr>",
        newurl, priv->dna.dnp[idx].name, ctime(&(pinfo->st_mtime)));
    } else {
      fmt_size(pinfo->st_size, sizestr, 64);
      snprintf(lpath, BLKSIZE, "<tr><td class='d'><a href='%s'>%s</a></td><td class='s'>%s</td><td class='m'>%s</td></tr>",
        newurl, priv->dna.dnp[idx].name, sizestr, ctime(&(pinfo->st_mtime)));
    }
    free(newurl);
    APPEND_BUF(lpath);
  }

  APPEND_BUF("</table></div>");
  snprintf(lpath, BLKSIZE, UPLOAD_FORM, url);
  APPEND_BUF(lpath);
  APPEND_BUF("<div class='foot'>Powered by Linux (WebFS - 1.0 beta)</div></body></html>");

  closedir(dp);
  free(lpath);
}

static int gen_file_page(struct MHD_Connection *conn, void *data) {
  struct MHD_Response *resp;
  webfs_t *priv = (webfs_t *)data;
  FILE *fp = NULL;
  struct stat info;
  int ret = MHD_YES;

  fp = fopen(priv->path, "r");
  if(fp != NULL) {
    fstat(fileno(fp), &info);
  
    if(info.st_size > priv->size) {
      resp = MHD_create_response_from_callback (info.st_size, 64 * 1024, &file_read_cb, fp, (MHD_ContentReaderFreeCallback) &fclose);
      ret = MHD_queue_response (conn, MHD_HTTP_OK, resp);
      MHD_destroy_response (resp);
      return MHD_YES;
    } else {
      priv->len = fread(priv->page, 1, info.st_size, fp);
    }
  }

  if(fp == NULL || ret == MHD_NO) {
    strcpy(priv->page, file_err_page);
    priv->len = strlen(file_err_page);
  }
  if(fp) fclose(fp);

  return MHD_NO ;
}

#define ITERPOST_ARG enum MHD_ValueKind kind, const char *key, const char *filename, const char *type,const char *encoding

static int iterate_post (void *data, ITERPOST_ARG, const char *buf, size_t off, size_t size) {
  webfs_t *priv = (webfs_t *)data;
  char fpath[2048] = {0};
  char *newfilename, *newpath;

  if(0 != strcmp (key, "file")) return MHD_NO;

  if(NULL == priv->postfp) {
    snprintf(fpath, 2048, "%s%s", priv->path, filename);
    DEBUG("new file = %s\n", fpath);
    priv->postfp = fopen(fpath, "r");
    if(priv->postfp) {
      strcpy(priv->page, file_exist_err_page);
      priv->len = strlen(file_exist_err_page);
      priv->respcode = MHD_HTTP_FORBIDDEN;
      return MHD_NO;
    }
    priv->postfp = fopen(fpath, "ab");
    if(NULL == priv->postfp) {
      strcpy(priv->page, file_err_page);
      priv->len = strlen(file_err_page);
      return MHD_NO;
    }
  }
    
  if(size > 0) {
    if(!fwrite(buf, size, sizeof(char), priv->postfp))
      return MHD_NO;
  }

  newfilename = string_encoded(filename, strlen(filename), ENCODING_MINIMAL_XML);
  newpath = string_encoded(priv->path, strlen(priv->path), ENCODING_MINIMAL_XML);
  snprintf(priv->page, priv->size, upload_complete_page, newfilename, newpath, priv->url);
  priv->len = strlen(priv->page);
  priv->respcode = MHD_HTTP_OK;
  free(newpath);
  free(newfilename);

  return MHD_YES;
}
int answer_conn(void *cls, struct MHD_Connection *conn, AHC_ARG, void **data) {
  struct MHD_Response *resp;
  int ret;
  char *body = NULL;
  webfs_t *priv = NULL;
  struct stat info;
  int urllen = strlen(url);

  DEBUG("\n\n");
  DEBUG("url = %s, method = %s, version = %s, upload.data = %x, upload.size = %d\n", url , method, version, udata, *udata_size);

  if(NULL == *data) {
    char *currdir = NULL;
    int len = 0;

    currdir = (char *)get_current_dir_name();
    len = strlen(currdir) + urllen + 2;

    priv = (webfs_t *) calloc (1, sizeof (webfs_t));
    priv->path = calloc(len, sizeof(char));
    priv->page = calloc(BLKSIZE, sizeof(char));
    priv->dna.size = DNASIZE;
    priv->dna.len  = 0;
    priv->respcode = MHD_HTTP_OK;
    priv->dna.dnp  = calloc(DNASIZE, sizeof(dir_node_t));
    priv->size = BLKSIZE;
    priv->len = 0;
    priv->url = url;

    if(strcmp(method, "POST") == 0)
      priv->postproc = MHD_create_post_processor(conn, POST_BUFSIZE, iterate_post, (void*)priv);   
    else
      priv->postproc = NULL;

    //urldecode((char *)url, (char *)url, 0);
    snprintf(priv->path, len, "%s%s", currdir, url);
    if(currdir) free(currdir);

    DEBUG("current path = %s, url = %s\n", priv->path, url);
    *data = priv;
  } else {
    priv = *data;
  }

  if(priv->postproc) {
     /**HTTP POST*/
     if(0 != *udata_size) { 
       MHD_post_process(priv->postproc, udata, *udata_size);
       *udata_size = 0;
       return MHD_YES;
     } 
  } else {
    /**HTTP GET */
    ret = stat(priv->path, &info);
    if(ret == 0) {
      if(S_ISDIR(info.st_mode)) {
        if(*(url+urllen-1) != '/') {
          strcat(priv->path, "/");
          gen_dir_page(conn, *data, url, TRUE);
        } else {
          gen_dir_page(conn, *data, url, FALSE);
        }
      } else if(S_ISREG(info.st_mode)) {
        if(ret = gen_file_page(conn, *data)) 
          return MHD_YES;
      } 
    } else {
      strcpy(priv->page, no404_err_page);
      priv->len = strlen(no404_err_page);
    }
  }

  //DEBUG("page = %s, len = %d\n", priv->page, priv->len);
  resp= MHD_create_response_from_data (priv->len, (void *) priv->page, MHD_NO, MHD_NO);
  ret = MHD_queue_response (conn, priv->respcode, resp);
  MHD_destroy_response (resp);

  return MHD_YES;
}

void request_completed (void *cls, struct MHD_Connection *conn, void **data, enum MHD_RequestTerminationCode toe) {
  webfs_t *priv = (webfs_t *) *data;

  if (NULL == priv)
    return;

  DEBUG("priv = %x, len = %d\n", priv, priv->len);

  if(priv->postproc) {
    MHD_destroy_post_processor(priv->postproc);
    if(priv->postfp)
      fclose(priv->postfp);
  }
  if(priv->page) {
    free(priv->page);
  }
  if(priv->path) {
    free(priv->path);
  }
  if(priv->dna.dnp) {
    int i;

    for(i=0; i<priv->dna.len; i++) {
      free(priv->dna.dnp[i].name);
    }
    free(priv->dna.dnp);
  }
  free (priv);
  priv= NULL;
}

int main(int argc, char *argv[]) {
  struct MHD_Daemon *daemon;
  int port = PORT;

  if(argc > 1) {
    port = strtol(argv[1], (char **)NULL, 10);
    if(port == LONG_MIN || port == LONG_MAX)
      port = PORT;
  }

  //if (0 != fork()) exit(0);

  DEBUG("set port to %d\n", port);
  chdir("/home/user");
  daemon = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION, port, NULL, NULL, &answer_conn, NULL,
                             MHD_OPTION_NOTIFY_COMPLETED, request_completed, NULL, 
                             MHD_OPTION_END);

  if (NULL == daemon) {
    DEBUG("Web file server startup failure!\n");
    return 1;
  }

	sleep(UINT_MAX);
  MHD_stop_daemon (daemon);

  return 0;
}
