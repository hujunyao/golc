#include <stdio.h>
#include <time.h>

time_t atotm(char *tm) {
  time_t r = 0;

  while(*tm != '\0') {
    r = (*tm - '0') + (r *10);
    tm ++;
  }
  return r;
}

int main(int argc, char *argv[]) {
  time_t tm;

  if(argc < 2) {
    tm = time(NULL);
  } else {
    tm = atotm(argv[1]);
  }

  printf("tm = %s\n", ctime(&tm));

  return 0;
}
