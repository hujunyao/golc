#ifndef __CNUTILS_H__
#define __CNUTILS_H__

#include "nsICNUtils.h"

#define NS_CNUTILS_CID\
	{0x097ecc80, 0x4c4e, 0x478f, {0x83, 0xd5, 0xf6, 0x63, 0x0d, 0x20, 0xcd, 0xbd}}

#define NS_CNUTILS_CONTRACTID "@wuruxu.com/cnutils;1"

class nsCNUtilsImpl: public nsICNUtils {
	public:
		nsCNUtilsImpl();
		NS_DECL_ISUPPORTS
		NS_DECL_NSICNUTILS

	private:
		~nsCNUtilsImpl();
		IHTMLDocument2 *m_pHTMLDoc;
		char *m_svn, *m_sver, *m_hver, *m_lver, *m_bver;
		char *m_prog, *m_script, *m_log, *m_dvt;
};

#endif
