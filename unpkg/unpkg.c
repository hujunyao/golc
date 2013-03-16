#include <sys/types.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <sys/mman.h>
#include <fcntl.h>
#endif

#include "bzlib.h"
#include "unpkg.h"
#include "md5.h"

#define MAX_BUF_SIZE 1024
#define MAX_PATH 260

#define MAGIC_ID "CMS"
/*
#ifdef _WIN32
	#define _SLASH '\\'
#else
	#define _SLASH '/'
#endif
*/
#define GET_DWORD(ptr) \
	*(ptr+3)<<24 | *(ptr+2)<<16 | *(ptr+1)<<8 | *ptr\

#define GET_WORD(ptr)\
    *(ptr+1)<<8 | *ptr \
    
typedef struct pkg_hdr_t {
	char szName[32];
	char szVersion[32];
	char szAuthor[32];
	char szCopyRight[256];
} PKG_HDR;

struct unpkg_t{
	int level;
	char szDirectory[MAX_PATH];
	/*mmap memory from bzip file*/
	void *mem;
	unsigned char *ptr;

#ifdef _WIN32
	HANDLE hMap;
#else
	int fd;
#endif

	bz_stream stream;
	PKG_HDR *hdr;
};

static int _pkg_chk(UNPKG *pkg) {
	int n;
	MD5_CTX context;
	unsigned char digest[16];
	
#ifdef MD5_CHECK
	n = pkg->ptr - (unsigned char *)pkg->mem - sizeof(digest);
	/*check md5 digest*/
	MD5Init(&context);

	MD5Update(&context, (unsigned char *)pkg->mem, n);
	
	MD5Final(digest, &context);

	pkg->ptr -= 16;
	if(memcmp(digest, pkg->ptr, 16) != 0) {
		printf("MD5 check failure\n");
		return RET_FAILURE;
	}
#endif
	/*check MAGIC_ID*/
	n = strlen(MAGIC_ID);
	pkg->ptr -= n;

	if(strncmp(pkg->ptr, MAGIC_ID, n) != 0)
		return RET_FAILURE;

	n = sizeof(PKG_HDR);
	/**/
	pkg->ptr -= n;
	pkg->hdr = (PKG_HDR *)pkg->ptr;

	return RET_SUCCESS;
}

static int _decompress_buffer2file(UNPKG *pkg, int len, FILE *fp) {
	int ret, n;
	char out_buf[MAX_BUF_SIZE];

	if(! pkg) return RET_FAILURE;

	ret = BZ2_bzDecompressInit(&(pkg->stream), 0, 0);
	if(ret != BZ_OK) return RET_FAILURE;

	pkg->stream.next_out = out_buf;
	pkg->stream.avail_out = MAX_BUF_SIZE;
	pkg->stream.next_in = pkg->ptr;
	pkg->stream.avail_in = len;

	while(1) {
		ret = BZ2_bzDecompress(&(pkg->stream));
		switch(ret) {
		case BZ_OK:
		case BZ_STREAM_END:
			len = MAX_BUF_SIZE - pkg->stream.avail_out;
			if(len <= 0) break;

			n = fwrite(out_buf, 1, len, fp);
			if(n < len) {
				BZ2_bzDecompressEnd(&(pkg->stream));
				return RET_FAILURE;
			}
/*
			if(n < len) 
				printf("write to decompressed file fail!\n");
			else
				printf("[W1]BUF SIZE = %d\n", n);*/
			break;

		case BZ_SEQUENCE_ERROR:
#ifdef _CONSOLE
			printf("[E]:BZ2_bzDecompress\tBZ_SEQUENCE_ERROR\n");
#endif
			BZ2_bzDecompressEnd(&(pkg->stream));
			return RET_FAILURE;

		default: return RET_FAILURE;
		}

		if(ret == BZ_STREAM_END) {
#ifdef _CONSOLE
			printf("total_in = %d\t total_out = %d\n", pkg->stream.total_in_lo32, pkg->stream.total_out_lo32);
#endif
			BZ2_bzDecompressEnd(&(pkg->stream));
			return RET_SUCCESS;
		}/*if(ret == BZ_STREAM_END)*/

		pkg->stream.avail_out = MAX_BUF_SIZE;
		pkg->stream.next_out = out_buf;
	}/*while(1)*/

	return RET_FAILURE;
}

static int _unpkg_proc_file(UNPKG *pkg) {
	int len, ret;
	FILE *fp = NULL;

	pkg->ptr -= BITS_OFFSET;
	len = GET_DWORD(pkg->ptr);
	
	pkg->ptr -= len;

	fp = fopen(pkg->szDirectory, "wb");
	if(fp) {
		ret = _decompress_buffer2file(pkg, len, fp);
		fclose(fp);

		if(RET_FAILURE == ret) {
#ifdef _CONSOLE
			printf("[E]:decompress %s failure\n", pkg->szDirectory);
#endif
			return RET_FAILURE;
		}
		return RET_SUCCESS;
	}

#ifdef _CONSOLE
	printf("[E]:open %s failure\n", pkg->szDirectory);
#endif

	return RET_FAILURE;
}

#ifdef _WIN32/*win32 platform*/
UNPKG *unpkg_create(char *szArchive, char *szDirectory) {
	UNPKG *pkg = NULL;
	DWORD nSizeLow, nSizeHigh;

	pkg = malloc(sizeof(UNPKG));
	if(pkg) {
		HANDLE hFile;
		struct stat info;

		memset(pkg, 0, sizeof(UNPKG));
		
		if(stat(szDirectory, &info) < 0) {
			free(pkg);
#ifdef _CONSOLE
			printf("[E]: %s isn't exist\n", szDirectory);
#endif
			return NULL;
		}

		if(! (info.st_mode&S_IFDIR)) {
			free(pkg);
#ifdef _CONSOLE
			printf("[E]: %s isn't a direcory\n", szDirectory);
#endif
			return NULL;
		}

		strcpy(pkg->szDirectory, szDirectory);

		hFile = CreateFile(szArchive, GENERIC_READ, 0,NULL, 
					OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if(hFile != INVALID_HANDLE_VALUE) {
			nSizeLow = GetFileSize(hFile, &nSizeHigh);
			pkg->hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY,
						0, 0, NULL);
			if(pkg->hMap) {
				pkg->mem = MapViewOfFile(pkg->hMap, FILE_MAP_READ, 0, 0, 0);
				if(pkg->mem) {
					pkg->ptr = (unsigned char *)pkg->mem + nSizeLow;
				} else {
#ifdef _CONSOLE 
					printf("[E]MapViewOfFileEx failure\n");
#endif
					CloseHandle(pkg->hMap);
					CloseHandle(hFile);
					free(pkg);
					return NULL;
				}
			}
		}
		CloseHandle(hFile);
		
		if(_pkg_chk(pkg) == RET_FAILURE) {
			free(pkg); return NULL;
		}

		return pkg;
	}
	return NULL;
}

#else/*linux platform*/
UNPKG *unpkg_create(char *szArchive, char *szDirectory) {
	UNPKG *pkg = NULL;

	pkg = (UNPKG *)malloc(sizeof(UNPKG));
	if(pkg) {
		struct stat info;

		memset(pkg, 0, sizeof(UNPKG));
		
		if(stat(szDirectory, &info) < 0) {
			free(pkg);
#ifdef _CONSOLE
			printf("[E]: %s  isn't exist\n", szDirectory);
#endif
			return NULL;
		}

		if(! S_ISDIR(info.st_mode)) {
			free(pkg);
#ifdef _CONSOLE
			printf("[E]: %s isn't a direcory\n", szDirectory);
#endif
			return NULL;
		}

		strcpy(pkg->szDirectory, szDirectory);
		pkg->fd = open(szArchive, O_RDONLY);

		if(pkg->fd >= 0) {
			memset(&info, 0, sizeof(struct stat));
			fstat(pkg->fd, &info);
			pkg->mem = mmap(NULL, info.st_size, PROT_READ, MAP_PRIVATE, pkg->fd, 0);
			if(pkg->mem) {
				pkg->ptr = (unsigned char *)pkg->mem + info.st_size;
			} else { 
				close(pkg->fd);
				free(pkg);
#ifdef _CONSOLE
				printf("[E]mmap failure\n");
#endif
				return NULL;
			}
		} else {
#ifdef _CONSOLE
			printf("open %s failure\n", szArchive);
#endif
		}
		
		if(_pkg_chk(pkg) == RET_FAILURE) {
#ifdef _CONSOLE
			printf("_pkg_chk failure\n");
#endif
			free(pkg);
			return NULL;
		}

		return pkg;
	}
	return NULL;
}
#endif

#ifdef _WIN32
void unpkg_destroy(UNPKG *pkg) {
	if(pkg) {
		if(pkg->mem)
			UnmapViewOfFile(pkg->mem);
		if(pkg->hMap)
			CloseHandle(pkg->hMap);
		
		free(pkg);
	}
}

#else
void unpkg_destroy(UNPKG *pkg) {
	if(pkg) {
		struct stat info;

		if(pkg->mem) {
			fstat(pkg->fd, &info);
			munmap(pkg->mem, info.st_size);
		}

		if(pkg->fd)
			close(pkg->fd);
		
		free(pkg);
	}
}
#endif

int unpkg_run(UNPKG *pkg) {
	int len, level, i, n;
	struct stat info;

	if(! pkg) return RET_FAILURE;

	do {
		/*get the file/directory level*/
		pkg->ptr --;
		level = *(pkg->ptr);
		/*get the file/directory name length*/
		pkg->ptr --;
		len = *(pkg->ptr); 

		if(len > 0x80) {
			/*if directory , clear flag & get the current name length*/
			len = CLRDIRFLAG(*(pkg->ptr));
			if(level <= pkg->level) {
				n = pkg->level - level;
				for(i=strlen(pkg->szDirectory); i >= 0; i--)
#ifdef _WIN32
					if(pkg->szDirectory[i] == '\\') {
#else
					if(pkg->szDirectory[i] == '/') {
#endif
						pkg->szDirectory[i] = '\0';
						n --;
						if(n < 0) break;
					}
			}/*if(level <= pkg->level)*/
			pkg->level = level;

			pkg->ptr -= len;

#ifdef _WIN32
			strcat(pkg->szDirectory, "\\");
#else
			strcat(pkg->szDirectory, "/");
#endif
			strncat(pkg->szDirectory, pkg->ptr, len);
			if(stat(pkg->szDirectory, &info) < 0) {
#ifdef _WIN32
				mkdir(pkg->szDirectory);
#else
				mkdir(pkg->szDirectory, S_IRWXU|S_IFDIR|S_IXUSR);
#endif

#ifdef _CONSOLE
				printf("[E]:stat %s failure\n", pkg->szDirectory);
#endif
			}

		} else {
			if(level <= pkg->level) {
				n = pkg->level - level;
				for(i=strlen(pkg->szDirectory); i >= 0; i--)
#ifdef _WIN32
					if(pkg->szDirectory[i] == '\\') {
#else
					if(pkg->szDirectory[i] == '/') {
#endif
						pkg->szDirectory[i] = '\0';
						n --;
						if(n < 0) break;
					}
			}/*if(level <= pkg->level)*/
			pkg->level = level;

			pkg->ptr -= len;

#ifdef _WIN32
			strcat(pkg->szDirectory, "\\");
#else
			strcat(pkg->szDirectory, "/");
#endif
			strncat(pkg->szDirectory, pkg->ptr, len);
#ifdef _CONSOLE
			printf("[F]:%s \n", pkg->szDirectory);
#endif
			pkg->ptr -= sizeof(int);
			n = GET_DWORD(pkg->ptr);
			
			_unpkg_proc_file(pkg);
#ifdef _WIN32
			chmod(pkg->szDirectory, n);
#else
			chmod(pkg->szDirectory, n);
#endif
		}/*else */
				
	} while(pkg->ptr > pkg->mem);

	return RET_SUCCESS;
}

#ifdef MD5_CHECK
#ifdef _WIN32
int pkg_md5_chk(char *szArchive) {
	HANDLE hFile, hMap;
	void *mem;
	DWORD nSizeLow, nSizeHigh;

	hFile = CreateFile(szArchive, GENERIC_READ, 0,NULL, 
					OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile != INVALID_HANDLE_VALUE) {
		nSizeLow = GetFileSize(hFile, &nSizeHigh);
		hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY,
						0, 0, NULL);
		if(hMap) {
			mem = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
			if(mem) {
				MD5_CTX context;
				unsigned char digest[16];
				/*check md5 digest*/
				MD5Init(&context);
	
				nSizeLow -= 16;
				MD5Update(&context, (unsigned char *)mem, nSizeLow);
	
				MD5Final(digest, &context);
				if(memcmp(digest, (unsigned char *)mem+nSizeLow, 16)==0) {		
					CloseHandle(hMap);
					CloseHandle(hFile);
					return RET_SUCCESS;
				} else {
					CloseHandle(hMap);
					CloseHandle(hFile);
					return RET_FAILURE;
				}
			}
		}

		CloseHandle(hFile);
	}

	return RET_FAILURE;
}
#else
int pkg_md5_chk(char *szArchive) {
	void *mem;
	int fd, n;
	struct stat info;

	fd = open(szArchive, O_RDONLY);

	if(fd >= 0) {
		memset(&info, 0, sizeof(struct stat));
		fstat(fd, &info);
		mem = mmap(NULL, info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		if(mem) {
			MD5_CTX context;
			unsigned char digest[16];
			/*check md5 digest*/
			MD5Init(&context);
			n = info.st_size - 16;
			MD5Update(&context, (unsigned char *)mem, n);
			MD5Final(digest, &context);

			if(memcmp(digest, mem+n, 16)==0) {
				munmap(mem, info.st_size);
				close(fd);
				return RET_SUCCESS;
			} else {
				munmap(mem, info.st_size);
				close(fd);
				return RET_FAILURE;
			}
		}

		close(fd);
	}

	return RET_FAILURE;
}
#endif
#endif/*MD5_CHECK*/
