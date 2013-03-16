#ifndef _NS_TCPCONN_H
#define _NS_TCPCONN_H

#include <glib.h>
#include "nsCOMPtr.h"
#include "nsITCPConn.h"

#define NS_ITCPCONN_CONTRACTID "@mozilla.org/tcpconn;1"
#define NS_ITCPCONN_CLASSNAME "TCPConn"
#define NS_ITCPCONN_CID {0xb3eed5a6, 0xa580, 0x43ba, { 0xa4, 0xf4, 0xc2, 0x7c, 0xf2, 0x7b, 0xbd, 0x5c}}

class nsTCPConn : public nsITCPConn {
	public:

		NS_DECL_ISUPPORTS

    NS_DECL_NSITCPCONN

    nsCOMPtr<nsIOnReadCallback> m_OnRead;
    int tcpfd;

		nsTCPConn();
		nsresult Init();
	private:
		~nsTCPConn();
};
#endif
