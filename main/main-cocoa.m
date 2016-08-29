/*
 *	$Id$
 */

#import <Cocoa/Cocoa.h>
#include <pobl/bl_conf_io.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_dlfcn.h>
#include <pobl/bl_mem.h> /* alloca */
#include <pobl/bl_unistd.h>
#include "main_loop.h"

#if defined(SYSCONFDIR)
#define CONFIG_PATH SYSCONFDIR
#else
#define CONFIG_PATH "/etc"
#endif

/* --- static functions --- */

static void set_lang(void) {
  char* locale;

  if ((locale = [[[NSLocale currentLocale]
           objectForKey:NSLocaleIdentifier] UTF8String])) {
    char* p;

    if ((p = alloca(strlen(locale) + 7))) {
      sprintf(p, "%s.UTF-8", locale);
      bl_setenv("LANG", p, 1);
    }
  }
}

/* --- global functions --- */

int main(int argc, const char* argv[]) {
  if (!getenv("LANG")) {
    set_lang();
  }

  bl_set_sys_conf_dir([[[NSBundle mainBundle] bundlePath] UTF8String]);
  bl_set_msg_log_file_name("mlterm/msg.log");

  char* myargv[] = {"mlterm", NULL};
  main_loop_init(1, myargv);

  return NSApplicationMain(argc, argv);
}
