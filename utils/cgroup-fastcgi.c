/** cgroups FastCGI demo
 ** compile command: gcc -g -o cgroup.fastcgi cgroup-fastcgi.c -lcgroup  -lfcgi
 ** start fastcgi: spawn-fcgi -n -a 127.0.0.1 -p 9000 /home/user/cgroup.fastcgi
 ** I have tested with lighttpd on my linux PC
 **/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libcgroup.h>
extern char **environ;
#include <fcgi_stdio.h>
//char **environ = NULL;

#define OUT printf
#define DEBUG(...) fprintf(stderr, __VA_ARGS__)

#define GURI_LSTASKS "/cgroups/lstasks/"
#define GURI_LSCGROUPS "/cgroups/"
#define PURI_TASKMGR "/cgroups/taskmgr/"

#define URIMAX 256

void visit_cgroups_node(struct cgroup_file_info *info, char *root) {
  if (info->type == CGROUP_FILE_TYPE_DIR) {
    OUT("<cgroup><name>%s</name><fullpath>%s</fullpath></cgroup>", info->path, info->full_path+strlen(root)-1);
  }
}

void list_groups_info_by_controller(char *controller) {
  int err = 0, lvl;
  void *handle = NULL;
  struct cgroup_file_info info;
  char root[FILENAME_MAX] = {0};

  err = cgroup_walk_tree_begin(controller, "/", 0, &handle, &info, &lvl);
  if(err != 0) {
    return;
  }
  cgroup_walk_tree_set_flags(&handle, CGROUP_WALK_TYPE_POST_DIR);
  strcpy(root, info.full_path);
  visit_cgroups_node(&info, root);
  while((err = cgroup_walk_tree_next(0, &handle, &info, lvl)) != ECGEOF) {
    visit_cgroups_node(&info, root);
  }
  cgroup_walk_tree_end(&handle);
}

int list_all_cgroups () {
  int err = 0;
  void *handle = NULL;
  struct controller_data info;

  err = cgroup_get_all_controller_begin(&handle, &info);
  OUT("<controllers>");
  while(err != ECGEOF) {
    OUT("<controller>");
    OUT("<name>%s</name>", info.name);
    OUT("<ngroups>%d</ngroups>", info.num_cgroups);
    list_groups_info_by_controller(info.name);
    OUT("<enable>%d</enable>", info.enabled);
    OUT("</controller>");
    err = cgroup_get_all_controller_next(&handle, &info);
    if(err && err != ECGEOF) {
      DEBUG("list_all_cgroups failed with %s\n",  cgroup_strerror(err));
      return -1;
    }
  }
  OUT("</controllers>");

  err = cgroup_get_all_controller_end(&handle);

  return 0;
}

int list_task_by_cgroup(char *name, char *controller) {
  int size = 0, err = 0, idx =0 ;
  pid_t *pids = NULL;

  err = cgroup_get_procs(name, controller, &pids, &size);
  if (err) {
    DEBUG("list_task_by_cgroup failed with %s\n",  cgroup_strerror(err));
    return -1;
  }

  OUT("<tasks name=\"%s\" controller=\"%s\">", name, controller);
  for (; idx < size; idx++) {
    OUT("<pid>%d</pid>", pids[idx]);
  }
  OUT("</tasks>");

  if(pids) {
    free(pids);
  }

  return 0;
}

int attach_task_to_cgroup(char *path, const char * const controllers[], pid_t pid) {
  int err = 0;

  err = cgroup_change_cgroup_path(path, pid, controllers);
  if(err) {
    OUT("<action><status>ERR=%s</status><path>%s</path><PID>%d</PID></action>", cgroup_strerror(err), path, pid);
    return -1;
  }
  OUT("<action><status>SUCCESS</status><path>%s</path><PID>%d</PID></action>", path, pid);
  return 0;
}

static void dumpEnv(char **envp) {
  OUT("<EnvDump>");
  for(; *envp != NULL; envp++) {
    OUT("<env>%s</env>", *envp);
  }
  OUT("</EnvDump>\n");
}

int main(int argc, char *argv[]) {
  char **reqEnv = environ;
  int err = 0;

  err = cgroup_init();
  if(err) {
    DEBUG("cgroup_init failed with %s\n", cgroup_strerror(err));
    return -1;
  }

  while (FCGI_Accept() >= 0) {
    OUT("Content-type: text/xml\r\n");
    OUT("\r\n");
    char *uri = getenv("REQUEST_URI");
    if(! uri) continue;

    OUT("<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n");
    OUT("<root>");
    if(strncmp(GURI_LSCGROUPS, uri, URIMAX) == 0) {
      list_all_cgroups();
    } else if(strncmp(GURI_LSTASKS, uri, strlen(GURI_LSTASKS)) == 0) {
      char *controller = NULL, *name = NULL, *ptr = NULL;
      controller = uri+strlen(GURI_LSTASKS);
      name = strchr(controller, '/');
      if(! name) {
        name = "";
      } else {
        *name='\0', name++;
        ptr = strchr(name, '/');
        if(ptr) *ptr='\0';
      }
      list_task_by_cgroup(controller, name);
    } else if(strncmp("/cgroups/taskmgr", uri, URIMAX) == 0) {
      char postdata[256] = {'\0'}, *name = NULL, *pid;
      const char* controllers[] ={"cpuset", "memory", "cpu", NULL};

      fread(postdata, 256, 1, stdin);
      name = strchr(postdata, '&');
      if(! name) {
        name = "";
      } else {
        *name = '\0', name++;
        name = strchr(name, '='), name ++;
      }
      pid = strchr(postdata, '='), pid++;
      //OUT("<postdata><PID>%s</PID><name>%s</name></postdata>", pid, name);
      attach_task_to_cgroup(name, controllers, atol(pid));
    }
    //dumpEnv(reqEnv);
    OUT("</root>");
  }

  return 0;
}
