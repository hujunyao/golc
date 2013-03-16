#ifndef _NS_TCPCONN_H
#define _NS_TCPCONN_H

#include <glib.h>
#include "nsCOMPtr.h"
#include "nsIJSUtils.h"

#define NS_IJSUTILS_CONTRACTID "@mozilla.org/jsutils;1"
#define NS_IJSUTILS_CLASSNAME "JSUtils"
#define NS_IJSUTILS_CID {0xacad5005, 0x4d90, 0x49e5, { 0xa3, 0x89, 0xb2, 0xb7, 0x47, 0xc9, 0x00, 0xf0 }}

class nsJSUtils: public nsIJSUtils {
	public:

		NS_DECL_ISUPPORTS

    NS_DECL_NSIJSUTILS

		nsJSUtils();
		nsresult Init();
    nsCOMPtr<nsIAppWatchCallback> m_AppWatch;
    nsCOMPtr<nsIWindowEventFilterCallback> m_EventFilter;
    nsCOMPtr<nsITrayPopupMenuCallback> m_PopupMenuCallback;
    nsCOMPtr<nsIMsgCleanupCallback> m_Cleanup;
    GdkWindow *m_MainWindow;
    GtkWidget *m_DWNWindow, *m_DWNChkButton;
    GtkStatusIcon *m_Tray;
    GSList *m_KeyValue;
	private:
		~nsJSUtils();
};
#endif
