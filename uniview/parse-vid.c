#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <curl/curl.h>
#include <json.h>

#define D(...) fprintf(stdout, "PARSE-VID: "__VA_ARGS__)
#define RD(...) fprintf(stdout, __VA_ARGS__)

#define PARSE_QQ_VID 0
#define JSON_OFFSET 13

typedef enum {
  PV_TYPE_NULL = 0,
  PV_TYPE_YOUKU_VID,
  PV_TYPE_QQ_VID
} pv_type_t;

typedef struct {
  int vt;
  char *url;
} pv_qq_preurl_array_t;

typedef struct {
  char *key;
} pv_qq_vidkey_t;

typedef struct {
  char *fn;
  int pa_size;
  pv_qq_preurl_array_t *pa;
  int vka_size;
  pv_qq_vidkey_t *vka;
} pv_qq_info_t;

typedef struct {
  char *key, *sid;
  char *mixed;
} pv_youku_info_t;

typedef struct {
  CURL *handle;
  json_object *json;
  unsigned char *mem;
  int memsize;
  union {
    pv_qq_info_t qq;
    pv_youku_info_t youku;
  } pv_info;
  pv_type_t type;
} pv_core_t;

typedef struct {
  unsigned char *mem;
  int memsize;
} pv_mem_t;

typedef char * (*parse_vid_func_t)(pv_core_t *pvc, char *vid, const char *url);

static void reset_mem(pv_core_t *pvc) {
  if(pvc->mem) {
    free(pvc->mem);
    pvc->memsize = 0;
    pvc->mem = NULL;
  }
}

static char *_youku_gen_sid() {
  char *sid = calloc(sizeof(unsigned char), 32);
  long r1 = 1098;
  long r2 = 3065;

  snprintf(sid, 32, "%d%d%d", time(NULL), r1, r2);
  RD("sid = %s\n", sid);

  return sid;
}

long _youku_atohex(const char *hexstr) {
  long ret = 0;

  sscanf(hexstr, "%x", &ret);
  return ret;
}

static char *_youku_gen_key(struct json_object *key1, struct json_object *key2) {
  char *key = calloc(sizeof(unsigned char), 64);
  long hkey1 = _youku_atohex(json_object_get_string(key1));
  hkey1 = hkey1 ^ 0xA55AA5A;

  snprintf(key, 64, "%s%x", json_object_get_string(key2), hkey1);
  RD("genkey = %s\n", key);

  return key;
}

static void _youku_gen_fileid(json_object *fid, char *fileid, pv_core_t *this) {
  const char *sfid = json_object_get_string(fid);
  char *ptr = NULL;
  int i = 0;

  //RD("sfid = %s\n", sfid);
  memset(fileid, 0, 256);
  while(*sfid != '\0') {
    int idx;
    char idstr[5] = {0};
    ptr = strchr(sfid, '*');
    if(ptr != NULL) {
      strncpy(idstr, sfid, ptr-sfid);
      //RD("ptr = %x, sfid = %x, idstr = %s\n", ptr, sfid, idstr);
      //RD("%d,%d,%d,%d", idstr[0], idstr[1], idstr[2], idstr[3]);
      sscanf(idstr, "%d", &idx);
      //RD("idx = %d, %c\n", idx, this->mixed[idx]);
      fileid[i++] = this->pv_info.youku.mixed[idx];
      sfid = ptr +1;
    } else {
      break;
    }
  }
}

static void _youku_gen_mixed_string(json_object *seed, pv_core_t *this) {
  const char *seedstring = json_object_get_string(seed);
  char source[] = {"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ/\\:._-1234567890"};
  int nseed, slen = sizeof(source)/sizeof(char), i = 0, idx, cnt;
  this->pv_info.youku.mixed = calloc(sizeof(unsigned char), slen+1);

  sscanf(seedstring, "%d", &nseed);
  for(cnt = slen; i < slen; i++, cnt--) {
    nseed = (nseed * 211 + 30031)%65536;
    idx = ((nseed*1.0)/65536)*(cnt-1);
    this->pv_info.youku.mixed[i] = source[idx];
    memmove(source+idx, source+idx+1, cnt-idx-1);
  }
  RD("mixed = %s\n", this->pv_info.youku.mixed);
}

static void _youku_parse_vid_files(json_object *segs, json_object *sfid, const char *stype, pv_core_t *this) {
  json_object *fid = json_object_object_get(sfid, stype);
  json_object *segsdata = json_object_object_get(segs, stype);
  char fileid[256] = {0};

  if(fid != NULL) {
    int len = json_object_array_length(segsdata), i;
    json_object *seg, *key;

    _youku_gen_fileid(fid, fileid, this);
    //RD("fid = %x, stype = %s, fid = %s, seg = %s\n", fid, stype, json_object_to_json_string(fid), json_object_to_json_string(segsdata));
    //RD("seg len = %d\n", json_object_array_length(seg));
    RD("fileid = %s\n", fileid);
    for(i=0; i<len; i++) {
      char idxhex[3] = {0};
      const char *pathfmt = "http://f.youku.com/player/getFlvPath/sid/%s/st/%s/fileid/%s?K=%s";
      char rvpath[256] = {0};
      char *skey = NULL;

      seg = json_object_array_get_idx(segsdata, i);
      key = json_object_object_get(seg, "k");
      if(key == NULL)
        skey = this->pv_info.youku.key;
      else
        skey = (char *)json_object_get_string(key);
      if(strcmp(skey, "-1") == 0) {
        RD("paid needed for %s video stream\n", stype);
        continue;
      }
      //RD("type = %s, seg[%d].k = %s\n", stype, i, json_object_get_string(key));
      if(i < 16)
        snprintf(idxhex, 3, "0%X", i);
      else
        snprintf(idxhex, 3, "%X", i);
      fileid[8] = idxhex[0];
      fileid[9] = idxhex[1];
      if(strcmp(stype, "mp4") == 0)
        snprintf(rvpath, 256, pathfmt, this->pv_info.youku.sid, "mp4", fileid, skey);
      else
        snprintf(rvpath, 256, pathfmt, this->pv_info.youku.sid, "flv", fileid, skey);
      RD("video.seg[%X]: %s\n", i, rvpath);
    }
  }
}

size_t json_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
  pv_core_t *this = NULL;
  this = (pv_core_t *)userdata;

  this->mem = realloc(this->mem, this->memsize + nmemb*size + 1);
  memcpy(this->mem+this->memsize, ptr, nmemb*size);
  this->memsize += nmemb * size;
  this->mem[this->memsize] = 0;
  //RD("json = %x, ptr = %x, size = %d, nmemb = %d\n", this->json, ptr, size, nmemb);
  return size * nmemb;
}

static void _parse_prefix_uri(struct json_object *vi, pv_core_t *pvc) {
  struct json_object *ul = NULL, *uis = NULL;
  int len = 0, i;

  ul = json_object_object_get(vi, "ul");
  uis = json_object_object_get(ul, "ui");
  len = json_object_array_length(uis);
  pvc->pv_info.qq.pa_size = len;
  pvc->pv_info.qq.pa = calloc(sizeof(pv_qq_preurl_array_t), len);

  for (i=0; i<len; i++) {
    struct json_object *ui = NULL, *url, *vt;
    ui = json_object_array_get_idx(uis, i);
    url = json_object_object_get(ui, "url");
    vt = json_object_object_get(ui, "vt");
    pvc->pv_info.qq.pa[i].vt = json_object_get_int(vt);
    pvc->pv_info.qq.pa[i].url = strdup(json_object_get_string(url));
    RD("url = %s, vt = %d\n", pvc->pv_info.qq.pa[i].url, pvc->pv_info.qq.pa[i].vt);
  }
}

#define GETKEY_URL_FMT "http://vv.video.qq.com/getkey?vid=%s&otype=json&filename=%s&ran=0.623500510584563&charge=0&platform=1&format=10202"
static char * _parse_vid_qq_video_key(char *vid, int i, pv_core_t *pvc) {
  char *key = NULL;
  char getkey_url[256] = {0};
  char filename[256] = {0};
  CURL *hQQ = NULL;
  CURLcode res;

  snprintf(filename, 256, pvc->pv_info.qq.fn, i);
  snprintf(getkey_url, 256, GETKEY_URL_FMT, vid, filename);
  //RD("getkey_url = %s\n", getkey_url);

  hQQ = curl_easy_init();
  curl_easy_setopt(hQQ, CURLOPT_URL, getkey_url);
  curl_easy_setopt(hQQ, CURLOPT_WRITEDATA, pvc);
  curl_easy_setopt(hQQ, CURLOPT_WRITEFUNCTION, json_callback);

  res = curl_easy_perform(hQQ);
  curl_easy_cleanup(hQQ);
  //RD("_parse_vid_qq_video_key, mem = %s\n", pvc->mem);
  pvc->json = json_tokener_parse(pvc->mem+JSON_OFFSET);
  reset_mem(pvc);
  if(pvc->json) {
    struct json_object *key = NULL;
    key = json_object_object_get(pvc->json, "key");
    return strdup(json_object_get_string(key));
  }

  return NULL;
}

static void _parse_vid_qq_video_seg(char *vid, struct json_object *vi, pv_core_t *pvc) {
  struct json_object *cl = NULL, *cis = NULL;
  int len = 0, i;

  cl = json_object_object_get(vi, "cl");
  cis = json_object_object_get(cl, "ci");
  len = json_object_array_length(cis);
  //RD("ci = %s, len = %d\n", json_object_to_json_string(cis), len);
  pvc->pv_info.qq.vka_size = len;
  pvc->pv_info.qq.vka = calloc(sizeof(pv_qq_vidkey_t), len);

  for(i=0; i<len; i++) {
    struct json_object *ci = NULL;
    ci = json_object_array_get_idx(cis, i);
    //RD("ci = %s\n", json_object_to_json_string(ci));
    pvc->pv_info.qq.vka[i].key = _parse_vid_qq_video_key(vid, i+1, pvc);
    RD("vidkey = %s\n", pvc->pv_info.qq.vka[i].key);
  }
}

/**QQ:get_info http://vv.video.qq.com/getinfo?defaultfmt=hd&speed=529&otype=xml&ran=0.E14024054119363427&charge=0&platform=1&vids=8BECgd5s4NN
      get_key  http://vv.video.qq.com/getkey?vid=8BECgd5s4NN&otype=xml&filename=8BECgd5s4NN.p202.1.mp4&ran=0.623500510584563&charge=0&platform=1&format=10202
*/
char *parse_vid_qq(pv_core_t *pvc, char *vid, const char *url) {
  CURL *hQQ = NULL;
  CURLcode res;
  char getinfo_url[256] = {0};
  struct json_object *vdata = NULL;

  pvc->type = PV_TYPE_QQ_VID;
  snprintf(getinfo_url, 256, "http://vv.video.qq.com/getinfo?defaultfmt=hd&speed=1024&otype=json&ran=0.E98764054119363427&charge=0&platform=1&vids=%s", vid);
  hQQ = curl_easy_init();
  curl_easy_setopt(hQQ, CURLOPT_URL, getinfo_url);
  curl_easy_setopt(hQQ, CURLOPT_WRITEDATA, pvc);
  curl_easy_setopt(hQQ, CURLOPT_WRITEFUNCTION, json_callback);

  res = curl_easy_perform(hQQ);
  curl_easy_cleanup(hQQ);
  //RD("\nQQ parse DONE %s\n", pvc->mem);
  pvc->json = json_tokener_parse(pvc->mem+JSON_OFFSET);
  reset_mem(pvc);
  if(pvc->json) {
    struct json_object *vl = NULL, *vis = NULL;
    int cnt = 0, i;
    vl = json_object_object_get(pvc->json, "vl");
    cnt = json_object_get_int(json_object_object_get(vl, "cnt"));
    vis = json_object_object_get(vl, "vi");
    RD("cnt = %d, vi = %s\n", cnt, json_object_to_json_string(vis));
    for(i=0; i<cnt; i++) {
      struct json_object *vi = NULL, *fn = NULL;
      char str[256] = {0}, *ptr = NULL, *pend = NULL;
      char s1[256] = {0};
      vi = json_object_array_get_idx(vis, i);
      fn = json_object_object_get(vi, "fn");
      RD("fn = %s\n", json_object_to_json_string(fn));
      ptr = json_object_get_string(fn);
      pend = strrchr(ptr, '.')+1;
      strncpy(s1, ptr, pend-ptr);
      snprintf(str, 256, "%s%c%c.%s", s1, '%', 'd', pend);
      //RD("s1 = %s, new str = %s, pend = %s\n", s1, str, pend);
      pvc->pv_info.qq.fn = strdup(str);
      _parse_prefix_uri(vi, pvc);
      _parse_vid_qq_video_seg(vid, vi, pvc);
    }
  }

  return NULL;
}

char *parse_vid_youku(pv_core_t *pvc, char *vid, const char *url) {
  char vidsurl[256] = {0};
  CURLcode res;
  struct json_object *vdata = NULL;
  struct json_object *st = NULL, *key1 = NULL, *key2 = NULL;
  struct json_object *seed = NULL, *sfid = NULL;

  pvc->type = PV_TYPE_YOUKU_VID;
  snprintf(vidsurl, 256, "http://v.youku.com/player/getPlayList/VideoIDS/%s", vid);
  RD("parse youku vid = %s, url = %s\n", vid, vidsurl);
  curl_easy_setopt(pvc->handle, CURLOPT_URL, vidsurl);
  curl_easy_setopt(pvc->handle, CURLOPT_WRITEDATA, pvc);
  curl_easy_setopt(pvc->handle, CURLOPT_WRITEFUNCTION, json_callback);
  res = curl_easy_perform(pvc->handle);
  RD("\nYouKu parse DONE\n");

  pvc->json = json_tokener_parse(pvc->mem);
  if(pvc->json) {
    vdata = json_object_object_get(pvc->json, "data");
    RD("data =%s\n", json_object_to_json_string(vdata));
    //RD("type = %x\n", json_object_get_type(vdata));
    if(json_type_array == json_object_get_type(vdata)) {
      vdata = json_object_array_get_idx(vdata, 0);
    }
    key1 = json_object_object_get(vdata, "key1");
    key2 = json_object_object_get(vdata, "key2");
    if(key1 && key2) {
      RD("key1 = %s, key2 = %s, type = %x\n", json_object_to_json_string(key1), json_object_to_json_string(key2), json_object_get_type(key1));
      pvc->pv_info.youku.key = _youku_gen_key(key1, key2);
    }
    st = json_object_object_get(vdata, "streamtypes");
    seed = json_object_object_get(vdata, "seed");
    _youku_gen_mixed_string(seed, pvc);
    pvc->pv_info.youku.sid = _youku_gen_sid();
    RD("st=%s\n", json_object_to_json_string(st));
    sfid = json_object_object_get(vdata, "streamfileids");

    int len = json_object_array_length(st);
    for(int idx = 0; idx <len; idx++) {
      struct json_object *type = json_object_array_get_idx(st, idx);
      const char *stype = json_object_get_string(type);
      struct json_object *segs = json_object_object_get(vdata, "segs");
      _youku_parse_vid_files(segs, sfid, stype, pvc);
    }
  }

  return NULL;
}

#if PARSE_QQ_VID
size_t _get_vqq_index_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
  pv_mem_t *pvmem = (pv_mem_t *)userdata;

  pvmem->mem = realloc(pvmem->mem, pvmem->memsize + nmemb*size + 1);
  memcpy(pvmem->mem+pvmem->memsize, ptr, nmemb*size);
  pvmem->memsize += nmemb * size;
  pvmem->mem[pvmem->memsize] = 0;
  
  return size * nmemb;
}

static void _get_qq_vid(const char *url, char *vid) {
  CURL *hQQ = NULL;
  CURLcode res;
  pv_mem_t pvmem = {0};
  char *svid = NULL;

  hQQ = curl_easy_init();
  curl_easy_setopt(hQQ, CURLOPT_URL, url);
  curl_easy_setopt(hQQ, CURLOPT_WRITEDATA, &pvmem);
  curl_easy_setopt(hQQ, CURLOPT_WRITEFUNCTION, _get_vqq_index_callback);

  res = curl_easy_perform(hQQ);

  svid = strstr(pvmem.mem, "vid:");
  if(svid) {
    RD("vqq = %s\n", svid);
    svid += 5;
    while(*svid != '"') {
      *vid = *svid;
      svid ++, vid ++;
    }
  }
  curl_easy_cleanup(hQQ);
}
#endif

parse_vid_func_t url_prepare(const char *url, char *vid) {
  char *ptr = NULL, *pend = NULL;

  if(url && strlen(url)> 4) {
    int len=strlen(url);
    ptr = strstr(url, "youku.com");
    pend = (char *)(url + len - 5);

    if(ptr) {
      if(strncmp(pend, ".html", 5) == 0) {
        ptr = strstr(ptr, "id_");
        if(ptr) {
           ptr += 3;
           strncpy(vid, ptr, pend-ptr);
        }
      }

      return parse_vid_youku;
    }
    
    ptr = strstr(url, "qq.com");
    if(ptr) {
#if PARSE_QQ_VID
      _get_qq_vid(url, vid);
#else
      if(strncmp(pend, ".html", 5) == 0) {
        ptr = strstr(ptr, "id_");
        if(ptr) {
           ptr += 3;
           strncpy(vid, ptr, pend-ptr);
        }
      }
#endif
      return parse_vid_qq;
    }
  }

  return NULL;
}

void pvc_init(pv_core_t *pvc) {
  curl_global_init(CURL_GLOBAL_ALL);
  pvc->handle = curl_easy_init();
  pvc->mem = NULL;
  pvc->memsize = 0;
  pvc->type = PV_TYPE_NULL;
}

void pvc_cleanup(pv_core_t *pvc) {
  curl_easy_cleanup(pvc->handle);
  curl_global_cleanup();

  if(pvc->type == PV_TYPE_YOUKU_VID) {
    if(pvc->pv_info.youku.key)
      free(pvc->pv_info.youku.key);
    if(pvc->pv_info.youku.sid)
      free(pvc->pv_info.youku.sid);
    if(pvc->pv_info.youku.mixed)
      free(pvc->pv_info.youku.mixed);
  }
  if(pvc->type == PV_TYPE_QQ_VID) {
    if(pvc->pv_info.qq.fn)
      free(pvc->pv_info.qq.fn);
    if(pvc->pv_info.qq.pa) {
      int i =0;
      for(; i<pvc->pv_info.qq.pa_size; i++) {
        if(pvc->pv_info.qq.pa[i].url)
          free(pvc->pv_info.qq.pa[i].url);
      }
      free(pvc->pv_info.qq.pa);
    }
  }
  if(pvc->mem)
    free(pvc->mem);
}

/**QQ: http://v.qq.com/cover/8/8xjgaeijaa7t2wt.html vid:"8BECgd5s4NN", 
   NEW QQ URL: http://v.qq.com/cover/id_8BECgd5s4NN.html*/

int main(int argc, char *argv[]) {
  if(argc == 2) {
    char vid[256] = {0};
    parse_vid_func_t parse_vid;
    pv_core_t pvc = {0};

    parse_vid = url_prepare(argv[1], vid);
    if(parse_vid) {
      pvc_init(&pvc);
      (* parse_vid)(&pvc, vid, argv[1]);
      pvc_cleanup(&pvc);
    }
  } else {
   RD("usage: %s http://v.youku.com/v_show/id_XMzU0Nzg2NzI4.html\n", argv[0]);
  }

  return 0;
}
