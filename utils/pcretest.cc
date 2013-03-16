/**
 * compile command: g++ -o test pcretest.cc -lpcrecpp -lpcre
 */
#include <string.h> 
#include <pcrecpp.h>
#include <stdio.h>

using std::string;
using pcrecpp::StringPiece;
using pcrecpp::RE;
using pcrecpp::RE_Options;

int main(int argc, char** argv) {
	char *cstr = NULL, *patt = NULL;

	cstr = argv[1], patt = argv[2];

	printf("\n\nold.str = %s\n", cstr);
	printf("-------------------------------%s------------------------------\n", patt);

	std::string str(cstr);
	pcrecpp::RE(patt).GlobalReplace("", &str);
	printf("new.str = %s", str.c_str());

	return 0;
}
