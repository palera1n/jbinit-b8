#include <jbloader.h>
#include <mach-o/loader.h>

mach_port_t (*SBSSpringBoardServerPort)(void);

static CFRunLoopRef loop;
void sb_launched(CFNotificationCenterRef center, void *observer,
				 CFStringRef name, const void *object, CFDictionaryRef info) {
    CFRunLoopStop(loop);
}

int jbloader_bakera1nd(int argc, char **argv)
{
  setvbuf(stdout, NULL, _IONBF, 0);
  if (checkrain_options_enabled(pinfo.flags, palerain_option_jbinit_log_to_file))
  {
    int fd_log = open("/cores/jbinit.log", O_WRONLY | O_APPEND | O_SYNC, 0644);
    if (fd_log != -1)
    {
      dup2(fd_log, STDOUT_FILENO);
      dup2(fd_log, STDERR_FILENO);
      puts("======== jbloader (system boot) log start =========");
    }
    else
      puts("cannot open /cores/jbinit.log for logging");
  }
  // sleep(2);
  puts(
    "#==================================\n"
    "#    palera1n loader (bakera1nd)   \n"
    "#      (c) palera1n team     \n"
    "#==================================="
  );
  pthread_t ssh_thread, prep_jb_launch_thread, prep_jb_ui_thread;
  pthread_create(&prep_jb_launch_thread, NULL, prep_jb_launch, NULL);
  pthread_create(&ssh_thread, NULL, enable_ssh, NULL);
  pthread_join(prep_jb_launch_thread, NULL);
  pthread_join(ssh_thread, NULL);

  if (dyld_platform == PLATFORM_IOS) {
    void *springboardservices = dlopen("/System/Library/PrivateFrameworks/SpringBoardServices.framework/SpringBoardServices", RTLD_NOW);
    if (springboardservices) {
      SBSSpringBoardServerPort = dlsym(springboardservices, "SBSSpringBoardServerPort");
      if (SBSSpringBoardServerPort && !MACH_PORT_VALID(SBSSpringBoardServerPort())) {
        loop = CFRunLoopGetCurrent();
        CFNotificationCenterAddObserver(CFNotificationCenterGetDarwinNotifyCenter(), NULL, &sb_launched, CFSTR("SBSpringBoardDidLaunchNotification"), NULL, 0);
        CFRunLoopRun();
      }
      dlclose(springboardservices);
    } else {
      fprintf(stderr, "failed to dlopen springboardservices\n");
    }
  }
  if (checkrain_options_enabled(info.flags, checkrain_option_safemode)) safemode_alert();

  if (dyld_platform == PLATFORM_IOS)
    uicache_loader();
  if (!checkrain_options_enabled(info.flags, checkrain_option_force_revert)
  && dyld_platform == PLATFORM_IOS)
  {
    pthread_create(&prep_jb_ui_thread, NULL, prep_jb_ui, NULL);
  }
  if (!checkrain_options_enabled(info.flags, checkrain_option_force_revert) && dyld_platform == PLATFORM_IOS)
    pthread_join(prep_jb_ui_thread, NULL);


  printf("palera1n: goodbye!\n");
  printf("========================================\n");
  return 0;
}
