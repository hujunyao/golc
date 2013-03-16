#include "base.h"
#include "java-lex.h"

#define DD(...) fprintf(stderr, "[JAVA] "__VA_ARGS__)

int main(int argc, char *argv[]) {
	FILE *fp = NULL;
	baseinfo_t *bi = NULL;
	yyscan_t ys;

	bi = (baseinfo_t *)baseinfo_new();
	fp = fopen(argv[1], "r");

	java_lex_init_extra(bi, &ys);
	java_set_in(fp, ys);
	java_lex(ys);

	DD("pure java line = %d, pure char = %d\n", bi->pure_line, bi->pure_char);

	java_lex_destroy(ys);

	fclose(fp);
	baseinfo_free(bi);
	return 0;
}

