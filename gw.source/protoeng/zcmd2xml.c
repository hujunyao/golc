#include "utils.h"
#include "base.h"

#define ZCMDSIZE 2048

char *xmlfmt = "<Trans TransId = '9' TransType = 'ZNET_STATUS_RPT '> <Report RptType = 'Update'> <!-- DEV_STUS/OPT_STUS--> <HACInfo AccessIp = '*.*.*.*' Port = '9783' > <ZNet PanFreq =  AccessCode =  > <ZDev DevId = %s> <Status StaType = 'Event' StaName = '%s' StaValue = '%s'/> </ZDev> </ZNet> </HACInfo> </Report> </Trans>";

xmlDocPtr zcmd2xml(gw_ipc_t *ipc, unsigned char *zcmdbuf) {/**convert zcmdbuf to xmlbuf*/
#if 0
	char xmlbuf[ZCMDSIZE] = {0};
	char *p = NULL, *ptr = NULL;
	char *devid = NULL, *cmd = NULL, *temp = NULL;

	p = strchr(zcmdbuf, ',');
	*p = '\0', p++, ptr = p;
	devid = zcmdbuf;

	p = strchr(ptr, ',');
	p++, ptr = p;

	p = strchr(ptr, ',');
	*p = '\0', p++, ptr = p;
	cmd = ptr;

	p = strchr(ptr, ';');
	*p = '\0', p++;
	temp = ptr;
	
	sprintf(xmlbuf, xmlfmt, devid, cmd, temp);
	printf("zcmd2xml = %s\n", xmlbuf);
	gw_ipc_sendmsg(ipc, GWAPPIPC, xmlbuf, strlen(xmlbuf));
#endif

	return (xmlDocPtr)xmlbuf_new_doc_from_buffer(zcmdbuf);
}
