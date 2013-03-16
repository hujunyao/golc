#include <signal.h>
#include <glib.h>
#include <libsoup/soup-address.h>
#include <libsoup/soup-message.h>
#include <libsoup/soup-server.h>
#include <libsoup/soup-session-async.h>

#undef _POSIX_C_SOURCE
#include <Python.h>

#define DEBUG(...) fprintf(stdout, "[webbridge] "__VA_ARGS__)
#define ERROR(...) fprintf(stderr, "[WEBBRIDGE] "__VA_ARGS__)
#define DD(...) fprintf(stdout, __VA_ARGS__)

#define PATH_SIZE 1024
#define CFGFILE   ".webbridge.cfg"
#define EMBED_PYTHON_PATH "./"

typedef struct {
  SoupServer *server;
  SoupSession *session;
  GSList *walist;
  GKeyFile *kf;
} webbridge_t;

typedef struct {
  gchar *host;
  gchar *root;
} waprop_t;

typedef struct {
  SoupMessage *msg, *newmsg;
  webbridge_t *wbr;
} proxypriv_t;

static GMainLoop *g_loop;

static void priv_copy_msghdr (const char *name, const char *value, gpointer dest_headers) {
  soup_message_headers_append (dest_headers, name, value);
}

static void priv_send_headers (SoupMessage *from, proxypriv_t *pp) {
  SoupMessage *to = pp->msg;
  webbridge_t *wbr = pp->wbr;

  DEBUG("[%p] HTTP/1.%d %d %s\n", to, soup_message_get_http_version (from), from->status_code, from->reason_phrase);

  soup_message_set_status_full (to, from->status_code, from->reason_phrase);
  soup_message_headers_foreach (from->response_headers, priv_copy_msghdr, to->response_headers);
  soup_message_headers_remove (to->response_headers, "Content-Length");
  soup_server_unpause_message (wbr->server, to);
}

static void priv_send_chunk (SoupMessage *from, SoupBuffer *chunk, proxypriv_t *pp) {
  SoupMessage *to = pp->msg;
  webbridge_t *wbr = pp->wbr;

  DEBUG("[%p]   writing chunk of %lu bytes\n", to, (unsigned long)chunk->length);

  soup_message_body_append_buffer (to->response_body, chunk);
  soup_server_unpause_message (wbr->server, to);
}

static void priv_client_msg_failed (SoupMessage *msg, proxypriv_t *pp) {
  SoupMessage *newmsg = pp->newmsg;
  webbridge_t *wbr = pp->wbr;

  soup_session_cancel_message (wbr->session, newmsg, SOUP_STATUS_IO_ERROR);
}

static void priv_finish_msg (SoupSession *session, SoupMessage *newmsg, gpointer data) {
  proxypriv_t *pp = (proxypriv_t *)data;
  SoupMessage *msg = pp->msg;
  webbridge_t *wbr = pp->wbr;

  g_signal_handlers_disconnect_by_func (msg, priv_client_msg_failed, pp);
  DEBUG("[%p]   done\n\n", msg);

  soup_message_body_complete (msg->response_body);
  soup_server_unpause_message (wbr->server, msg);
  g_object_unref (msg);
  g_slice_free1(sizeof(proxypriv_t), pp);
}

static gboolean priv_check_is_webapp(webbridge_t *wb, const gchar *host, gchar **root) {
  gboolean ret = FALSE;
  GSList *ptr = wb->walist;
  waprop_t *prop = NULL;
  gint len = 0;

  if(host) len = strlen(host);

  if(len > 0) {
    while(ptr) {
      if(ptr->data) {
        prop = (waprop_t *)ptr->data;
        if(prop->host && g_ascii_strncasecmp(prop->host, host, len)==0) {
          if(root) *root = prop->root;
          ret = TRUE;
          break;
        }
      }
      ptr = g_slist_next(ptr);
    }
  }

  return ret;
}

static void priv_run_web_app(PyObject *pymod, SoupMessage *msg, const char *path, webbridge_t *wbr) {
  PyObject *pyhtml = NULL, *class = NULL;

  class = PyObject_GetAttrString(pymod, "webapp");
  if(class) {
    PyObject *cargs = NULL, *obj = NULL, *mrun = NULL;

    cargs = Py_BuildValue("(s)", path);
    obj = PyObject_CallObject(class, cargs);
    if(obj) {
      mrun = PyObject_GetAttrString(obj, "run");
      if(mrun) {
        PyObject *margs = NULL;

        margs = Py_BuildValue("()");
        pyhtml = PyObject_CallObject(mrun, margs);
        Py_DECREF(margs);
        Py_DECREF(mrun);
      } else {
        ERROR("call webapp.run failed\n");
        PyErr_Print();
      }
      Py_DECREF(obj);
    } else {
      ERROR("initialize class webapp failed\n");
      PyErr_Print();
    }
  } else {
    ERROR("get class webapp failed\n");
    PyErr_Print();
  }

  if(pyhtml) {
    gchar *str = NULL;
    GString *resp = NULL;

    //_PyObject_Dump(pyhtml);
    PyArg_Parse(pyhtml, "s", &str);
    if(str) {
      resp = g_string_new(str);
      soup_message_set_response(msg, "text/html", SOUP_MEMORY_TAKE, resp->str, resp->len);
      soup_message_set_status (msg, SOUP_STATUS_OK);
      g_string_free (resp, FALSE);
    }
    Py_DECREF(pyhtml);
  }

  if(PyErr_Occurred())
    PyErr_Print();
}

static void wb_core_cb (SoupServer *server, SoupMessage *msg, const char *path, GHashTable *query, SoupClientContext *cxt, gpointer data) {
  webbridge_t *wbr = (webbridge_t *)data;
  gchar *uristr = NULL, *root = NULL;
  const gchar *host = NULL;

  uristr = soup_uri_to_string (soup_message_get_uri (msg), FALSE);
  host = soup_message_headers_get(msg->request_headers, "Host");

  if (msg->method == SOUP_METHOD_CONNECT) {
    ERROR("CONNECT http method isn't implemented!\n");
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
    return;
  }

  if(priv_check_is_webapp(wbr, host, &root)) {
    PyObject *pymod = NULL;
    gchar modpath[PATH_SIZE] = {0};

    snprintf(modpath, PATH_SIZE, "%s%s", root, path);

    if(g_file_test(modpath, G_FILE_TEST_IS_REGULAR)) {
      gchar *buf;
      GError *err = NULL;
      gsize len;

      DEBUG("GET file %s\n", modpath);
      if(g_file_get_contents(modpath, &buf, &len, &err)) {
        soup_message_body_append (msg->response_body, SOUP_MEMORY_TAKE, buf, len);
      } else {
        soup_message_set_status (msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
      }
    } else {
      pymod = PyImport_ImportModule(modpath);
      if(pymod == NULL) {
        PyErr_Print();
        pymod = PyImport_ImportModule(root);
        DEBUG("import python module %s\n", root);
      } else {
        DEBUG("import python module %s\n", modpath);
      }

      DEBUG("WEBAPP %s, path = %s, root = %s, pymod = %x\n", host, path, root, pymod);
      if(pymod) {
        priv_run_web_app(pymod, msg, path, wbr);
        Py_DECREF(pymod);
      } else {
        soup_message_set_status (msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
        PyErr_Print();
      }
    }
  } else {
    proxypriv_t *pp = NULL;
    SoupMessage *newmsg = NULL;

    pp = g_slice_alloc0(sizeof(proxypriv_t));
    newmsg = soup_message_new (msg->method, uristr);

    pp->msg = msg;
    pp->wbr = wbr;
    pp->newmsg = newmsg;

    soup_message_headers_foreach (msg->request_headers, priv_copy_msghdr, newmsg->request_headers);
    soup_message_headers_remove (newmsg->request_headers, "Host");
    soup_message_headers_remove (newmsg->request_headers, "Connection");

    if (msg->request_body->length) {
      SoupBuffer *request = soup_message_body_flatten (msg->request_body);
      soup_message_body_append_buffer (newmsg->request_body, request);
      soup_buffer_free (request);
    }
    soup_message_headers_set_encoding (msg->response_headers, SOUP_ENCODING_CHUNKED);

    g_signal_connect (newmsg, "got_headers", G_CALLBACK (priv_send_headers), pp);
    g_signal_connect (newmsg, "got_chunk", G_CALLBACK (priv_send_chunk), pp);
    g_signal_connect (msg, "finished", G_CALLBACK (priv_client_msg_failed), pp);

    soup_session_queue_message (wbr->session, newmsg, priv_finish_msg, pp);

    g_object_ref (msg);
    soup_server_pause_message (wbr->server, msg);
  }
}

static void wb_core_term(webbridge_t *wbr) {
  if(wbr->kf) {
    g_key_file_free(wbr->kf);
  }
}

static gboolean wb_core_init(webbridge_t *wbr, gint port) {
  wbr->server = soup_server_new (SOUP_SERVER_PORT, port, NULL);
  if (!wbr->server) {
    ERROR("Unable to bind to server port %d\n", port);
    return FALSE;
  }
  soup_server_add_handler (wbr->server, NULL, wb_core_cb, wbr, NULL);
  soup_server_run_async (wbr->server);
  
  wbr->session = soup_session_async_new ();
  wbr->kf = g_key_file_new();

  return TRUE;
}

static void wb_app_quit (int sig) {
  g_main_loop_quit(g_loop);
}

static void wb_config_init(webbridge_t *wbr, const gchar *file) {
  gchar path[PATH_SIZE] = {0};
  gchar *home = getenv("HOME");
  GError *err = NULL;

  if(home) {
    snprintf(path, PATH_SIZE, "%s/%s", home, file);
  } else {
    snprintf(path, PATH_SIZE, "%s", file);
  }

  if(g_key_file_load_from_file(wbr->kf, path, 0, &err)) {
    gchar ** groups = NULL;
    gsize len;
    gint idx;

    groups = g_key_file_get_groups(wbr->kf, &len);
    for(idx = len -1; idx >= 0; idx--) {
      waprop_t *prop = NULL;

      prop = g_slice_alloc0(sizeof(waprop_t));
      err = NULL, prop->host = g_key_file_get_string(wbr->kf, groups[idx], "host", &err);
      err = NULL, prop->root = g_key_file_get_string(wbr->kf, groups[idx], "root", &err);
      wbr->walist = g_slist_append(wbr->walist, prop);
      DD("WebApp = %s, host = %s, root = %s\n", groups[idx], prop->host, prop->root);
    }
  }
}

static void wb_config_term(webbridge_t *wbr) {
  GSList *ptr = wbr->walist;

  g_slist_free(ptr);
}

int main (int argc, char **argv) {
  gint port = 8888;
  webbridge_t self = {0};

  signal (SIGINT, wb_app_quit);
  setenv("PYTHONPATH", EMBED_PYTHON_PATH, 1);

  g_thread_init (NULL);
  g_type_init ();
  Py_Initialize();

  if(wb_core_init(&self, port)) {
    DD("webbridge is running port (%d).\n", port);

    wb_config_init(&self, CFGFILE);
    g_loop = g_main_loop_new (NULL, TRUE);
    g_main_loop_run (g_loop);
    g_main_loop_unref (g_loop);

    wb_config_term(&self);
  }
  
  wb_core_term(&self);
  Py_Finalize();

  return 0;
}
