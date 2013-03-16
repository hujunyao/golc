#ifndef _NS_TCPCONN_H
#define _NS_TCPCONN_H

#include <shobjidl.h>
#include <ObjIdl.h>
#include <ShlDisp.h>
#include <ShlGuid.h>
#include <wbemidl.h>
# pragma comment(lib, "wbemuuid.lib")
#include "xpcom-config.h"
#include "nsCOMPtr.h"
#include "nsISTUtils.h"

#define NS_ISTUTILS_CONTRACTID "@splashtop.com/stutils;1"
#define NS_ISTUTILS_CLASSNAME "STUtils"
#define NS_ISTUTILS_CID {0x920ec3e1, 0x01c1, 0x4150, {0xa7, 0x33, 0x96, 0x2c, 0xa7, 0xab, 0x2c, 0x50}}

class nsSTUtils: public nsISTUtils {
	public:

		NS_DECL_ISUPPORTS

    NS_DECL_NSISTUTILS

		nsSTUtils();
		nsresult Init();
    IShellLink *m_pLink;
		IWbemLocator *m_pLoc;
		IWbemServices *m_pSvc;
    nsCOMPtr<nsISENSCallback> m_OnSENSEvent;
	private:
		~nsSTUtils();
};
#endif
