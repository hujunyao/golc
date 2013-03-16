#include <iwlib.h>
#include "inet-types.h"

int inet_wifi_scan(inet_core_t *ic, char *ifname, wireless_scan_head **psr) {
  struct iw_range range;
  wireless_scan_head *cxt = *psr;

  if(iw_get_range_info(ic->fd, ifname, &range) >= 0) {
    int delay = 0;
    cxt->result = NULL, cxt->retry = 0;

    delay = iw_process_scan(ic->fd, ifname, range.we_version_compiled, cxt);
    return delay;
  }
  return -1;
}

int inet_if_type_is_wifi(inet_core_t *ic, if_info_t *ii) {
  struct iwreq wrq;
  struct wireless_info wi = {0};

  if(iw_get_basic_config(ic->fd, ii->name, &(wi.b)) < 0) {
    return FALSE;
  }

  return TRUE;
}
