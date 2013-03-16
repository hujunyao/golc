#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "nscore.h"
#include "nsIGenericFactory.h"
#include "nsTCPConn.h"

#define BUFSIZE 1024
#define ERROR(...) fprintf(stderr, "[XXX] "__VA_ARGS__)
#define DEBUG(...) fprintf(stderr, "[III] "__VA_ARGS__)

static gboolean _tcp_fd_watch(GIOChannel *source, GIOCondition cond, gpointer data) {
  PRBool ret = TRUE;
  nsTCPConn *obj = (nsTCPConn *)data;
  char buf[BUFSIZE] = {0};

  if(cond & (G_IO_ERR|G_IO_HUP|G_IO_NVAL)) {
    DEBUG("tcp socket fd is INVALID\n");
    return FALSE;
  }
  if(cond & G_IO_IN) {
    //DEBUG("channel cond = %x, G_IO_IN = %x, ret = %d\n", cond, G_IO_IN, ret);
    if(read(obj->tcpfd, buf, BUFSIZE) > 0) {
      if(obj->m_OnRead) {
        obj->m_OnRead->Call(0, buf, &ret);
      }
    }
  }

  return TRUE;
}

nsTCPConn::nsTCPConn() {
  tcpfd = socket(AF_INET, SOCK_STREAM, 0);
  m_OnRead = NULL;
}

nsresult nsTCPConn::Init() {
  DEBUG("TCPConn->init\n");
	return NS_OK;
}

nsTCPConn::~nsTCPConn() {
  if(this->tcpfd)
    close(this->tcpfd);
}

NS_IMETHODIMP nsTCPConn::SetOnReadCallback(nsIOnReadCallback *aCallback) {
  this->m_OnRead = aCallback;

  return NS_OK;
}

NS_IMETHODIMP nsTCPConn::SendMessage(const char *msg, PRBool *rv) {

  *rv = FALSE;
  if(this->tcpfd < 0) {
    DEBUG("socket tcpfd is INVALID!\n");
    return NS_OK;
  }

  if(write(this->tcpfd, msg, strlen(msg) + 1) > 0) {
    *rv = TRUE;
    DEBUG("SendMessage successful!\n");
  }
  return NS_OK;
}

NS_IMETHODIMP nsTCPConn::OpenTCPConn(const char *ipaddr, PRUint32 port, PRBool *rv) {
  sockaddr_in sa = {0};
  int opt = 1;

  if(this->tcpfd < 0) {
    *rv = FALSE;
    DEBUG("socket tcpfd is INVALID!\n");
    return NS_OK;
  }

  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = inet_addr(ipaddr);

  if(setsockopt(tcpfd, IPPROTO_TCP,TCP_NODELAY,(char *) &opt, sizeof(int)) < 0) {
    DEBUG("set socketopt IPPROTO_TCP.TCP_NODELAY failed!\n");
  }
  if(connect(this->tcpfd, (sockaddr *)&sa, sizeof(sockaddr)) < 0) {
    *rv = FALSE;
  } else {
    GIOChannel *channel = NULL;
    GIOCondition cond = (GIOCondition)(G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL);

    channel = g_io_channel_unix_new(this->tcpfd);
    g_io_add_watch(channel, cond, _tcp_fd_watch, this);
    g_io_channel_unref(channel);
    *rv = TRUE;
  }

	return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsTCPConn, Init)

static NS_METHOD nsTCPConnRegProc (nsIComponentManager *aCompMgr,
                                  nsIFile *aPath,
                                  const char *registryLocation,
                                  const char *componentType,
                                  const nsModuleComponentInfo *info) {


	DEBUG("nsTCPConnRegProc\n");
    return NS_OK;
}

static NS_METHOD nsTCPConnUnregProc (nsIComponentManager *aCompMgr,
                                    nsIFile *aPath,
                                    const char *registryLocation,
                                    const nsModuleComponentInfo *info) {
	DEBUG("nsTCPConnUnregProc\n");
    return NS_OK;
}


NS_IMPL_ISUPPORTS1(nsTCPConn, nsITCPConn)

static const nsModuleComponentInfo components[] = {
	{
		NS_ITCPCONN_CLASSNAME, NS_ITCPCONN_CID, NS_ITCPCONN_CONTRACTID,
		nsTCPConnConstructor, nsTCPConnRegProc, nsTCPConnUnregProc
	}
};

NS_IMPL_NSGETMODULE(nsTCPConn, components)
