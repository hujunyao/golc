/**gcc -g -o post_uploader post_uploader.c -levent -ljson-c -lcurl*/
#include <netinet/in.h>  /* sockaddr_in, sockaddr_in6 */
#include <stddef.h>      /* size_t, ssize_t */
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>  /* sockaddr */
#include <sys/types.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <dirent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/event_struct.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <json-c/json.h>

#define DD(...) fprintf(stderr, __VA_ARGS__)
#define POST_THREAD_MAX 4
#define DEFAULT_CACHE_DIR "/sdcard/zmcash_files/zm_cache"
#define QUICK_MSG "亲,请慢点操作,反应不过来了!"
#define POST_UPLOADER_VERSION "1.0a"
#define RETRY_SLEEP_SECOND 30
#define RETRY_MAX 5
#define EID_CONFIG_XML "/data/data/com.zmsoft.eatery.cashdesk.activity/shared_prefs/CashDesk.xml"
#define SERVER_HEALTH_CHECK "http://api.2dhuo.com/check_health"

#define DD_L5 5
#define DD_L4 4
#define DD_L3 3
#define DD_L2 2
#define DD_L1 1
#define DD_L0 0

typedef struct {
  int datalen;
  FILE *fp;
} app_data_t;
 
typedef struct {
  int pidx;
  char *resp;
} post_thread_data_t;

typedef struct appdata_list_s {
  app_data_t* data;
  int is_cached;
  struct appdata_list_s* pnext;
} appdata_list_t;

typedef struct appdata_retry_list_item_s {
  app_data_t* data;
  time_t ctime;
  int nsec;
  int retry_cnt;
  struct appdata_retry_list_item_s *pnext;
} appdata_retry_list_item_t;

typedef struct {
  struct event_base *base;
  struct evconnlistener *listener;
  pthread_t ptarray[POST_THREAD_MAX];
  appdata_list_t* end;
  appdata_list_t* hdr;

  appdata_retry_list_item_t* retry_end;
  appdata_retry_list_item_t* retry_hdr;
} post_uploader_t;

static post_uploader_t gpu;
static int g_appdata_cnt = 0;
static char* g_cache_dir = DEFAULT_CACHE_DIR;
static int g_port = 7777;
static int g_post_cnt = 0;
static int g_retry_list_count = 0;
static int g_born_time = 0;
static int g_is_debug = 0;
static int g_failed_cnt = 0;
static int g_net_status = 1;
static struct event g_timer;
static char g_cache_real_dir[1024] = {0};
static char g_eid[32] = {0};
static  pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
static  pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static  pthread_mutex_t g_retry_mutex = PTHREAD_MUTEX_INITIALIZER;

static void post_upload_add_data(post_uploader_t* u, app_data_t* data, int is_cached);

static unsigned int str2hex(char* str) {
  unsigned int ret = 0;

  while(*str != '.' && *str != '\0') {
    unsigned char v = 0;
    char c = *str;
    ret = ret << 4;
    if(c >= '0' && c <= '9')
      v = c - '0';
    if(c >= 'a' && c <= 'f')
      v = c - 'a' + 10;
    if(c >= 'A' && c <= 'F')
      v = c - 'A' + 10;

    ret |= v;
    str++;
  }
  if(strcmp(str, ".txt") != 0) {
    return -1;
  }

  return ret;
}

static void get_eid(char* eid) {
  struct stat fi;
  FILE* fp = fopen(EID_CONFIG_XML, "r");
  if(fp == NULL) {
    fp = fopen("CashDesk.xml", "r");
  }

  if(fp != NULL) {
    char* tmpbuf = NULL;

    fstat(fileno(fp), &fi);
    tmpbuf = calloc(fi.st_size, sizeof(char));
    if(tmpbuf) {
      char* s = NULL, *e = NULL;
      fread(tmpbuf, sizeof(char), fi.st_size, fp);
      s = strstr(tmpbuf, "\"eId\">") + 6;
      if(s != NULL) {
        e = strchr(s, '<');
      }
      if(e && s) {
        if(g_is_debug > DD_L5)
          DD(" s = %s, e = %s\n", s, e);
        strncpy(eid, s, e - s);
      }
      free(tmpbuf);
    }
    fclose(fp);
  }
}

static void check_network_status() {
  CURL *curl = NULL;
  CURLcode res;
  int new_status = 1, idx = 0;

  for(; idx < 3; idx++) {
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, SERVER_HEALTH_CHECK);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
      DD("check_network_status failed: %s, will try it again\n", curl_easy_strerror(res));
      new_status = 0;
    } else {
      break;
    }
    curl_easy_cleanup(curl);
  }

  if(new_status != g_net_status) {
    pthread_mutex_lock(&g_mutex);
    g_net_status = new_status;
    if(new_status == 1) {
      DD("network is available, signal g_cond\n");
      pthread_cond_signal(&g_cond);
    }
    pthread_mutex_unlock(&g_mutex);
  }
}

static void init_cache_dir() {
  DIR *dir = opendir(g_cache_real_dir);
  struct dirent *dent = NULL;

  if(dir != NULL) {
    while((dent = readdir(dir)) != NULL) {
      char cachefn[1024] = {0};
      struct stat info;
      unsigned int ptr = 0;

      if(dent->d_name[0] == '.')
        continue;

      snprintf(cachefn, 1024, "%s/%s", g_cache_real_dir, dent->d_name);
      stat(cachefn, &info);
      if(S_ISDIR(info.st_mode))
        continue;

      if(g_is_debug > DD_L5)
        DD("init_cache_dir: fn = %s\n", dent->d_name);

      ptr = str2hex(dent->d_name);
      //DD("ptr = %x\n", ptr);
      if(ptr != -1) {
        post_upload_add_data(&gpu, (app_data_t *)ptr, 1);
      }
    }
    closedir(dir);
  }
}

static app_data_t* post_upload_get_data(post_uploader_t* u, int pidx, int *is_cached) {
  app_data_t* appdata = NULL;

  pthread_mutex_lock(&g_mutex);
  while(g_appdata_cnt == 0 || g_net_status == 0) {
    if(g_is_debug > DD_L0)
      DD("pthread_cond_wait g_appdata_cnt = %d, pidx = %d, g_net_status = %d\n", g_appdata_cnt, pidx, g_net_status);
    pthread_cond_wait(&g_cond, &g_mutex);
  }
  if(g_appdata_cnt) {
    //DD("u = %x, u->hdr = %x\n", u, u->hdr);
    appdata = u->hdr->data;
    *is_cached = u->hdr->is_cached;

    if(g_appdata_cnt > 0) {
      u->hdr = u->hdr->pnext;
    } else {
      u->hdr = u->end = NULL;
    }
    g_appdata_cnt --;
  }
  DD("get appdata %p for thread.%d, g_appdata_cnt = %d\n", appdata, pidx, g_appdata_cnt);
  pthread_mutex_unlock(&g_mutex);

  return appdata;
}

static void post_upload_add_data(post_uploader_t* u, app_data_t* data, int is_cached) {
  appdata_list_t *item = calloc(1, sizeof(appdata_list_t));

  item->data = data;
  item->is_cached = is_cached;

  pthread_mutex_lock(&g_mutex);
  if(u->end != NULL) {
    u->end->pnext = item;
  }

  u->end = item;
  if(u->hdr == NULL) {
    u->hdr = item;
  }
  g_appdata_cnt ++;
  DD("add appdata %p, g_appdata_cnt = %d, is_cached = %d\n", data, g_appdata_cnt, is_cached);
  if(g_appdata_cnt == 1)
    pthread_cond_signal(&g_cond);

  pthread_mutex_unlock(&g_mutex);
}

static void post_upload_add_retry_list(post_uploader_t* u, app_data_t* data) {
  appdata_retry_list_item_t* item = NULL, * free_item = NULL;

  g_failed_cnt ++;
  for(item = u->retry_hdr; item != NULL; item = item->pnext) {
    if(!free_item && item->nsec < 0) {
      free_item = item;
    }
    if(data == item->data)
      break;
  }

  DD("post_upload_add_retry_list appdata = %p, item = %p, free_item = %p\n", data, item, free_item);
  if(!item) {
    if(!free_item) {
      item = calloc(1, sizeof(appdata_retry_list_item_t));

      pthread_mutex_lock(&g_retry_mutex);
      if(u->retry_end != NULL) {
        u->retry_end->pnext = item;
      }

      u->retry_end = item;
      if(u->retry_hdr == NULL) {
        u->retry_hdr = item;
      }
      pthread_mutex_unlock(&g_retry_mutex);

      g_retry_list_count ++;
    } else {
      item = free_item;
    }

    item->data = data;
    item->ctime = time(NULL);
    item->nsec = 0;
    item->retry_cnt = 0;
  } else {
    item->retry_cnt++;
    if(item->retry_cnt > RETRY_MAX) {
      char fnpath[1024] = {0};

      snprintf(fnpath, 1024, "%s/%p.txt", g_cache_real_dir, item->data);
      DD("appdata %p retry %d times %dsec, report to log server\n", item->data, item->retry_cnt, item->nsec);
      //report_failed_upload(fnpath);
      unlink(fnpath);

      free(item->data);
      item->nsec = -1;
      item->data = NULL;
      /**TODO*/
    } else {
      item->nsec = item->nsec + item->retry_cnt * RETRY_SLEEP_SECOND;
    }
  }
}

static void post_upload_do_retry(post_uploader_t* u) {
  time_t now = time(NULL);
  appdata_retry_list_item_t* item = NULL, * free_item = NULL;

  pthread_mutex_lock(&g_retry_mutex);
  for(item = u->retry_hdr; item != NULL; item = item->pnext) {
    if(now - item->ctime > item->nsec) {
      DD("appdata %p retry again\n", item->data);
      post_upload_add_data(&gpu, item->data, 0);
    }
  }
  pthread_mutex_unlock(&g_retry_mutex);
}

static void bev_read_callback(struct bufferevent *bev, void *data) {
  struct evbuffer *in = bufferevent_get_input(bev);
  int buflen = evbuffer_get_length(in);
  app_data_t *appdata = (app_data_t *)data;
  int nout = 0;

  //DD("bev_read.evbuffer length = %d\n", buflen);
  if(appdata->datalen > 0) {
    evbuffer_write(in, fileno(appdata->fp));
    appdata->datalen -= buflen;
    //DD("write %d bytes data to file, datalen = %d \n", buflen, appdata->datalen);
    if(appdata->datalen == 0) {
      //bufferevent_write(bev, "OK", 2);
      bufferevent_write(bev, "{\"success\":true}", 16);
    } else if(appdata->datalen < 0) {
      bufferevent_write(bev, "ERR", 3);
    }
  } else if(appdata->datalen == -1) {
    char indata[32] = {0};
    char fname[1024] = {0};

    evbuffer_remove(in, indata, sizeof(indata));
    if(strncmp(indata, ".quit", 5) == 0) {
      event_base_loopexit(bufferevent_get_base(bev), NULL);
    } else if(strncmp(indata, ".debug", 6) == 0) {
      g_is_debug = !g_is_debug;
      if(indata[6] >= '0' && indata[0] < '9') {
        g_is_debug = indata[6] - '0';
      } else {
        g_is_debug = 1;
      }
      DD("set g_is_debug = %d\n", g_is_debug);
    } else {
      if(indata[0] >='0' && indata[0] <= '9') {
        snprintf(fname, 1024, "%s/%p.txt", g_cache_real_dir, appdata);
        appdata->fp = fopen(fname, "w");
        DD("fp = %p, fname = %s, indata = %s\n", appdata->fp, fname, indata);
        appdata->datalen = atoi(indata);
        bufferevent_write(bev, "{\"success\":true}", 16);
      } else {
        //bufferevent_write(bev, "invalid command\n", 16);
        bufferevent_free(bev);
      }
    }
  }
}

static void bev_write_callback(struct bufferevent *bev, void *data) {
  struct evbuffer *output = bufferevent_get_output(bev);
  int buflen = evbuffer_get_length(output);
  app_data_t *appdata = (app_data_t *)data;
  int nout = 0;

  //DD("bev_write.evbuffer length = %d\n", buflen);
  if(buflen == 0) {
    //bufferevent_free(bev);
  }
}

static void bev_event_callback(struct bufferevent *bev, short events, void *user_data) {
  app_data_t *appdata = (app_data_t *)user_data;

  if(events & BEV_EVENT_EOF) {
    //DD("BEV_EVENT_EOF bev_event_callback\n");
  } else if(events & BEV_EVENT_ERROR) {
    DD("Got an error on connection :%s\n", strerror(errno));
  }

  if(appdata->fp != NULL) {
    DD("close fp %p\n", appdata->fp);
    fclose(appdata->fp);
    post_upload_add_data(&gpu, appdata, 0);
    bufferevent_free(bev);
    appdata->fp = NULL;
  }
}

static void new_conn_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *sa, int socklen, void *user_data) {
  post_uploader_t* u = (post_uploader_t *)user_data;
  struct bufferevent *bev;
  FILE *fp = NULL;
  app_data_t *appdata = NULL;

  bev = bufferevent_socket_new(u->base, fd, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
  appdata = calloc(1, sizeof(app_data_t));
  appdata->datalen = -1;
  bufferevent_setcb(bev, bev_read_callback, NULL, bev_event_callback, appdata);
  bufferevent_enable(bev, EV_WRITE|EV_READ);
}

void post_uploader_startup(post_uploader_t* u) {
  struct sockaddr_in sin;

  u->base = event_base_new();
  memset(&sin, 0, sizeof(struct sockaddr_in));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(g_port);
  //sin->sin_addr.s_addr = htonl(0x7f000001);

  u->listener = evconnlistener_new_bind(u->base, new_conn_cb, u, LEV_OPT_CLOSE_ON_EXEC|LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1, (struct sockaddr *)&sin, sizeof(sin));
  if(u->listener == NULL) {
    DD("create listener @ %d failed\n", g_port);
  }
}

void post_uploader_run(post_uploader_t *u) {
  if(u->base) {
    event_base_dispatch(u->base);
  }
}

void post_uploader_term(post_uploader_t *u) {
  if(u->listener) {
    evconnlistener_free(u->listener);
  }
  if(u->base) {
    event_base_free(u->base);
  }
}

static void evtimer_callback(evutil_socket_t fd, short event, void *arg) {
  struct timeval tv;

  evutil_timerclear(&tv);
  tv.tv_sec = 30;
  event_add(&g_timer, &tv);
  //post_upload_add_data(&gpu, (app_data_t *)&tv);
  DD("STATUS REPORT: g_appdata_cnt = %d, g_post_cnt = %d, alive_time = %dsec, g_retry_list_count = %d\n", g_appdata_cnt, g_post_cnt, time(NULL) - g_born_time, g_retry_list_count);
  post_upload_do_retry(&gpu);
}

int get_resp_result(char* resp) {
  json_object *json = NULL;
  json_object *obj = NULL;
  int code = 0, quick_msg = 0;

  json = json_tokener_parse(resp);
  json_object_object_get_ex(json, "code", &obj);
  if(obj) {
    code = json_object_get_int(obj);
    json_object_put(obj);
    json_object_object_get_ex(json, "message", &obj);
    if(strcmp(json_object_get_string(obj), QUICK_MSG) == 0)
      quick_msg = 1;
    json_object_put(obj);
  }
  if(quick_msg || code == 1) {
    return 0;
  } else if(code == 0) {
    return -1;
  }
  return -2;
}

static int curl_verbose_dump(CURL* handle, curl_infotype type, char* data, size_t size, void *userp) {
  switch(type) {
  case CURLINFO_TEXT:
    if(g_is_debug > DD_L5)
      DD("* %s", data);
    break;
  case CURLINFO_DATA_OUT:
  case CURLINFO_HEADER_OUT:
    if(g_is_debug > DD_L5)
      DD("--> %s", data);
    break;
  case CURLINFO_SSL_DATA_OUT:
    if(g_is_debug > DD_L5)
      DD("(SSL)--> %s", data);
    break;
  case CURLINFO_DATA_IN:
  case CURLINFO_HEADER_IN:
    if(g_is_debug > DD_L5)
      DD("<-- %s", data);
    break;
  case CURLINFO_SSL_DATA_IN:
    if(g_is_debug > DD_L5)
      DD("(SSL)<-- %s", data);
    break;
  }
  return 0;
}

CURLcode do_post_upload(CURL *curl, app_data_t* appdata, int pidx) {
  char fn[1024] = {0};
  CURLcode err = CURLE_OBSOLETE;
  struct evbuffer *evb = evbuffer_new();
  if(evb != NULL) {
    FILE *fp = NULL;

    snprintf(fn, 1024, "%s/%p.txt", g_cache_real_dir, appdata); 
    fp = fopen(fn, "r");
    if(fp != NULL) {
      struct stat fi;
      char *url = NULL;
      size_t nout;
      fstat(fileno(fp), &fi);
      evbuffer_add_file(evb, fileno(fp), 0, fi.st_size);
      DD("fi.st_size = %zu, %s \n", fi.st_size, fn);
      url = evbuffer_readln(evb, &nout, EVBUFFER_EOL_ANY);
      DD("url = %s, evlen = %d\n", url, evbuffer_get_length(evb));
      if(url == NULL || strncmp(url, "http", 4) != 0) {
        fclose(fp);
      } else {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, evbuffer_get_length(evb));
        curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, evbuffer_pullup(evb, -1));
        if(g_is_debug) {
          curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
          curl_easy_setopt(curl, CURLOPT_DEBUGDATA, appdata);
          curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, curl_verbose_dump);
        }
        err = curl_easy_perform(curl);
        g_post_cnt++;
      }
    }
    evbuffer_free(evb);
  }
  return err;
}

static size_t curl_resp_callback(char* in, size_t size, size_t nmemb, void *userdata) {
  post_thread_data_t* ptdobj = (post_thread_data_t *)userdata;

  if(ptdobj) {
    ptdobj->resp = calloc(nmemb+1, size);
    strncpy(ptdobj->resp, in, nmemb*size);
    //DD("pidx = %d, in = %s, size = %zu, nmemb = %d\n", ptdobj->pidx, in, size, nmemb);
  }
  return size * nmemb;
}

void* post_thread_func(void* data) {
  int pidx = (int)data;
  app_data_t *appdata = NULL;
  char *resp = NULL;
  CURL *curl = NULL;
  CURLcode res;
  post_thread_data_t ptd = {0};

  ptd.pidx = pidx;
  while(data) {
    int is_cached = 0;
    curl = curl_easy_init();
    appdata = post_upload_get_data(&gpu, pidx, &is_cached);
    DD("post_thread_func pidx = %d, appdata = %p\n", pidx, appdata);

    if(appdata && curl) {
      char fnpath[1024] = {0};
      int sta = 0xff;

      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_resp_callback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ptd);
      
      res = do_post_upload(curl, appdata, pidx);
      if(ptd.resp) {
        sta = get_resp_result(ptd.resp);
      } else if(res != CURLE_OK) {
        DD("post failed: %s, will try it again\n", curl_easy_strerror(res));
        sta = -2;
      }
      DD("thread.%d resp = %s, sta = %d\n", pidx, ptd.resp, sta);
      switch(sta) {
        case 0:
          snprintf(fnpath, 1024, "%s/%p.txt", g_cache_real_dir, appdata);
          if(! is_cached) {
            free(appdata);
          }
          unlink(fnpath);
          break;

        case -1:
          break;

        case -2:
          //post_upload_add_data(&gpu, appdata, 0);
          post_upload_add_retry_list(&gpu, appdata);
          if(g_failed_cnt > 3) {
            check_network_status();
            g_failed_cnt = 0;
          }
          break;
      }
      if(ptd.resp != NULL) {
        free(ptd.resp);
        ptd.resp = NULL;
      }
    }
    curl_easy_cleanup(curl);
  }
  return NULL;
}

static void print_usage() {
}

int main(int argc, char *argv[]) {
  struct sockaddr_in addr;
  struct timeval tv;
  int idx = 1, ret, opt;
  CURLcode res;
  char* ptr = NULL;
  char lastchar = '\0';

  if(g_is_debug > DD_L0)
    DD("post_uploader v%s  Build %s, %s\n", POST_UPLOADER_VERSION, __TIME__, __DATE__);

  while((opt = getopt(argc, argv, "hdp:c:")) != -1) {
    switch(opt) {
      case 'v':
      case 'd':
      g_is_debug = 1;
      break;

      case 'p':
      g_port = atoi(optarg);
      break;

      case 'c':
      ptr = g_cache_dir = optarg;
      DD("optarg = %s\n", optarg);
      break;

      case 'h':
      print_usage();
      return 0;
    }
  }
  g_born_time = time(NULL);

  if(ptr) {
    while(*ptr != '\0') {
      lastchar = *ptr++;
    }
  }

  do {
    struct stat tmpinfo;
    int ret = 0;

    memset(g_cache_real_dir, 0, 1024);
    if(lastchar == '/')
      snprintf(g_cache_real_dir, 1024, "%s%d", g_cache_dir, g_born_time);
    else
      snprintf(g_cache_real_dir, 1024, "%s/%d", g_cache_dir, g_born_time);

    ret = stat(g_cache_real_dir, &tmpinfo);
    if(ret == -1) {
      mkdir(g_cache_real_dir, 0777);
      break;
    }
    g_born_time += 1;
  } while(1);

  get_eid(g_eid);

  if(g_is_debug > DD_L0)
    DD("post_uploader  port: %d cache_dir: %s, eId: %s\n", g_port, g_cache_real_dir, g_eid);

  res = curl_global_init(CURL_GLOBAL_DEFAULT);
  if(res != CURLE_OK) {
    DD("curl_global_init failed: %s\n", curl_easy_strerror(res));
    return -1;
  }

  post_uploader_startup(&gpu);

  event_assign(&g_timer, gpu.base, -1, EV_PERSIST, evtimer_callback, &gpu);
  evutil_timerclear(&tv);
  tv.tv_sec = 30;
  event_add(&g_timer, &tv);

  ret = pthread_cond_init(&g_cond, NULL);
  if(ret != 0) {
    DD("pthread_cond_init failed\n");
    return -1;
  }

  if(pthread_mutex_init(&g_mutex, NULL) != 0) {
    DD("pthread_mutex_init failed\n");
    return -1;
  }

  //check_network_status();
  for(; idx <= POST_THREAD_MAX; idx++) {
    pthread_create(gpu.ptarray+idx, NULL, post_thread_func, (void *)idx);
  }

  init_cache_dir();

  post_uploader_run(&gpu);

  pthread_cond_destroy(&g_cond);

  post_uploader_term(&gpu);

  return 0;
}
