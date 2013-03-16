#ifndef _PKG_H
#define _PKG_H

//#define _CONSOLE

#define SETDIRFLAG(flag) flag | 0x80
#define CLRDIRFLAG(flag) flag & 0x7f
#define CHKDIRFLAG(flag) flag & 0x80

#define RET_FAILURE	0xFF
#define RET_SUCCESS 0x00

#define TYPE_FILE		0xFF
#define TYPE_DIR		0x00

#define STATUS_UNKNOWN		0x81
#define STATUS_INITIALING	0x00
#define STATUS_INITIALED	0x01
#define STATUS_RUNNING		0x02
#define STATUS_FINISH		0x03

#define MD5_CHECK

typedef struct pkg_t PKG;

typedef int (* path_file_filter)(PKG *pkg, char *szfilename, int type);

/*set filter for path & file*/
int pkg_set_filter(PKG *pkg, path_file_filter func);
/*get post-compressed file size*/
int pkg_get_post_size(PKG *pkg);
/*get pre-compressed file total size*/
int pkg_get_pre_size(PKG *pkg);
/*get pkg instance status*/
int pkg_get_status(PKG *pkg);

/*set pkg application name*/
int pkg_hdr_set_name(PKG *pkg, char *szName);
/*set pkg version*/
int pkg_hdr_set_version(PKG *pkg, char *szVersion);
/*set pkg author*/
int pkg_hdr_set_author(PKG *pkg, char *szAuthor);
/*set pkg copyright*/
int pkg_hdr_set_copyright(PKG *pkg, char *szCopyright);

/*create a pkg instance*/
PKG *pkg_create(char *szDirectory, char *szArchive, path_file_filter func);
/*run this pkg instance*/
int pkg_run(PKG *pkg);
/*destroy this pkg instance*/
void pkg_destroy(PKG *pkg);

#endif
