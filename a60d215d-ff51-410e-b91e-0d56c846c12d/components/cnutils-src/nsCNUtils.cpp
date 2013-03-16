#include <stdio.h>
//#include <windows.h>
#include <tchar.h>
#include <comutil.h>
//#include <atlbase.h>
#include <oleacc.h>
#include <mshtml.h>
#include <exdisp.h>
#include <winuser.h>

#include "nsMemory.h"

#include "mozilla/ModuleUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsIBaseWindow.h"

#include "nsCNUtils.h"
#pragma comment (lib, "oleacc")

#define DEBUG(...) fprintf(stderr, "[III] "__VA_ARGS__)

static BOOL CALLBACK EnumChildProc(HWND hWnd, LPARAM lParm) {
	TCHAR szClassName[256] = {0};

	GetClassName(hWnd, (LPSTR)&szClassName, sizeof(szClassName));
	if(_tcscmp(szClassName, _T("Internet Explorer_Server")) == 0) {
		DEBUG("find IE window handle \n");
		*(HWND *)lParm = hWnd;
		return FALSE;
	} else {
		return TRUE;
	}
}

static BOOL FindIEByWnd(HWND hWnd, IHTMLDocument2 **pDoc) {
	HWND hWndChild = NULL;

	EnumChildWindows(hWnd, EnumChildProc, (LPARAM) &hWndChild);
	if(hWndChild == NULL) {
		DEBUG("find IE window handle failed\n");
		return FALSE;
	}

	UINT nMsg = RegisterWindowMessage(_T("WM_HTML_GETOBJECT"));
	LRESULT res;
	SendMessageTimeout(hWndChild, nMsg, 0L, 0L, SMTO_ABORTIFHUNG, 1000, (DWORD *)&res);

	HRESULT hr = ObjectFromLresult(res, IID_IHTMLDocument2, 0, (LPVOID *)pDoc);
	if(FAILED(hr)) {
		DEBUG("Get Object failed\n");
		*pDoc = NULL;
		return FALSE;
	}
	return TRUE;
}

static HWND getParentWindowHWND(nsIBaseWindow *window) {
	nativeWindow hwnd;

	nsresult rv = window->GetParentNativeWindow(&hwnd);
	if(NS_FAILED(rv)) {
		DEBUG("getParentWindowHWND failed\n");
		return NULL;
	}

	return (HWND)hwnd;
}

static char *getTextByName(IHTMLElementCollection *elem, BSTR s_name) {
	IHTMLInputTextElement *textelem = NULL;
	VARIANT name, idx;
	IDispatch *dispatch = NULL;
	long len;

	name.vt = VT_BSTR;
	name.bstrVal = L"soft_version";
	idx.vt = VT_I4;
	idx.lVal = 0;
    elem->get_length(&len);
    DEBUG("IHTMLElementCollection len = %d\n", len);
	elem->item(name, idx, &dispatch);
	DEBUG("getTextByName: dispatch = %x\n", dispatch);
	if(dispatch) {
		dispatch->QueryInterface(IID_IHTMLInputTextElement, (LPVOID *)&textelem);
		if(textelem) {
			BSTR v = NULL;
			char *str;
			textelem->get_value(&v);
			str = _com_util::ConvertBSTRToString(v);
			DEBUG("str = %s\n", str);
			textelem->Release();
			return str;
		}
	}
	return NULL;
}

static char *getFilePathByName(IHTMLElementCollection *elem, BSTR s_name) {
	IHTMLInputFileElement *ifelem = NULL;
  VARIANT name, idx;
  IDispatch *dispatch = NULL;
	long len;

  name.vt = VT_BSTR;
  name.bstrVal = s_name;
  idx.vt = VT_I4;
  idx.lVal = 0;

    elem->get_length(&len);
    DEBUG("IHTMLElementCollection len = %d\n", len);
	elem->item(name, idx, &dispatch);
	DEBUG("getFilePathByName: dispatch = %x\n", dispatch);
	if(dispatch) {
		dispatch->QueryInterface(IID_IHTMLInputFileElement, (LPVOID *)&ifelem);
		if(ifelem) {
			BSTR v = NULL;
			char *str;
			ifelem->get_value(&v);
			str = _com_util::ConvertBSTRToString(v);
			DEBUG("input str = %s\n", str);
			ifelem->Release();
			return str;
		}
	}
	return NULL;
}

typedef enum {
	HTML_TYPE_NULL,
	HTML_TYPE_INPUT,
	HTML_TYPE_OPTION,
	HTML_TYPE_SELECT
}HTMLType;

static void parseHTMLElement(nsCNUtilsImpl *obj, IEnumVARIANT *evar) {
	VARIANT var;
	HRESULT hr;

	hr = evar->Next(1, &var, NULL);
	if(SUCCEEDED(hr)) {
		IDispatch *disp = NULL;
		BOOL finished = FALSE;

		for(; (disp = V_DISPATCH(&var))&&(finished==FALSE) ; evar->Next(1, &var, NULL)) {
			IHTMLElement *curr = NULL;
			IHTMLSelectElement *opt = NULL;
			VARIANT val, name;
			HTMLType t = HTML_TYPE_NULL;
			BSTR tag, innerHTML;
			char *valstr = NULL, *namestr = NULL;
			disp->QueryInterface(IID_IHTMLElement, (LPVOID *)&curr);
			if(curr) {
				if(S_OK == curr->get_tagName(&tag)) {
					if(wcscmp(tag, L"INPUT") == 0)
						t = HTML_TYPE_INPUT;
					//if(wcscmp(tag, L"SELECT") == 0) {
					//	t = HTML_TYPE_SELECT;
					//	disp->QueryInterface(IID_IHTMLSelectElement, (LPVOID *)&opt);
					//}
				}
				if(t == HTML_TYPE_NULL) {
						curr->Release();
						continue;
				}
				//if(opt) {
				//	BSTR optstr;
				//	if(S_OK == opt->get_value(&optstr)) {
				//		char *str = _com_util::ConvertBSTRToString(optstr);
				//		if(str) {
				//		DEBUG("optstr = %s\n", str);
				//		delete[] str;
				//		}
				//	}
				//}

				if(S_OK == curr->getAttribute(L"value", 1, &val)) {
					valstr = _com_util::ConvertBSTRToString(val.bstrVal);
					if(valstr) {
					  DEBUG("value = %s\t", valstr);
					}
				}
				hr = curr->getAttribute(L"name", 1, &name);
				if(S_OK == hr) {
					//DEBUG("str = %x, name val.vt = %x\n", namestr, val.vt);
					namestr = _com_util::ConvertBSTRToString(name.bstrVal);
					if(namestr) {
					  DEBUG("name = %sn", namestr);
						if(strcmp(namestr, "uploadfile_215_4") == 0)
							finished = TRUE;
					}
				}

				if(namestr) delete namestr;
				if(valstr) delete valstr;
				curr->Release();
			}
			disp->Release();
		}/**end of for loop*/
	}
}

static BOOL checkESTForm(nsCNUtilsImpl *obj, IHTMLDocument3 *vDoc) {
	return FALSE;
}

static IHTMLDocument3* getDocumentEx(IHTMLWindow2 *htmlWnd) {
	IServiceProvider *sp = NULL;
	IWebBrowser2 *browser = NULL;
	IHTMLDocument3 *vDoc = NULL;
	IDispatch *dispatch = NULL;

	htmlWnd->QueryInterface(IID_IServiceProvider, (LPVOID *)&sp);
	if(sp) {
		sp->QueryService(IID_IWebBrowserApp, IID_IWebBrowser2, (LPVOID *)&browser);
		if(browser) {
			browser->get_Document(&dispatch);
			if(dispatch) {
				dispatch->QueryInterface(IID_IHTMLDocument3, (LPVOID *)&vDoc);
				dispatch->Release();
			}
			DEBUG("get IHTMLDocument3 from browser = %x\n", vDoc);
			browser->Release();
		}
		sp->Release();
	}
	return vDoc;
}

#define CLR_ALL_STR \
	m_svn = m_sver = m_hver = m_lver = m_bver = NULL; \
	m_prog = m_script = m_log = m_dvt = NULL;

#define DEL_ALL_STR \
	if(m_svn) delete[] m_svn; if(m_sver) delete[] m_sver; if(m_lver) delete[] m_lver; if(m_bver) delete[] m_bver; if(m_hver) delete[] m_hver; \
	if(m_prog) delete[] m_prog; if(m_script) delete[] m_script; if(m_log) delete[] m_log; if(m_dvt) delete[] m_dvt;

nsCNUtilsImpl::nsCNUtilsImpl() {
	CoInitialize(NULL);
	m_pHTMLDoc = NULL;
	CLR_ALL_STR
}

nsCNUtilsImpl::~nsCNUtilsImpl() {
	CoUninitialize();
	if(m_pHTMLDoc)
		m_pHTMLDoc->Release();
	DEL_ALL_STR
}

NS_IMPL_CLASSINFO(nsCNUtilsImpl, NULL, 0, NS_CNUTILS_CID)
NS_IMPL_ISUPPORTS1_CI(nsCNUtilsImpl, nsICNUtils)

NS_IMETHODIMP nsCNUtilsImpl::DoCheck(nsIBaseWindow *mainWnd) {
	DEBUG("XPCOM: DoCheck\n");
	DEL_ALL_STR
	CLR_ALL_STR
	HWND hwnd = getParentWindowHWND(mainWnd);
	if(hwnd) {
		if(FindIEByWnd(hwnd, &m_pHTMLDoc)) {
			long i = 0, len;
			VARIANT vtIdx, vtObj;
			IHTMLFramesCollection2 *frames = NULL;
			IHTMLWindow2 *htmlWnd = NULL;
			IDispatch *vWnd = NULL;
			m_pHTMLDoc->get_frames(&frames);
			frames->get_length(&len);
			DEBUG("frames.len = %d\n", len);
			if(len > 0) {
			  vtIdx.vt = VT_BSTR;
			  vtIdx.bstrVal = L"main";
			  frames->item(&vtIdx, &vtObj);
			  if(vtObj.ppdispVal) {
			  	vWnd = (IDispatch *)(vtObj.ppdispVal);
			  	vWnd->QueryInterface(IID_IHTMLWindow2, (LPVOID*)&htmlWnd);
			  	DEBUG("vWnd = %x, htmlWnd = %x\n", vWnd, htmlWnd);
			  	if(htmlWnd) {
						IHTMLDocument3 *pDoc = NULL;
			  		IHTMLDocument2 *vDoc = NULL;
			  	  HRESULT hr = htmlWnd->get_document(&vDoc);
						if(hr != S_OK) {
							pDoc = getDocumentEx(htmlWnd);
						} else {
							vDoc->QueryInterface(IID_IHTMLDocument3, (LPVOID *)&pDoc);
							DEBUG("get IHTMLDocument3 from IHTMLDocument2, pDoc=%x\n", pDoc);
							vDoc->Release();
						}
						if(pDoc) {
							checkESTForm(this, pDoc);
							pDoc->Release();
						}
			  	  htmlWnd->Release();
			  	}
			  }
			}
			frames->Release();
		}
	}

	return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsCNUtilsImpl)
NS_DEFINE_NAMED_CID(NS_CNUTILS_CID);

static const mozilla::Module::CIDEntry kCNUtilsCIDs[] = {
	{ &kNS_CNUTILS_CID, false, NULL, nsCNUtilsImplConstructor },
	{ NULL }
};

static const mozilla::Module::ContractIDEntry kCNUtilsContracts[] = {
	{ NS_CNUTILS_CONTRACTID, &kNS_CNUTILS_CID },
	{ NULL }
};

static const mozilla::Module::CategoryEntry kCNUtilsCategories[] = {
	{ "cn-category", "cn-key", NS_CNUTILS_CONTRACTID },
	{ NULL }
};

static const mozilla::Module kCNUtilsModule = {
	mozilla::Module::kVersion,
	kCNUtilsCIDs,
	kCNUtilsContracts,
	kCNUtilsCategories
};

NSMODULE_DEFN(nsCNUtilsModule) = &kCNUtilsModule;
NS_IMPL_MOZILLA192_NSGETMODULE(&kCNUtilsModule)
