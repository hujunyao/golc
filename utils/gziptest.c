/**
 *gcc -o gztest gziptest.c -lz
 */
#include <zlib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define OSIZE 1024

int decompress_buf(char *ibuf, int ilen, char **obuf, int *olen) {
	int ret;
	z_stream s = {0};

	s.zalloc = Z_NULL;
	s.zfree = Z_NULL;
	s.opaque = Z_NULL;
	s.avail_in = 0;
	s.next_in = Z_NULL;
	ret = inflateInit2(&s, 47);
	if(ret != Z_OK) {
		printf("inflateInit2 failed\n");
		return Z_OK;
	}

	*olen = 0;
	*obuf = NULL;
	s.avail_in = ilen;
	s.next_in = ibuf;

	do {
		char oblock[OSIZE] = {0};
		int used_size = 0;

		s.avail_out = OSIZE;
		s.next_out = oblock;
		ret = inflate(&s, Z_NO_FLUSH);
		switch(ret) {
		case Z_NEED_DICT:
			ret = Z_DATA_ERROR;
		case Z_DATA_ERROR:
		case Z_MEM_ERROR:
			printf("gz.inflate ERROR %d\n", ret);
			inflateEnd(&s);
			return ret;
		}
		used_size = OSIZE - s.avail_out;
		*obuf = realloc(*obuf, *olen + used_size);
		if (*obuf == NULL) {
			printf("obuf.realloc failed\n");
			inflateEnd(&s);
			return Z_ERRNO;
		}
		memset(*obuf + *olen, 0, used_size);
		memcpy(*obuf + *olen, oblock, used_size);
		*olen += used_size;
	} while(s.avail_out == 0);

	return Z_OK;
}

int main(int argc, char *argv[]) {
	char *obuf;
	int olen, ilen;
	FILE *fp = NULL;
	char ibuf[1024] = {0};

	fp = fopen(argv[1], "r");
	if(fp == NULL) {
		printf("open gzfile %s failed\n", argv[1]);
		return -1;
	}

	ilen  = fread(ibuf, 1, 1024, fp);
	fclose(fp);
	decompress_buf(ibuf, ilen, &obuf, &olen);
	printf("output.len:%d\n%s", olen, obuf);
	free(obuf);
	return 0;
}
