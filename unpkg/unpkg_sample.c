#include "unpkg.h"

int main(int argc, char *argv[]) {
	UNPKG *pkg;

#ifdef _CONSOLE
	printf("\nunpkg -v 1.00\n");
	printf("Date:2004.07.28\n");
	printf("Author:RuXu W.\n");
	printf("Website:http://www.mobilesoft.com.cn\n");
	printf("Company:Mobilesoft Technology(Nanjing) Co.,Ltd.\n");
	printf("Address:Nanjing, Jiangsu, China\n\n");
#endif

	if(argc < 3) {
#ifdef _CONSOLE
		printf("[E] usage:unpkg source.pkg target\n");
#endif
		exit(-1);
	}
#ifdef MD5_CHECK
	if(pkg_md5_chk(argv[1]) == RET_SUCCESS)
		printf("MD5 OK\n");
	else
		printf("MD5 ERR\n");
#endif
	pkg = unpkg_create(argv[1], argv[2]);
	if(pkg) {
		unpkg_run(pkg);
		unpkg_destroy(pkg);
	} else {
#ifdef _CONSOLE
	 	printf("unpkg %s failure\n", argv[1]);
#endif
	}

	return 0;
}