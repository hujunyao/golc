#include <stdio.h>

int main(int argc , char *argv[]) {
  FILE *fp = NULL;
  char *speed = "118";
  char *sensitivity = "250";
  char *pts = "1";

  fp = fopen("/sys/devices/platform/i8042/serio4/serio5/speed", "w");
  fwrite(speed, sizeof(speed), 1, fp);
  fclose(fp);

  fp = fopen("/sys/devices/platform/i8042/serio4/serio5/sensitivity", "w");
  fwrite(sensitivity, sizeof(sensitivity), 1, fp);
  fclose(fp);

  fp = fopen("/sys/devices/platform/i8042/serio4/serio5/press_to_select", "w");
  fwrite(pts, sizeof(pts), 1, fp);
  fclose(fp);

  return 0;
}

