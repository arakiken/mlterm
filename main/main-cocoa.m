/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

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

extern char *global_args;

int main(int argc, const char* argv[]) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  @try {
    if (!getenv("LANG")) {
      set_lang();
    }

    bl_set_sys_conf_dir([[[NSBundle mainBundle] bundlePath] UTF8String]);
  }
  @finally {
    [pool release];
  }

  bl_set_msg_log_file_name("mlterm/msg.log");

  char *myargv[] = {"mlterm", NULL};
  main_loop_init(1, myargv);

  if (argc > 0) {
    int count;
    size_t len = 1; /* NULL terminator */

    for (count = 0; count < argc; count++) {
      len += (strlen(argv[count]) + 1);
    }

    if ((global_args = malloc(len))) {
      char *p = global_args;

      for (count = 0; count < argc - 1; count++) {
        strcpy(p, argv[count]);
        p += strlen(p);
        *(p++) = ' ';
      }
      strcpy(p, argv[count]);
    }
  }

  return NSApplicationMain(argc, argv);
}
