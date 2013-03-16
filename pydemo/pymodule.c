#include <sys/wait.h>

static PyObject* _py_cfg_launchapp(PyObject *, PyObject *);

static PyMethodDef g_dvmcfg_methods[] = {
  {"launchapp", _py_cfg_launchapp, METH_OLDARGS,
   "launch application within dvmcfg"},
  {NULL, NULL, 0, NULL}
};

static gboolean launch_app_async(gchar *argv[], GPid *pid, gint flag) {
  gboolean ret;
  GError *err = NULL;

  ret = g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH|flag, NULL, NULL, pid, &err);
  if(ret == FALSE) {
    DD("**LAUNCH APP %s %s failed**\n", argv[0], argv[1]);
  }

  return ret;
}

static void _app_watch_cb(GPid pid, gint status, gpointer data) {
  PyObject *args= Py_BuildValue("(i)", status);
  PyObject *watch_cb = (PyObject *)data;

  if(watch_cb && PyCallable_Check(watch_cb)) {
    PyObject_CallObject(watch_cb, args);
    Py_DECREF(watch_cb);
  }
  Py_DECREF(args);
  Py_DECREF(watch_cb);
}

static PyObject* _py_cfg_launchapp(PyObject *self, PyObject *args) {
  char *source = NULL, *method = NULL;
  gchar **argv;
  GPid pid;
  gint idx = 0;
  PyObject *_pid_watch_cb;

  if(!PyArg_ParseTuple(args, "Os", &_pid_watch_cb, &source))
    return NULL;

  /**out of this function MUST BE INCREF _pid_watch_cb*/
  Py_INCREF(_pid_watch_cb);
  argv = g_strsplit(source, " ", 0);
  if(launch_app_async(argv, &pid, G_SPAWN_DO_NOT_REAP_CHILD)) {
    g_child_watch_add(pid, _app_watch_cb, _pid_watch_cb);
  }
  g_strfreev(argv);

  return Py_BuildValue("I", pid);
}
