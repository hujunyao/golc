#include <stdio.h>
#include <string.h>
#define _WIN32_DCOM
#include <Objbase.h>
#include <ShlObj.h>
#include <Sddl.h>
#include <comdef.h>
#include <plstr.h>
#ifndef UNICODE
#define UNICODE
#endif
#pragma comment(lib, "netapi32.lib")
#include <lm.h>

#include "nscore.h"
#include "nsIGenericFactory.h"
#include "nsSTUtils.h"

#define BUF_SIZE_L0 256
#define BUF_SIZE_L1 1024

#define NUM_ELEMENTS(x) (sizeof((x)) / sizeof((x)[0]))
#define DEBUG(...) fprintf(stderr, "[XPCOM.stutils] "__VA_ARGS__)
typedef BOOL (WINAPI *tConvertSidToStringSid)(PSID, LPTSTR *);

#define CINTERFACE 0
#include <sensevts.h>
#include <eventsys.h>
const char* MethodNames[]={
    "ConnectionMade",
    "ConnectionMadeNoQOCInfo",
    "ConnectionLost",
    "DestinationReachable",
    "DestinationReachableNoQOCInfo"
};
const char* SubscriptionNames[]={
    "TSTConnectionMade",
    "TSTConnectionMadeNoQOCInfo",
    "TSTConnectionLost",
    "TSTDestinationReachable",
    "TSTDestinationReachableNoQOCInfo"
};
unsigned char* SubscriptionIDs[5] = { 0, 0, 0, 0, 0 };

typedef struct {
    ISensNetworkVtbl *lpVtbl;
    LONG ref;
    nsSTUtils *obj;
} MySensNetwork;

static MySensNetwork *pMySensNetwork = 0;
static IEventSystem *pIEventSystem = 0;

static const BSTR MB2WCHAR (const char* a) {
    static WCHAR b[64];
    MultiByteToWideChar (0, 0, a, -1, b, 64);
    return b;
}

static const char* AddCurlyBraces (const char* stringUUID) {
    static char curlyBracedUuidString[64];
    int i;
    if (!stringUUID)
        return NULL;
    lstrcpy(curlyBracedUuidString,"{");
    i = strlen(curlyBracedUuidString);
    lstrcat(curlyBracedUuidString+i,stringUUID);
    i = strlen(curlyBracedUuidString);
    lstrcat(curlyBracedUuidString+i,"}");
    return curlyBracedUuidString;
}

HRESULT WINAPI MySensNetwork_QueryInterface(ISensNetwork *pISensNetwork, REFIID iid, void ** ppv)
{
    *ppv=NULL;

    if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IDispatch) || IsEqualIID(iid, IID_ISensNetwork))
        *ppv=pISensNetwork;

    if (NULL==*ppv)
        return ResultFromScode(E_NOINTERFACE);

    ((LPUNKNOWN)*ppv)->AddRef();
    return NOERROR;
}

ULONG WINAPI MySensNetwork_AddRef(ISensNetwork *pISensNetwork)
{
    MySensNetwork *pMySensNetwork=(MySensNetwork*)pISensNetwork;
    return InterlockedIncrement(&(pMySensNetwork->ref));
}

ULONG WINAPI MySensNetwork_Release(ISensNetwork *pISensNetwork)
{
    MySensNetwork *pMySensNetwork=(MySensNetwork*)pISensNetwork;
    ULONG tmp = InterlockedDecrement(&(pMySensNetwork->ref));
    return tmp;
}

HRESULT WINAPI MySensNetwork_GetTypeInfoCount(ISensNetwork *pISensNetwork, UINT *pctinfo)
{
    return E_NOTIMPL;
}

HRESULT WINAPI MySensNetwork_GetTypeInfo(ISensNetwork *pISensNetwork, UINT iTInfo,LCID lcid,ITypeInfo **ppTInfo)
{
    return E_NOTIMPL;
}

HRESULT WINAPI MySensNetwork_GetIDsOfNames(ISensNetwork *pISensNetwork, REFIID riid,
                                           LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    return E_NOTIMPL;
}

HRESULT WINAPI MySensNetwork_Invoke(ISensNetwork *pISensNetwork,DISPID dispIdMember,REFIID riid,
                                    LCID lcid,WORD wFlags,DISPPARAMS *pDispParams,VARIANT *pVarResult,
                                    EXCEPINFO *pExcepInfo,UINT *puArgErr)
{
    return E_NOTIMPL;
}

HRESULT WINAPI MySensNetwork_ConnectionMade(ISensNetwork *pISensNetwork, BSTR bstrConnection,
                                            ULONG ulType, LPSENS_QOCINFO lpQOCInfo)
{
    MySensNetwork *pMySensNetwork=(MySensNetwork*)pISensNetwork;
    if (ulType) {
      DEBUG("* Network Connection Made *\n");
      PRBool ret = TRUE;
      nsSTUtils *obj = pMySensNetwork->obj;
      if(obj && obj->m_OnSENSEvent)
        obj->m_OnSENSEvent->Call(0, "enable", &ret);
    }
    return S_OK;
}

HRESULT WINAPI MySensNetwork_ConnectionMadeNoQOCInfo(ISensNetwork *pISensNetwork,
                                                     BSTR bstrConnection,ULONG ulType)
{
    return S_OK;
}

HRESULT WINAPI MySensNetwork_ConnectionLost(ISensNetwork *pISensNetwork,  BSTR bstrConnection, ULONG ulType)
{
    MySensNetwork *pMySensNetwork=(MySensNetwork*)pISensNetwork;
    if (ulType){ 
      DEBUG("* Network Connection Lost *\n");
      PRBool ret = TRUE;
      nsSTUtils *obj = pMySensNetwork->obj;
      if(obj && obj->m_OnSENSEvent)
        obj->m_OnSENSEvent->Call(0, "disable", &ret);
    }
    return S_OK;
}

HRESULT WINAPI MySensNetwork_DestinationReachable(ISensNetwork *pISensNetwork, BSTR bstrDestination,
                                                  BSTR bstrConnection,ULONG ulType,
                                                  LPSENS_QOCINFO lpQOCInfo)
{
    return E_NOTIMPL;
}

HRESULT WINAPI MySensNetwork_DestinationReachableNoQOCInfo(ISensNetwork *pISensNetwork, BSTR bstrDestination,
                                                           BSTR bstrConnection,ULONG ulType)
{
    return E_NOTIMPL;
}

ISensNetworkVtbl MySensNetwork_Vtbl = {
    MySensNetwork_QueryInterface,
    MySensNetwork_AddRef,
    MySensNetwork_Release,
    MySensNetwork_GetTypeInfoCount,
    MySensNetwork_GetTypeInfo,
    MySensNetwork_GetIDsOfNames,
    MySensNetwork_Invoke,
    MySensNetwork_ConnectionMade,
    MySensNetwork_ConnectionMadeNoQOCInfo,
    MySensNetwork_ConnectionLost,
    MySensNetwork_DestinationReachable,
    MySensNetwork_DestinationReachableNoQOCInfo
};

void MySensNetwork_Init(MySensNetwork **pMySensNetwork, nsSTUtils *obj)
{
    (*pMySensNetwork) = (MySensNetwork *)malloc(sizeof(MySensNetwork));
    (*pMySensNetwork)->lpVtbl = &MySensNetwork_Vtbl;
    (*pMySensNetwork)->obj= obj;
    (*pMySensNetwork)->ref = 1;
}

void MySensNetwork_Finalize(MySensNetwork **pMySensNetwork)
{
    if (*pMySensNetwork)
        free(*pMySensNetwork);
    (*pMySensNetwork) = NULL;
}

static void regNetworkEvent(nsSTUtils *obj) {
    int res = 0, i = 0;
    static const char* EventClassID="{D5978620-5B9F-11D1-8DD2-00AA004ABD5E}";

    static IEventSubscription *pIEventSubscription = 0;
    res=CoCreateInstance(CLSID_CEventSystem, 0, CLSCTX_SERVER, IID_IEventSystem,(LPVOID*)&pIEventSystem);
    MySensNetwork_Init(&pMySensNetwork, obj);

    for (i=0; i<NUM_ELEMENTS(SubscriptionNames); i++) {
        UUID tmp_uuid;
        res=CoCreateInstance(CLSID_CEventSubscription,0, CLSCTX_SERVER,
                             IID_IEventSubscription,(LPVOID*)&pIEventSubscription);
        res=pIEventSubscription->lpVtbl->put_EventClassID(pIEventSubscription,MB2WCHAR(EventClassID));
        res=pIEventSubscription->lpVtbl->put_SubscriberInterface(pIEventSubscription,(LPUNKNOWN)pMySensNetwork);
        res=pIEventSubscription->lpVtbl->put_MethodName(pIEventSubscription,MB2WCHAR(MethodNames[i]));
        res=pIEventSubscription->lpVtbl->put_SubscriptionName(pIEventSubscription,MB2WCHAR(SubscriptionNames[i]));;
        if (RPC_S_OK == UuidCreate(&tmp_uuid))
            UuidToString(&tmp_uuid, &SubscriptionIDs[i]);
        res=pIEventSubscription->lpVtbl->put_SubscriptionID(pIEventSubscription,
                                                            MB2WCHAR(AddCurlyBraces((const char*)SubscriptionIDs[i])));
        res=pIEventSubscription->lpVtbl->put_PerUser(pIEventSubscription,TRUE);
        res=pIEventSystem->lpVtbl->Store(pIEventSystem, PROGID_EventSubscription,(LPUNKNOWN)pIEventSubscription);
        pIEventSubscription->lpVtbl->Release(pIEventSubscription);
        pIEventSubscription=0;
    }
}

static void unregNetworkEvent() {
  int i = 0, res = 0, err;

  for (i=0; i<NUM_ELEMENTS(SubscriptionNames); i++) {
      char QueryCriteria[64];
      int length;
      lstrcpy(QueryCriteria,"SubscriptionID=");
      length = strlen(QueryCriteria);
      lstrcat(QueryCriteria+length,(const char*)SubscriptionIDs[i]);
      res = pIEventSystem->lpVtbl->Remove(pIEventSystem,PROGID_EventSubscription,
                                          MB2WCHAR(QueryCriteria),&err);
  }
  pIEventSystem->lpVtbl->Release(pIEventSystem);
  MySensNetwork_Finalize(&pMySensNetwork);
}
#define CINTERFACE 1

nsSTUtils::nsSTUtils() {
  HRESULT ret;

  ret = ::CoInitialize(NULL);
  if(ret != S_OK) {
    DEBUG("COM is already initialized.\n");
  }
  ret = CoCreateInstance(CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,IID_IShellLink,(void**)&m_pLink);
  if(ret != S_OK) {
    DEBUG("Create IShellLink Instance failed\n");
    return;
  }

	ret = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *) &m_pLoc);
  if(ret != S_OK) {
		DEBUG("Create IWbemLocator Instance failed\n");
	}
	ret = m_pLoc->ConnectServer(BSTR(L"root\\cimv2"), NULL, NULL, 0, NULL, 0, 0, &m_pSvc);
	if(ret != S_OK) {
		DEBUG("ConnectServer root\\cimv2 failed\n");
	}
	ret = CoSetProxyBlanket(m_pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
	if(FAILED(ret)) {
		DEBUG("Could not set proxy blanket.\n");
	}

  DEBUG("*STUtils constructor*\n");
}

nsresult nsSTUtils::Init() {
	DEBUG("nsSTUtils->init\n");
  regNetworkEvent(this);
	return NS_OK;
}

nsSTUtils::~nsSTUtils() {
  if(m_pLink) {
    m_pLink->Release();
  }
	if(m_pSvc) {
		m_pSvc->Release();
	}
	if(m_pLoc) {
		m_pLoc->Release();
	}
}

NS_IMETHODIMP nsSTUtils::SetSENSCallback(nsISENSCallback *aCallback) {
  this->m_OnSENSEvent = aCallback;
  return NS_OK;
}

NS_IMETHODIMP nsSTUtils::SetValToINI(const char *inifile, const char *section, const char *key, const char *val) {

  WritePrivateProfileString(section, key, val, inifile);
  return NS_OK;
}

NS_IMETHODIMP nsSTUtils::GetValFromINI(const char *inifile, const char *section, const char *key, char **val) {
  TCHAR   buf[BUF_SIZE_L0];
  int len;

  GetPrivateProfileString(section, key, "", buf, BUF_SIZE_L0, inifile);
  len = PL_strlen(buf);
  if(len > 0) *val = PL_strndup(buf, len);
  DEBUG("%s[%s] = %s\n", section, key, buf);

  return NS_OK;
}

NS_IMETHODIMP nsSTUtils::GetUsername(char **rv) {
  NET_API_STATUS sta;
  LPWKSTA_USER_INFO_1 pBuf = NULL;

  sta = NetWkstaUserGetInfo(NULL, 1, (LPBYTE *)&pBuf);
  if(sta == NERR_Success) {
    int len = -1;
    char str[BUF_SIZE_L0] = {0};

		WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)pBuf->wkui1_username, len, str, BUF_SIZE_L0, NULL, NULL);
    len = PL_strlen(str);
    if(len > 0) *rv = PL_strndup(str, len);
  } else {
    *rv = NULL;
    DEBUG("NetWkstaGetInfo: get username failed\n");
  }

  if(pBuf)
    NetApiBufferFree(pBuf);

	return NS_OK;
}

NS_IMETHODIMP nsSTUtils::GetDomain(char **rv) {
  NET_API_STATUS sta;
  LPWKSTA_USER_INFO_1 pBuf = NULL;

  sta = NetWkstaUserGetInfo(NULL, 1, (LPBYTE *)&pBuf);
  if(sta == NERR_Success) {
    int len = -1;
    char str[BUF_SIZE_L0] = {0};

		WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)pBuf->wkui1_logon_domain, len, str, BUF_SIZE_L0, NULL, NULL);
    len = PL_strlen(str);
    if(len > 0) *rv = PL_strndup(str, len);
  } else {
    *rv = NULL;
    DEBUG("NetWkstaGetInfo: get domain failed\n");
  }

  if(pBuf)
    NetApiBufferFree(pBuf);

	return NS_OK;
}

NS_IMETHODIMP nsSTUtils::GetSid(char **rv) {
  HANDLE hToken;
  PTOKEN_USER  ptiUser=NULL;
  DWORD        cbti = 0;
  tConvertSidToStringSid pConvertSidToStringSid = NULL;

  if (!OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &hToken )) {
    DEBUG("Get Process Token failed\n");
		return NS_OK;
  }
  if (GetTokenInformation(hToken, TokenUser, NULL, 0, &cbti)) {
    DEBUG("Get Token Information failed\n");
		return NS_OK;
  }
  ptiUser = (PTOKEN_USER) HeapAlloc(GetProcessHeap(), 0, cbti);
  if (!GetTokenInformation(hToken, TokenUser, ptiUser, cbti, &cbti)) {
		return NS_OK;
  }

  HMODULE hDLL = LoadLibrary("advapi32.dll");
  if(hDLL) {
    pConvertSidToStringSid = (tConvertSidToStringSid) GetProcAddress(hDLL, "ConvertSidToStringSidA");
    if(pConvertSidToStringSid) {
      LPSTR SID = NULL;
      int len = 0;
      DEBUG("GetProc ConvertSidToStringSidA success\n");
      pConvertSidToStringSid(ptiUser->User.Sid,  &SID);
      len = PL_strlen(SID);
      if(len > 0) *rv = PL_strndup(SID, len);
      LocalFree((HLOCAL)SID);

      //SID_NAME_USE SidType;
      //DWORD dwSize = BUF_SIZE_L0;
      //char lpName[BUF_SIZE_L0];
      //char lpDomain[BUF_SIZE_L0];
      //LookupAccountSid(NULL, ptiUser->User.Sid, lpName, &dwSize, lpDomain, &dwSize, &SidType);
      //DEBUG("username = %s, domain = %s\n", lpName, lpDomain);
    }
    FreeLibrary(hDLL);
  }

  if(hToken)
    CloseHandle(hToken);
  if(ptiUser)
    HeapFree(GetProcessHeap(), 0, ptiUser);

  return NS_OK;
}

NS_IMETHODIMP nsSTUtils::GetSystemInfo(const char *cmd, const char *prop, char **rv) {
	IEnumWbemClassObject* pEnumerator = NULL;
	HRESULT ret;

	ret = m_pSvc->ExecQuery(bstr_t("WQL"), bstr_t(cmd), WBEM_FLAG_FORWARD_ONLY, 0, &pEnumerator);
	if(FAILED(ret)) {
		DEBUG("ExecQuery failed\n");
	} else {
		IWbemClassObject *pclsObj;
		ULONG uReturn = 0;

		DEBUG("ExecQuery success\n");
		while(pEnumerator) {
			ret = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
			if(0 == uReturn)	break;
			VARIANT vtProp;
			char *val = NULL;
			int   len = 0;

			ret = pclsObj->Get(_com_util::ConvertStringToBSTR(prop), 0, &vtProp, 0, 0);

			val = _com_util::ConvertBSTRToString(vtProp.bstrVal);
			if(val) len = PL_strlen(val);
			if(len > 0)
				*rv = PL_strndup(val, len);
			else
				*rv = NULL;

			VariantClear(&vtProp);
		}
	}

	return NS_OK;
}

NS_IMETHODIMP nsSTUtils::RefreshDesktop() {
  HWND desktop;

  desktop = GetDesktopWindow();

  if(desktop) {
    DEBUG("desktop HWND found\n");
    SetForegroundWindow(desktop);
    PostMessage(desktop, WM_KEYDOWN, VK_F5, 0);
    PostMessage(desktop, WM_KEYUP, VK_F5, 0);
  }

  return NS_OK;
}

NS_IMETHODIMP nsSTUtils::CreateShortCut(const char *exepath, const char *args, const char *lnkpath, const char *iconpath, PRBool *rv) {
  IPersistFile *pPf = NULL;
	HRESULT ret;
  LPITEMIDLIST pidl;
  WORD wsz[BUF_SIZE_L1] = {0};
  TCHAR buf[BUF_SIZE_L1] = {0};

  ret = m_pLink->QueryInterface(IID_IPersistFile, (LPVOID*)&pPf);
  if(FAILED(ret)) {
    *rv = FALSE;
    DEBUG("query interface IID_IPersistFile failed\n");
    return NS_OK;
  }
  //m_pLink->SetDescription("created by XPCOM[@splashtop.com/stutils;1]");
  m_pLink->SetArguments(args);

  ret = m_pLink->SetPath(exepath);
  if(FAILED(ret)) {
    DEBUG("SetPath %s failed\n", exepath);
  }
  GetSystemDirectory(buf, BUF_SIZE_L1);
  DEBUG("iconpath = %s, %s\n", buf, iconpath);
	ret = m_pLink->SetIconLocation(iconpath, 0);
  if(FAILED(ret)) {
    DEBUG("SetIconLocation %s failed\n", iconpath);
  }

  SHGetSpecialFolderLocation(NULL, CSIDL_DESKTOP, &pidl);
  SHGetPathFromIDList(pidl, buf);
  lstrcat(buf, "\\");
  lstrcat(buf, lnkpath);

  MultiByteToWideChar(CP_ACP, 0, buf, -1, (LPWSTR)wsz, MAX_PATH);
  ret = pPf->Save((LPCOLESTR)wsz, TRUE);
  if(FAILED(ret)) {
    DEBUG("IID_IPersistFile->Save failed\n");
  }
  pPf->Release();

	DEBUG("stutils.CreateShortCut: %s\n", exepath);
	return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSTUtils, Init)

	static NS_METHOD nsSTUtilsRegProc (nsIComponentManager *aCompMgr,
			nsIFile *aPath,
			const char *registryLocation,
			const char *componentType,
			const nsModuleComponentInfo *info) {


		DEBUG("nsSTUtilsRegProc\n");
		return NS_OK;
	}

static NS_METHOD nsSTUtilsUnregProc (nsIComponentManager *aCompMgr,
		nsIFile *aPath,
		const char *registryLocation,
		const nsModuleComponentInfo *info) {
	DEBUG("nsSTUtilsUnregProc\n");
	return NS_OK;
}


NS_IMPL_ISUPPORTS1(nsSTUtils, nsISTUtils)

	static const nsModuleComponentInfo components[] = {
		{
			NS_ISTUTILS_CLASSNAME, NS_ISTUTILS_CID, NS_ISTUTILS_CONTRACTID,
			nsSTUtilsConstructor, nsSTUtilsRegProc, nsSTUtilsUnregProc
		}
	};

NS_IMPL_NSGETMODULE(nsSTUtils, components)
