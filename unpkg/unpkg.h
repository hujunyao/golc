#ifndef _UNPKG_H
#define _UNPKG_H

#define SETDIRFLAG(flag) flag | 0x80
#define CLRDIRFLAG(flag) flag & 0x7f
#define CHKDIRFLAG(flag) flag & 0x80

#define RET_FAILURE	0xFF
#define RET_SUCCESS 0x00

#define BITS_OFFSET 0x04
#define MD5_CHECK

typedef struct unpkg_t UNPKG;

int pkg_md5_chk(char *szArchive);
/*create a unpkg instance*/
UNPKG *unpkg_create(char *szArchive, char *szDirectory);
/*run this unpkg instance*/
int unpkg_run(UNPKG *pkg);
/*destroy this unpkg instance*/
void unpkg_destroy(UNPKG *pkg);
#endif
/*unpkg.h*/
