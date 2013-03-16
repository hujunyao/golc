#include "pkg.h"

static void print_progress(PKG *pkg) {
	double n, i, j;

	switch(pkg_get_status(pkg)) {
		case STATUS_UNKNOWN:
		case STATUS_INITIALING:
			break;
		default:
			i = pkg_get_post_size(pkg);
			j = pkg_get_pre_size(pkg);

			n = (i/j)*100;
			printf("progress = %.2f%c\n", n, '%');
			break;
	}
}

static int my_filter(PKG *pkg, char *szfilename, int flag) {
	if(flag == TYPE_FILE) {/*
		if(strstr(szfilename, ".c") ||
		   strstr(szfilename, ".h")) {
			print_progress(pkg);
			return RET_SUCCESS;
		}*/

		return RET_SUCCESS;
	} else {
		if(strcmp(szfilename, ".") == 0)
			return RET_FAILURE;
		if(strcmp(szfilename, "..") == 0)
			return RET_FAILURE;
		if(strstr(szfilename, "akrip"))
			return RET_SUCCESS;
		return RET_SUCCESS;
	}
	return RET_FAILURE;
}

int main(int argc, char *argv[]) {
	PKG *pkg;

#ifdef _CONSOLE
	printf("\npkg -v 1.00\n");
	printf("Date:2004.07.28\n");
	printf("Author:RuXu W.\n");
	printf("Website:http://www.mobilesoft.com.cn\n");
	printf("Company:Mobilesoft Technology(Nanjing) Co.,Ltd.\n");
	printf("Address:Nanjing, Jiangsu, China\n\n");
#endif
	if(argc < 3) {
#ifdef _CONSOLE
		printf("usage:pkg source target.pkg \n");
#endif
		exit(-1);
	}

	pkg = pkg_create(argv[1], argv[2], my_filter);
	if(pkg) {
		pkg_run(pkg);
		pkg_destroy(pkg);
	} else {
#ifdef _CONSOLE
	 	printf("pkg %s failure\n", argv[1]);
#endif
	}

	return 0;
}