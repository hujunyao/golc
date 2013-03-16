#ifndef _NS_DISKINFO_H
#define _NS_DISKINFO_H

#include "nsCOMPtr.h"
#include "nsIDiskinfo.h"
#include <glib.h>

#define NS_IDISKINFO_CONTRACTID "@mozilla.org/diskinfo;1"
#define NS_IDISKINFO_CLASSNAME "Diskinfo"
#define NS_IDISKINFO_CID {0x150a052f, 0x5e4b, 0x438c, {0x9b, 0xc7, 0xf6, 0x84, 0xa6, 0x07, 0x00, 0x82}}

class nsDiskinfo : public nsIDiskinfo {
	public:

		NS_DECL_ISUPPORTS

    NS_DECL_NSIDISKINFO

		nsDiskinfo();
		nsresult Init();
	private:
		~nsDiskinfo();
    GSList *m_MsgList;
};
#endif
