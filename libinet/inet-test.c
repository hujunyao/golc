#include "inet-common.c"
//#include "inet-wifi.c"

int main(int argc, char *argv[]) {
  inet_core_t ic ;
  if_array_t ia;
  int i = 0;

  inet_core_init(&ic);

  if_array_init(&ia);
  inet_get_if(&ic, &ia);

  for(; i < ia.cnt ; i++) {
    if_info_t ii;
    int ret;

    ret = inet_if_fetch_info(&ic, ia.array[i].name, &ii);
    DEBUG("interface[%d] = %s, status = %s\n", i, ia.array[i].name, ii.flags & IFF_UP ? "UP" : "DOWN");
    DEBUG("ip address = %s, type = %d\n", inet_if_get_ipstr(&ii), ii.type, ii.hwaddr);
		inet_dump_hwaddr(ii.hwaddr);
    DEBUG("ip hwaddr = %02x%02x%02x%02x%02x%02x\n", (unsigned char)ii.hwaddr[0], (unsigned char)ii.hwaddr[1], (unsigned char)ii.hwaddr[2], (unsigned char)ii.hwaddr[3], (unsigned char)ii.hwaddr[4], (unsigned char)ii.hwaddr[5]);
  }

  if_array_free(&ia);
  inet_core_free(&ic);

  return 0;
}
