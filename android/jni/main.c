/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <pobl/bl_conf_io.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_dlfcn.h>
#include <vt_term_manager.h>
#if 0
#include <sys/prctl.h>
#endif
#include "uitoolkit/ui_display.h"
#include "uitoolkit/ui_event_source.h"

#ifdef SYSCONFDIR
#define CONFIG_PATH SYSCONFDIR
#else
#define CONFIG_PATH "/etc"
#endif

#if 0
#define SAVE_DEFAULT_FONT
#endif

#ifdef SAVE_DEFAULT_FONT

/* --- global variables --- */

char *default_font_path;

/* --- static functions --- */

static inline void save_default_font(ANativeActivity *activity) {
  JNIEnv *env;
  jobject this;
  jstring jstr;
  const char *path;

  if (default_font_path || (*activity->vm)->GetEnv(activity->vm, &env, JNI_VERSION_1_6) != JNI_OK) {
    return;
  }

  this = activity->clazz;
  jstr = (*env)->CallObjectMethod(
      env, this, (*env)->GetMethodID(env, (*env)->GetObjectClass(env, this), "saveDefaultFont",
                                     "()Ljava/lang/String;"));

  path = (*env)->GetStringUTFChars(env, jstr, NULL);
  default_font_path = strdup(path);
  (*env)->ReleaseStringUTFChars(env, jstr, path);
}

#endif /* SAVE_DEFAULT_FONT */

static void attach_current_thread(JavaVM *vm) {
  JNIEnv *env;
  (*vm)->AttachCurrentThread(vm, &env, NULL);
}

static void detach_current_thread(JavaVM *vm) {
  (*vm)->DetachCurrentThread(vm);
}

static void finish(struct android_app *app) {
    int ident;
    int events;
    struct android_poll_source *source;

    ui_display_final(); /* Calls ANativeActivity_finish() */

    while ((ident = ALooper_pollAll(-1, NULL, &events, (void **)&source)) >= 0 &&
           ui_display_process_event(source, ident));
}

/* --- global functions --- */

void android_main(struct android_app *app) {
  int argc = 1;
  char *argv[] = {"mlterm"};

#if 0
  prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);
#endif

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " android_main started.\n");
#endif

  bl_set_sys_conf_dir(CONFIG_PATH);

  attach_current_thread(app->activity->vm);

#ifdef SAVE_DEFAULT_FONT
  save_default_font(app->activity);
#endif

  if (ui_display_init(app) &&      /* ui_display_init() returns 1 only once. */
      !main_loop_init(argc, argv)) /* main_loop_init() is called once. */
  {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " main_loop_init() failed.\n");
#endif

    finish(app);
  } else if (!main_loop_start()) {
    /* Unable to open any screen. */
    finish(app);
  }

  /* Only screen objects are closed. */
  ui_screen_manager_suspend();

  detach_current_thread(app->activity->vm);

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " android_main finished.\n");
#endif
}
