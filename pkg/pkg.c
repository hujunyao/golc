#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
	#include <io.h>
#else
	#include <dirent.h>
#endif

#include "pkg.h"
#include "bzlib.h"


#include "md5.h"
#ifdef MD5_CHECK
#endif

#define MAX_BUF_SIZE 1024
#define MAX_PATH	260
#define MAGIC_ID	"CMS"

typedef struct pkg_hdr_t {
	char szName[32];
	char szVersion[32];
	char szAuthor[32];
	char szCopyRight[256];
} PKG_HDR;

struct pkg_t {
	/*archive file handler*/
	FILE *fd;
	path_file_filter func;
	char szDirectory[MAX_PATH];
	unsigned char level;
	/*pre-compressed file total size*/
	int pre_size;
	/*post-compressed file size*/
	int post_size;
	/*pkg instance current status*/
	int status;

	bz_stream stream;
	PKG_HDR hdr;
#ifdef MD5_CHECK
	MD5_CTX md5;
#endif
};

static int mywrite(PKG *pkg, const void *buffer, size_t size, size_t count, FILE *stream ) {
	int n;

	n = fwrite(buffer, size, count, stream);
	if(n < count)
		printf("[ERROR]:write file failure\n");

	MD5Update(&(pkg->md5), (unsigned char *)buffer, n*size);

	return n;
}

static int _default_path_file_filter(PKG *pkg, char *szfilename, int type) {

	switch(type) {
		case TYPE_DIR:
			if(strcmp(".", szfilename) == 0) return RET_FAILURE;
			if(strcmp("..", szfilename) == 0) return RET_FAILURE;
		default: return RET_SUCCESS;
	}

	return RET_SUCCESS;
}

static int _compress_file_by_bzip2(PKG *pkg, FILE *in_fd, FILE *out_fd) {
	char in_buf[MAX_BUF_SIZE], out_buf[MAX_BUF_SIZE];
	int ret, len, n;

	if(in_fd && out_fd && pkg) {
		memset(out_buf, 0, MAX_BUF_SIZE);
		memset(&(pkg->stream), 0, sizeof(pkg->stream));
		ret = BZ2_bzCompressInit(&(pkg->stream), 5, 0, 30);
		switch(ret) {
			case BZ_OK: break;

			case BZ_MEM_ERROR:
#ifdef _CONSOLE
				printf("[E]:BZ2_bzCompressInit\tBZ_MEM_ERROR\n");
#endif
			default:
				return RET_FAILURE;
		}

		pkg->stream.avail_in = 0;
		pkg->stream.next_out = out_buf;
		pkg->stream.avail_out = MAX_BUF_SIZE;
		
		while(1) {
			if(pkg->stream.avail_in == 0) {
				memset(in_buf, 0, MAX_BUF_SIZE);

				pkg->stream.avail_in = fread(in_buf, 1, MAX_BUF_SIZE, in_fd);
				pkg->stream.next_in = in_buf;

				if(pkg->stream.avail_in <= 0) {
					while(1) {
						ret = BZ2_bzCompress(&(pkg->stream),BZ_FINISH);
						switch(ret) {
						case BZ_FINISH_OK:
						case BZ_STREAM_END:
							len = MAX_BUF_SIZE - pkg->stream.avail_out;
							if(len > 0) {
								mywrite(pkg, out_buf, sizeof(char), len, out_fd);
								/*if(n < len) 
									printf("write to bz2 file fail!\n");
								else
									printf("[W1]BUF SIZE = %d\n", n);*/
							}
							break;

						case BZ_SEQUENCE_ERROR:
#ifdef _CONSOLE
							printf("[E]:BZ2_bzCompress\tBZ_SEQUENCE_ERROR\n"); 
#endif
							return RET_FAILURE;


						default:
#ifdef _CONSOLE
							printf("[E]:BZ2_bzCompress\tUNKNOWN ERROR\n"); 
#endif
							return RET_FAILURE;
						}/*switch(ret)*/

						if(ret == BZ_STREAM_END) {
#ifdef _CONSOLE
							printf("total_in = %d\t total_out = %d\n", pkg->stream.total_in_lo32, pkg->stream.total_out_lo32);
#endif
							/*fwrite(&(stream.total_in_lo32), sizeof(int), 1, out_fd);*/
							pkg->post_size += pkg->stream.total_in_lo32; 
							mywrite(pkg, &(pkg->stream.total_out_lo32), sizeof(int), 1, out_fd);
							BZ2_bzCompressEnd(&(pkg->stream));
							return RET_SUCCESS;
						}/*if(ret == BZ_STREAM_END)*/

						memset(out_buf, 0, MAX_BUF_SIZE);
						pkg->stream.avail_out = MAX_BUF_SIZE;
						pkg->stream.next_out = out_buf;
					}
				}/*if(stream.avail_in <= 0)*/
			}/*if(stream.avail_in == 0)*/

			ret = BZ2_bzCompress(&(pkg->stream), BZ_RUN);
			if(ret != BZ_RUN_OK) return -1;

			if(pkg->stream.avail_out == 0) {
				n = mywrite(pkg, out_buf, 1, MAX_BUF_SIZE, out_fd);
/*
				if(n < MAX_BUF_SIZE)
					printf("write to bz2 file fail!\n");
				else
					printf("[W2]BUF SIZE = %d\n", n);
*/
				pkg->stream.next_out = out_buf;
				pkg->stream.avail_out = MAX_BUF_SIZE;
			}/*if(stream.avail_out == 0)*/
		}
	}/*if(in_fd && out_fd)*/

	return RET_SUCCESS;
}

static int _pkg_proc_file(PKG *pkg, char *szFile) {
	FILE *in_fd = NULL, *out_fd = NULL;
	char filename[MAX_PATH];
	int ret = RET_FAILURE;

	if(pkg && pkg->fd) {
		memset(filename, 0, MAX_PATH);
#ifdef _WIN32
		sprintf(filename, "%s%s", pkg->szDirectory, szFile);
#else
        sprintf(filename, "%s/%s", pkg->szDirectory, szFile);
#endif
		in_fd = fopen(filename, "rb");
		if(in_fd) {
			ret = _compress_file_by_bzip2(pkg, in_fd, pkg->fd);
			fclose(in_fd);
		}
	}

	return ret;
}

#ifdef _WIN32
static int _get_directory_total_size(PKG *pkg, char *szDirectory) {
	struct _finddata_t attr;
	long id;

	int size = 0;
	char _buff[MAX_PATH];

	memset(_buff, 0, MAX_PATH);
	sprintf(_buff, "%s\\*.*", szDirectory); 
	id = _findfirst(_buff, &attr);
	if(id < 0) {
#ifdef _CONSOLE
		printf("[E]:_findfirst @ _get_total_size");
#endif
		return 0;
	}

	do {
		if(attr.attrib == _A_SUBDIR) {
			if(pkg->func(pkg, attr.name, TYPE_DIR) == RET_SUCCESS) {
				memset(_buff, 0, MAX_PATH);
				sprintf(_buff, "%s\\%s", szDirectory, attr.name);
				size += _get_directory_total_size(pkg, _buff);
			}
		} else {
			if(pkg->func(pkg, attr.name, TYPE_FILE) == RET_SUCCESS) {
				struct _stat info;

				memset(_buff, 0, MAX_PATH);
				sprintf(_buff, "%s\\%s", szDirectory, attr.name);

				if(_stat(_buff, &info) >= 0)
					size += info.st_size;
			}
		}
	} while( _findnext(id, &attr) >= 0);

	_findclose(id);
	return size;
}
#else
static int _get_directory_total_size(PKG *pkg, char *szDirectory) {
	struct dirent *_ptr;
	DIR *_dir;
	struct stat info;

	int size = 0;
	char _buff[MAX_PATH];

	_dir = opendir(szDirectory);
	if(! _dir) {
#ifdef _CONSOLE
		printf("[E]:open directory %s failure\n", szDirectory);
#endif
		return RET_FAILURE;
	}

	while( _ptr = readdir(_dir) ) {
		memset(_buff, 0, MAX_PATH);
		sprintf(_buff, "%s/%s", szDirectory, _ptr->d_name);
		stat(_buff, &info);
		
		if(S_ISDIR(info.st_mode)) {
			if(pkg->func(pkg, _ptr->d_name, TYPE_DIR) == RET_SUCCESS)
				size += _get_directory_total_size(pkg, _buff);
		} else {
		    if(pkg->func(pkg, _ptr->d_name, TYPE_FILE) == RET_SUCCESS)
                size += info.st_size;
        }    
	}

	closedir(_dir);

	return size;
}

#endif
#ifdef _WIN32
/*proc directory under win32 platform*/
static int _pkg_proc_directory(PKG *pkg) {
	char _buff[MAX_PATH];
	int len;
	unsigned char n;

	struct _finddata_t attr;
	long id;

	if(pkg) {
		memset(_buff, 0, MAX_PATH);
		sprintf(_buff, "%s*.*", pkg->szDirectory);
		id = _findfirst(_buff, &attr);
		if(id < 0) {
#ifdef _CONSOLE
			printf("[E]:_findfirst @ %s\n", pkg->szDirectory);
#endif
			return RET_FAILURE;
		}

		do {
			memset(_buff, 0, MAX_PATH);
			if(attr.attrib == _A_SUBDIR) {
				if(pkg->func(pkg, attr.name, TYPE_DIR) == RET_SUCCESS) {
					len = strlen(pkg->szDirectory);
					strcat(pkg->szDirectory, attr.name);
					strcat(pkg->szDirectory, "\\");

					pkg->level++;
					_pkg_proc_directory(pkg);
					memset(&(pkg->szDirectory[len]), 0, strlen(attr.name));
					pkg->szDirectory[len] = '\0';

					n = strlen(attr.name);
					/*sprintf(_buff, "%s%d", attr.name, SETDIRFLAG(strlen(attr.name)));*/
					mywrite(pkg, attr.name, 1, n, pkg->fd);
					n = SETDIRFLAG(n);
					mywrite(pkg, &n, 1, 1, pkg->fd);
					mywrite(pkg, &(pkg->level), 1, 1, pkg->fd);
					pkg->level--;
				}
			} else {
				if(pkg->func(pkg, attr.name, TYPE_FILE) == RET_SUCCESS) {
					struct stat info;

					pkg->level++;
#ifdef _CONSOLE
					printf("[F]%s%s\n", pkg->szDirectory, attr.name);
#endif
					if(_pkg_proc_file(pkg, attr.name) == RET_FAILURE) {
#ifdef _CONSOLE
						printf("[F]%s%s\n", pkg->szDirectory, attr.name);
						printf("compress failure!\n");
#endif
						continue;
					}

					sprintf(_buff, "%s%s", pkg->szDirectory, attr.name);
					stat(_buff, &info);
					mywrite(pkg, &(info.st_mode), sizeof(int), 1, pkg->fd);

					n = strlen(attr.name);
					mywrite(pkg, attr.name, 1, n, pkg->fd);
					/*sprintf(_buff, "%s%d", attr.name, strlen(attr.name));*/
					mywrite(pkg, &n, 1, 1, pkg->fd);
					mywrite(pkg, &(pkg->level), 1, 1, pkg->fd);
					pkg->level--;
				}
			}
			memset(&attr, 0, sizeof(struct _finddata_t));
		} while(_findnext(id, &attr) >= 0);

		_findclose(id);
	}
	return RET_SUCCESS;
}
#else
/*proc directory under linux platform*/
static int _pkg_proc_directory(PKG *pkg) {
	char _buff[MAX_PATH];
	int ret, len;
	unsigned char n;

	struct dirent attr, *_ptr;
	DIR *_dir;
	struct stat info;

	if(pkg) {
		_dir = opendir(pkg->szDirectory);
		if(! _dir) {
#ifdef _CONSOLE
			printf("[E]:open directory %s failure\n", pkg->szDirectory);
#endif
			return RET_FAILURE;
		}

		while( _ptr = readdir(_dir) ) {
			memcpy(&attr, _ptr, sizeof(struct dirent));
			memset(_buff, 0, MAX_PATH);
			sprintf(_buff, "%s/%s", pkg->szDirectory, attr.d_name);
			stat(_buff, &info);
			memset(_buff, 0, MAX_PATH);

			if(S_ISDIR(info.st_mode)) {
				if(pkg->func(pkg, attr.d_name, TYPE_DIR) == RET_SUCCESS) {
					len = strlen(pkg->szDirectory);
					strcat(pkg->szDirectory, "/");
					strcat(pkg->szDirectory, attr.d_name);
					pkg->level++;
					_pkg_proc_directory(pkg);
					memset(&(pkg->szDirectory[len]), 0, strlen(attr.d_name));
					pkg->szDirectory[len] = '\0';

					n = strlen(attr.d_name);
					/*sprintf(_buff, "%s%d", attr.name, SETDIRFLAG(strlen(attr.name)));*/
					mywrite(pkg, attr.d_name, 1, n, pkg->fd);
					n = SETDIRFLAG(n);
					mywrite(pkg, &n, sizeof(unsigned char), 1, pkg->fd);
					mywrite(pkg, &(pkg->level), sizeof(unsigned char), 1, pkg->fd);
					pkg->level--;
				}
			} else {
				if(pkg->func(pkg, attr.d_name, TYPE_FILE) == RET_SUCCESS) {
					struct stat info;

					pkg->level++;
#ifdef _CONSOLE
					printf("[F]%s/%s\n", pkg->szDirectory, attr.d_name);
#endif
					if(_pkg_proc_file(pkg, attr.d_name) == RET_FAILURE) {
#ifdef _CONSOLE
						printf("[F]%s%s\n", pkg->szDirectory, attr.d_name);
						printf("compress failure!\n");
#endif
					}

					mywrite(pkg, &(info.st_mode), sizeof(int), 1, pkg->fd);

					n = strlen(attr.d_name);
					mywrite(pkg, attr.d_name, 1, n, pkg->fd);
					/*sprintf(_buff, "%s%d", attr.name, strlen(attr.d_name));*/
					mywrite(pkg, &n, 1, 1, pkg->fd);
					mywrite(pkg, &(pkg->level), 1, 1, pkg->fd);
					pkg->level--;
				}
			}
		}
		closedir(_dir);
	}
	return RET_SUCCESS;
}
#endif

PKG *pkg_create(char *szDirectory, char *szArchive, path_file_filter func) {
	PKG *pkg = NULL;
	struct stat info;
	int ret;


	pkg = malloc(sizeof(PKG));
	memset(pkg, 0, sizeof(PKG));

	pkg->status = STATUS_INITIALING;
	ret = stat(szDirectory, &info);
	if(ret < 0) {
#ifdef _CONSOLE
		printf("%s isn't exist\n", szDirectory);
#endif
		free(pkg);
		return NULL;
	}
#ifdef _WIN32
	if(!(info.st_mode&S_IFDIR)) {
#else
	if(! S_ISDIR(info.st_mode)) {
#endif
		free(pkg);
#ifdef _CONSOLE
		printf("[E]: %s isn't a direcory\n", szDirectory);
#endif
		return NULL;
	}

	pkg->fd = fopen(szArchive, "wb");
	if(! pkg->fd) {
		free(pkg);
#ifdef _CONSOLE
		printf("[E]:open %s fail\n", szArchive);
#endif
		return NULL;
	}

#ifdef _WIN32
	sprintf(pkg->szDirectory, "%s\\", szDirectory);
#else
	sprintf(pkg->szDirectory, "%s", szDirectory);
#endif

	if(func)
		pkg->func = func;
	else
		pkg->func = _default_path_file_filter;

	strncpy(pkg->hdr.szName, szArchive, 32);
	strncpy(pkg->hdr.szAuthor, "china mobilesoft", 32);
	strncpy(pkg->hdr.szVersion, "1.00", 32);
	strncpy(pkg->hdr.szCopyRight, "2000 - 2004 china mobilesoft All right reserved.", 256);

	pkg->pre_size = _get_directory_total_size(pkg, szDirectory);

	pkg->status = STATUS_INITIALED;
#ifdef MD5_CHECK
	MD5Init(&(pkg->md5));
#endif
	return pkg;
}

int pkg_run(PKG *pkg) {
	int ret = RET_FAILURE;

	if(pkg) {
		pkg->status = STATUS_RUNNING;
		ret = _pkg_proc_directory(pkg);
		pkg->status = STATUS_FINISH;
	}

	return ret;
}

int pkg_get_post_size(PKG *pkg) {
	if(pkg) return pkg->post_size;

	return -1;
}

int pkg_get_pre_size(PKG *pkg) {
	if(pkg) return pkg->pre_size;

	return -1;
}

int pkg_get_status(PKG *pkg) {
	if(pkg) return pkg->status;

	return STATUS_UNKNOWN;
}

void pkg_destroy(PKG *pkg) {
	unsigned char digest[16];

	if(pkg && pkg->status == STATUS_FINISH) {
		mywrite(pkg, &pkg->hdr, 1, sizeof(PKG_HDR), pkg->fd);
		mywrite(pkg, MAGIC_ID, 1, strlen(MAGIC_ID), pkg->fd);

#ifdef MD5_CHECK		
		MD5Final(digest, &(pkg->md5));
		fwrite(digest, 1, sizeof(digest), pkg->fd);
#endif

		if(pkg->fd)
			fclose(pkg->fd);
		free(pkg);
	}
}

int pkg_set_filter(PKG *pkg, path_file_filter func) {
	if(pkg != NULL && pkg->status != STATUS_RUNNING) {
		if(func)
			pkg->func = func;
		else
			pkg->func = _default_path_file_filter;

		return RET_SUCCESS;
	}

	return RET_FAILURE;
}

int pkg_hdr_set_copyright(PKG *pkg, char *szCopyRight) {
	if(pkg) {
		memset(pkg->hdr.szCopyRight, 0, 256);
		strncpy(pkg->hdr.szCopyRight, szCopyRight, 256);
		return RET_SUCCESS;
	}

	return RET_FAILURE;
}

int pkg_hdr_set_version(PKG *pkg, char *szVersion) {
	if(pkg) {
		memset(pkg->hdr.szVersion, 0, 256);
		strncpy(pkg->hdr.szVersion, szVersion, 256);
		return RET_SUCCESS;
	}

	return RET_FAILURE;
}

int pkg_hdr_set_name(PKG *pkg, char *szName) {
	if(pkg) {
		memset(pkg->hdr.szName, 0, 256);
		strncpy(pkg->hdr.szName, szName, 256);
		return RET_SUCCESS;
	}

	return RET_FAILURE;
}

int pkg_hdr_set_author(PKG *pkg, char *szAuthor) {
	if(pkg) {
		memset(pkg->hdr.szAuthor, 0, 256);
		strncpy(pkg->hdr.szAuthor, szAuthor, 256);
		return RET_SUCCESS;
	}

	return RET_FAILURE;
}
