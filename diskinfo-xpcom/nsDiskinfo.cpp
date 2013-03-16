#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/vfs.h>
#include "nscore.h"
#include "nsIGenericFactory.h"
#include "nsDiskinfo.h"
#include "nsString.h"
#include "prenv.h"

#define ERROR(...) fprintf(stderr, "[XXX] "__VA_ARGS__)
#define DEBUG(...) fprintf(stderr, "[III] "__VA_ARGS__)

typedef struct {
  PRUint32 key;
  PRExplodedTime date;
} msg_hdr_t;

nsDiskinfo::nsDiskinfo() {
  this->m_MsgList = NULL;
}

nsresult nsDiskinfo::Init() {
  DEBUG("Diskinfo->init\n");
	return NS_OK;
}

static void _msg_hdr_free(gpointer data, gpointer userdata) {
  if(data) {
    g_free(data);
  }
}

nsDiskinfo::~nsDiskinfo() {
  if(this->m_MsgList) {
    g_slist_foreach(this->m_MsgList, _msg_hdr_free, NULL);
  }
}

NS_IMETHODIMP nsDiskinfo::GetEnv(const char *env, char **rv) {
  char *str = NULL;
  //str = getenv(env);
  str = PR_GetEnv(env);
  if(str) {
    *rv = strndup(str, strlen(str));
  } else {
    *rv = NULL;
  }
  DEBUG("env[%s] = %s\n", env, *rv);

  return NS_OK;
}

NS_IMETHODIMP nsDiskinfo::InitCleanup() {

  if(this->m_MsgList) {
    g_slist_foreach(this->m_MsgList, _msg_hdr_free, NULL);
    g_slist_free(this->m_MsgList);
    this->m_MsgList = NULL;
  }
  return NS_OK;
}

NS_IMETHODIMP nsDiskinfo::AddCleanupItem(PRUint32 key, PRTime date) {
  msg_hdr_t *hdr = g_new0(msg_hdr_t, 1);
  hdr->key = key;
  PR_ExplodeTime(date, PR_GMTParameters, &(hdr->date));
  this->m_MsgList = g_slist_append(this->m_MsgList, hdr);

  return NS_OK;
}

static gint _msg_hdr_compare(gconstpointer a, gconstpointer b) {
  int dx = 0;
  msg_hdr_t *hdra = (msg_hdr_t *)a;
  msg_hdr_t *hdrb = (msg_hdr_t *)b;

  if(hdra->date.tm_mday == hdrb->date.tm_mday) {
    if(hdra->date.tm_hour == hdrb->date.tm_hour) {
      if(hdra->date.tm_min == hdrb->date.tm_min) {
        if(hdra->date.tm_sec == hdrb->date.tm_sec) {
          dx = hdra->date.tm_usec - hdrb->date.tm_usec;
        } else {
          dx = hdra->date.tm_sec - hdrb->date.tm_sec;
        }
      } else {
        dx = hdra->date.tm_min - hdrb->date.tm_min;
      }
    } else {
      dx = hdra->date.tm_hour -hdrb->date.tm_hour;
    }
  } else {
    dx = hdra->date.tm_mday - hdrb->date.tm_mday;
  }

  return dx;
}

NS_IMETHODIMP nsDiskinfo::DoCleanup(PRInt32 cnt, nsIMyCleanupCallback *aCallback) {

  if(this->m_MsgList) {
    GSList *item = NULL;
    msg_hdr_t *hdr = NULL;
    PRBool ret;

    this->m_MsgList = g_slist_sort(this->m_MsgList, _msg_hdr_compare);
    for(item = this->m_MsgList; cnt && item; item=g_slist_next(item)) {
      hdr = (msg_hdr_t *)(item->data);
      if(hdr && aCallback) {
        aCallback->Call(0, hdr->key, &ret);
      }
      cnt--;
    }
  }
  return NS_OK;
}

#define ONE_MB (1024*1024)
NS_IMETHODIMP nsDiskinfo::GetMountPointFreeInfo(const PRUnichar *mntpath, PRUint32 *totalMB, PRInt32 *rv) {
  struct statfs fsd;
  const char *mypath = NS_LossyConvertUTF16toASCII(mntpath).get();

  DEBUG("GetMountPointUsedInfo = %s\n", mypath);

  if(statfs(mypath, &fsd) < 0) {
    DEBUG("get file system statistics failed\n");
    *rv = 0;
    *totalMB = 0;
  } else {
    double total =   1.00*fsd.f_blocks;
    double avail = 100.00*fsd.f_bavail;
    PRInt32 val = (avail/total + 0.500);
    if(fsd.f_blocks < ONE_MB) {
      *totalMB = ((fsd.f_blocks*fsd.f_bsize)/ONE_MB);
    } else {
      *totalMB = ((fsd.f_blocks/ONE_MB) * fsd.f_bsize);
    }
    //DEBUG("free disk precent = %f, bsize=%ld, total=%ld EQ (df -B %ld), %dMB\n", avail/total, fsd.f_bsize, fsd.f_blocks, fsd.f_bsize, *totalMB);
    *rv = val;
  }

	return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsDiskinfo, Init)

static NS_METHOD nsDiskinfoRegProc (nsIComponentManager *aCompMgr,
                                  nsIFile *aPath,
                                  const char *registryLocation,
                                  const char *componentType,
                                  const nsModuleComponentInfo *info) {


	DEBUG("nsDiskinfoRegProc\n");
    return NS_OK;
}

static NS_METHOD nsDiskinfoUnregProc (nsIComponentManager *aCompMgr,
                                    nsIFile *aPath,
                                    const char *registryLocation,
                                    const nsModuleComponentInfo *info) {
	DEBUG("nsDiskinfoUnregProc\n");
    return NS_OK;
}


NS_IMPL_ISUPPORTS1(nsDiskinfo, nsIDiskinfo)

static const nsModuleComponentInfo components[] = {
	{
		NS_IDISKINFO_CLASSNAME, NS_IDISKINFO_CID, NS_IDISKINFO_CONTRACTID,
		nsDiskinfoConstructor, nsDiskinfoRegProc, nsDiskinfoUnregProc
	}
};

NS_IMPL_NSGETMODULE(nsDiskinfo, components)
