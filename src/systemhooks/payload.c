#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdarg.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <spawn.h>
#include <kerninfo.h>

#define RB_AUTOBOOT     0
#define CHECK_ERROR(action, msg) do { \
 ret = action; \
 if (ret) { \
  dprintf(fd_console, msg ": %d (%s)\n", errno, strerror(errno)); \
  spin(); \
 } \
} while (0)


int reboot_np(int howto, const char *message);

bool do_pspawn_hook = false;
uint32_t pflags = 0;

extern char** environ;
struct dyld_interpose_tuple {
	const void* replacement;
	const void* replacee;
};
void dyld_dynamic_interpose(const struct mach_header* mh, const struct dyld_interpose_tuple array[], size_t count);

int sandbox_check_by_audit_token(audit_token_t au, const char *operation, int sandbox_filter_type, ...);

typedef  void *posix_spawnattr_t;
typedef  void *posix_spawn_file_actions_t;
int posix_spawn(pid_t *, const char *,const posix_spawn_file_actions_t *,const posix_spawnattr_t *,char *const __argv[],char *const __envp[]);

typedef void* xpc_object_t;
typedef void* xpc_type_t;
typedef void* launch_data_t;
//typedef bool (^xpc_dictionary_applier_t)(const char *key, xpc_object_t value);

xpc_object_t xpc_dictionary_create(const char * const *keys, const xpc_object_t *values, size_t count);
void xpc_dictionary_set_uint64(xpc_object_t dictionary, const char *key, uint64_t value);
void xpc_dictionary_set_string(xpc_object_t dictionary, const char *key, const char *value);
int64_t xpc_dictionary_get_int64(xpc_object_t dictionary, const char *key);
xpc_object_t xpc_dictionary_get_value(xpc_object_t dictionary, const char *key);
bool xpc_dictionary_get_bool(xpc_object_t dictionary, const char *key);
void xpc_dictionary_set_fd(xpc_object_t dictionary, const char *key, int value);
void xpc_dictionary_set_bool(xpc_object_t dictionary, const char *key, bool value);
const char *xpc_dictionary_get_string(xpc_object_t dictionary, const char *key);
void xpc_dictionary_set_value(xpc_object_t dictionary, const char *key, xpc_object_t value);
xpc_type_t xpc_get_type(xpc_object_t object);
//bool xpc_dictionary_apply(xpc_object_t xdict, xpc_dictionary_applier_t applier);
int64_t xpc_int64_get_value(xpc_object_t xint);
char *xpc_copy_description(xpc_object_t object);
void xpc_dictionary_set_int64(xpc_object_t dictionary, const char *key, int64_t value);
const char *xpc_string_get_string_ptr(xpc_object_t xstring);
xpc_object_t xpc_array_create(const xpc_object_t *objects, size_t count);
xpc_object_t xpc_string_create(const char *string);
size_t xpc_dictionary_get_count(xpc_object_t dictionary);
void xpc_array_append_value(xpc_object_t xarray, xpc_object_t value);
void xpc_array_set_string(xpc_object_t xarray, size_t index, const char *string);
void xpc_release(xpc_object_t object);

#define XPC_ARRAY_APPEND ((size_t)(-1))
#define XPC_ERROR_CONNECTION_INVALID XPC_GLOBAL_OBJECT(_xpc_error_connection_invalid)
#define XPC_ERROR_TERMINATION_IMMINENT XPC_GLOBAL_OBJECT(_xpc_error_termination_imminent)
#define XPC_TYPE_ARRAY (&_xpc_type_array)
#define XPC_TYPE_BOOL (&_xpc_type_bool)
#define XPC_TYPE_DICTIONARY (&_xpc_type_dictionary)
#define XPC_TYPE_ERROR (&_xpc_type_error)
#define XPC_TYPE_STRING (&_xpc_type_string)


extern const struct _xpc_dictionary_s _xpc_error_connection_invalid;
extern const struct _xpc_dictionary_s _xpc_error_termination_imminent;
extern const struct _xpc_type_s _xpc_type_array;
extern const struct _xpc_type_s _xpc_type_bool;
extern const struct _xpc_type_s _xpc_type_dictionary;
extern const struct _xpc_type_s _xpc_type_error;
extern const struct _xpc_type_s _xpc_type_string;

#define DYLD_INTERPOSE(_replacment,_replacee) \
__attribute__((used)) static struct{ const void* replacment; const void* replacee; } _interpose_##_replacee \
__attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacment, (const void*)(unsigned long)&_replacee };

void spin() {
  while(1) {sleep(5);}
}

/*
  Launch our Daemon *correctly*
*/
xpc_object_t hook_xpc_dictionary_get_value(xpc_object_t dict, const char *key){
  xpc_object_t retval = xpc_dictionary_get_value(dict,key);
  if (getpid() != 1) return retval;
  if (strcmp(key,"LaunchDaemons") == 0) {
    static xpc_object_t submitJob = NULL;
    if (!submitJob) {
      submitJob = xpc_dictionary_create(NULL, NULL, 0);
      xpc_object_t programArguments = xpc_array_create(NULL, 0);
      xpc_array_set_string(programArguments, XPC_ARRAY_APPEND, "/cores/jbloader");
      if(getenv("XPC_USERSPACE_REBOOTED") != NULL) {
        xpc_array_set_string(programArguments, XPC_ARRAY_APPEND, "-u");
      }
      xpc_array_set_string(programArguments, XPC_ARRAY_APPEND, "-j");
      xpc_dictionary_set_bool(submitJob, "KeepAlive", false);
      xpc_dictionary_set_bool(submitJob, "RunAtLoad", true);
      xpc_dictionary_set_string(submitJob, "ProcessType", "Interactive");
      xpc_dictionary_set_string(submitJob, "UserName", "root");
      xpc_dictionary_set_string(submitJob, "Program", "/cores/jbloader");
      xpc_dictionary_set_string(submitJob, "StandardInPath", "/dev/console");
      xpc_dictionary_set_string(submitJob, "StandardOutPath", "/dev/console");
      xpc_dictionary_set_string(submitJob, "StandardErrorPath", "/dev/console");
      xpc_dictionary_set_string(submitJob, "Label", "in.palera.jbloader");
      xpc_dictionary_set_value(submitJob, "ProgramArguments", programArguments);
      xpc_release(programArguments);
    }
    xpc_dictionary_set_value(retval, "/System/Library/LaunchDaemons/in.palera.jbloader.plist", submitJob);
  } else if (strcmp(key, "sysstatuscheck") == 0) {
    static xpc_object_t newTask = NULL;
    if (newTask) return newTask;
    newTask = xpc_dictionary_create(NULL, NULL, 0);
    xpc_object_t programArguments = xpc_array_create(NULL, 0);
    xpc_array_set_string(programArguments, XPC_ARRAY_APPEND, "/cores/jbloader");
    if(getenv("XPC_USERSPACE_REBOOTED") != NULL) {
      xpc_array_set_string(programArguments, XPC_ARRAY_APPEND, "-u");
    }
    xpc_array_set_string(programArguments, XPC_ARRAY_APPEND, "-s");
    xpc_dictionary_set_bool(newTask, "PerformAfterUserspaceReboot", true);
    xpc_dictionary_set_bool(newTask, "RebootOnSuccess", true);
    xpc_dictionary_set_string(newTask, "Program", "/cores/jbloader");
    xpc_dictionary_set_value(newTask, "ProgramArguments", programArguments);
    xpc_release(programArguments);
    return newTask;
  }
  return retval;
}
DYLD_INTERPOSE(hook_xpc_dictionary_get_value, xpc_dictionary_get_value);

#if 0
bool hook_xpc_dictionary_get_bool(xpc_object_t dictionary, const char *key) {
  if (!strcmp(key, "LogPerformanceStatistics")) return true;
  else return xpc_dictionary_get_bool(dictionary, key);
}
DYLD_INTERPOSE(hook_xpc_dictionary_get_bool, xpc_dictionary_get_bool);
#endif

int hook_posix_spawnp_launchd(pid_t *pid,
                      const char *path,
                      const posix_spawn_file_actions_t *action,
                      const posix_spawnattr_t *attr,
                      char *const argv[], char *envp[]) {
  if (!argv || !path || !envp)
    return posix_spawnp(pid, path, action, attr, argv, envp);
  if (argv[1] == NULL || strcmp(argv[1], "com.apple.cfprefsd.xpc.daemon"))
    return posix_spawnp(pid, path, action, attr, argv, envp);

    char *inj = NULL;
    int envcnt = 0;
    while (envp[envcnt] != NULL) {
      envcnt++;
    }
    char** newenvp = malloc((envcnt + 2) * sizeof(char **));
    if (!newenvp) {
      return ENOMEM;
    }
    int j = 0;
    char* currentenv = NULL;
    for (int i = 0; i < envcnt; i++){
      if (strstr(envp[j], "DYLD_INSERT_LIBRARIES") != NULL) {
        currentenv = envp[j];
        continue;
      }
        newenvp[i] = envp[j];
        j++;
    }
            
    char *newlib = "/cores/payload.dylib";
    if(currentenv) {
      size_t inj_len = strlen(currentenv) + 1 + strlen(newlib) + 1;
      inj = malloc(inj_len);
      if (inj == NULL) {
        if (newenvp) free(newenvp);
        return ENOMEM;
      }
      snprintf(inj, inj_len, "%s:%s", currentenv, newlib);
    } else {
      size_t inj_len = strlen("DYLD_INSERT_LIBRARIES=") + strlen(newlib) + 1;
      inj = malloc(inj_len);
      if (inj == NULL) {
        if (newenvp) free(newenvp);
        return ENOMEM;
      }
      snprintf(inj, inj_len, "DYLD_INSERT_LIBRARIES=%s", newlib);
    }
    newenvp[j] = inj;
    newenvp[j + 1] = NULL;
            
    int ret = posix_spawnp(pid, path, action, attr, argv, newenvp);
    if (inj) free(inj);
    if (newenvp) free(newenvp);
    return ret;
}
int hook_posix_spawnp_xpcproxy(pid_t *pid,
                      const char *path,
                      const posix_spawn_file_actions_t *action,
                      const posix_spawnattr_t *attr,
                      char *const argv[], char *envp[])
{
    if (!argv || !envp || !path) return posix_spawnp(pid, path, action, attr, argv, envp);
    if(strcmp(argv[0], "/usr/sbin/cfprefsd")) {
        return posix_spawnp(pid, path, action, attr, argv, envp);
    }
    int envcnt = 0;
    while (envp[envcnt] != NULL)
    {
        envcnt++;
    }

    char **newenvp = malloc((envcnt + 2) * sizeof(char **));
    if (!newenvp) {
      return ENOMEM;
    }
    int j = 0;
    char *currentenv = NULL;
    for (int i = 0; i < envcnt; i++)
    {
        if (strstr(envp[j], "DYLD_INSERT_LIBRARIES") != NULL)
        {
            currentenv = envp[j];
            continue;
        }
        newenvp[i] = envp[j];
        j++;
    }

    char *newlib = "/cores/binpack/usr/lib/rootlesshooks.dylib";
    char *inj = NULL;
    if (currentenv)
    {
        size_t inj_len = strlen(currentenv) + 1 + strlen(newlib) + 1;
        inj = malloc(inj_len);
        if (inj == NULL) {
          if (newenvp) free(newenvp);
          return ENOMEM;
        }
        snprintf(inj, inj_len, "%s:%s", currentenv, newlib);
    }
    else
    {
        size_t inj_len = strlen("DYLD_INSERT_LIBRARIES=") + strlen(newlib) + 1;
        inj = malloc(inj_len);
        if (inj == NULL) {
          if (newenvp) free(newenvp);
          return ENOMEM;
        }
        snprintf(inj, inj_len, "DYLD_INSERT_LIBRARIES=%s", newlib);
    }
    newenvp[j] = inj;
    newenvp[j + 1] = NULL;

    int ret = posix_spawnp(pid, path, action, attr, argv, newenvp);
    if (inj) free(inj);
    if (newenvp) free(newenvp);
    return ret;
}
int hook_posix_spawnp(pid_t *pid,
                      const char *path,
                      const posix_spawn_file_actions_t *action,
                      const posix_spawnattr_t *attr,
                      char *const argv[], char *envp[]) {
  /* do_pspawn_hook only works in launchd */
  if (do_pspawn_hook && getpid() == 1) {
    return hook_posix_spawnp_launchd(pid, path, action, attr, argv, envp);
  }
  char exe_path[PATH_MAX];
  uint32_t bufsize = PATH_MAX;
  int ret = _NSGetExecutablePath(exe_path, &bufsize);
  if (ret) abort();
  if (!strcmp("/usr/sbin/cfprefsd", path) && getppid() == 1 && !strcmp("/usr/libexec/xpcproxy", exe_path)) {
    return hook_posix_spawnp_xpcproxy(pid, path, action, attr, argv, envp);
  } else {
    return posix_spawnp(pid, path, action, attr, argv, envp);
  }
}
DYLD_INTERPOSE(hook_posix_spawnp, posix_spawnp);

#define MEMORYSTATUS_CMD_SET_JETSAM_TASK_LIMIT        6

int memorystatus_control(uint32_t command, int32_t pid, uint32_t flags, void *buffer, size_t buffersize);
int hook_memorystatus_control(uint32_t command, int32_t pid, uint32_t flags, void *buffer, size_t buffersize) {
  if (command == MEMORYSTATUS_CMD_SET_JETSAM_TASK_LIMIT && pid == 1) {
    flags = 32768;
  }
  return memorystatus_control(command, pid, flags, buffer, buffersize);
}
DYLD_INTERPOSE(hook_memorystatus_control, memorystatus_control);

uint32_t get_flags_from_p1ctl(int fd_console) {
  uint32_t ramdisk_size_actual;
  int fd = open("/dev/rmd0", O_RDONLY, 0);
  read(fd, &ramdisk_size_actual, 4);
  lseek(fd, (long)(ramdisk_size_actual) + 0x1008L, SEEK_SET);
  read(fd, &pflags, 4);
  close(fd);
  dprintf(fd_console, "pflags: %u\n", pflags);
  return 0;
}

__attribute__((constructor))
static void customConstructor(int argc, const char **argv){
  if (getpid() != 1) return;
  int fd_console = open("/dev/console",O_RDWR,0);
  dprintf(fd_console,"================ Hello from payload.dylib ================ \n");
  signal(SIGBUS, SIG_IGN);
  /* make binpack available */
  pid_t pid;
  int ret;
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO,"/dev/console", O_RDWR, 0);
  posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/console", O_RDWR, 0);
  posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/console", O_RDWR, 0);
  CHECK_ERROR(posix_spawn(&pid, "/cores/jbloader", &actions, NULL, (char*[]){"/cores/jbloader","-f",NULL},environ), "could not spawn jbloader");
  posix_spawn_file_actions_destroy(&actions);
  int status;
  waitpid(pid, &status, 0);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    dprintf(fd_console, "jbloader quit unexpectedly\n");
    spin();
  }
  get_flags_from_p1ctl(fd_console);
  if ((pflags & palerain_option_setup_rootful)) {
    int32_t initproc_started = 1;
    CHECK_ERROR(sysctlbyname("kern.initproc_spawned", NULL, NULL, &initproc_started, 4), "sysctl kern.initproc_spawned=1");
    CHECK_ERROR(unmount("/cores/binpack/Applications", MNT_FORCE), "unmount(/cores/binpack/Applications)");
    CHECK_ERROR(unmount("/cores/binpack", MNT_FORCE), "unmount(/cores/binpack)");
    dprintf(fd_console, "Rebooting\n");
    host_reboot(mach_host_self(), 0x1000);
    dprintf(fd_console, "reboot failed\n");
    spin();
  }
  if ((pflags & palerain_option_rootful) == 0) do_pspawn_hook = true;
  dprintf(fd_console, "do_pspawn_hook: %d\n", do_pspawn_hook);
  dprintf(fd_console,"========= Goodbye from payload.dylib constructor ========= \n");
  close(fd_console);
}
