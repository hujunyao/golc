#define GET(x) get_##x
#define STR(x, y) "STRING1:"#x" STRING2: "#y

#include <stdio.h>

void get_a(void) {
  printf("get_a invoked.\n");
}

void get_b(void) {
  printf("get_b invoked.\n");
}

int main(int argc, char *argv[]) {
  GET(a)();

  printf(STR(hello world., powered by C Language.)"\n");
  return 0;
}
