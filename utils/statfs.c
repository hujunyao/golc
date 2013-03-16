#include <sys/vfs.h>
#include <stdio.h>

int main(int argc, char *argv[]) {

  struct statfs fsd;
  double precent = 0.0;
  double total, free;
  char *mntpath = "/";

  if(argv[1])
    mntpath = argv[1];

  if(statfs(mntpath, &fsd) < 0) {
    printf("get %s statfs failed\n", mntpath);
    return 0;
  }
  if(fsd.f_blocks < fsd.f_bavail) {
    printf("f_blocks is too LONG: fsd.f_blocks < fsd.f_bavail\n");
  } else {
    free = fsd.f_bavail*100.00;
    total = fsd.f_blocks*1.00;
    printf("mount point %s free disk precent %f.\n", mntpath, free/total);
    printf("disk.info[ free = %ld, avail=%ld, total = %ld ]\n", fsd.f_bfree*4, fsd.f_bavail*4, fsd.f_blocks*4);
  }
  return 0;
}

