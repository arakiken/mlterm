/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "main_loop.h"

#include <signal.h>
#include <unistd.h> /* getpid */
#include <pobl/bl_locale.h>
#include <pobl/bl_sig_child.h>
#include <pobl/bl_mem.h> /* bl_alloca_garbage_collect */
#include <pobl/bl_str.h> /* bl_str_alloca_dup */
#include <pobl/bl_def.h> /* USE_WIN32API */

#include <ui_font.h> /* ui_use_cp932_ucs_fot_xft */
#include <ui_screen_manager.h>
#include <ui_event_source.h>
#ifdef USE_XLIB
#include <xlib/ui_xim.h>
#endif
#ifdef USE_BRLAPI
#include <ui_brltty.h>
#endif
#ifdef USE_CONSOLE
#include <vt_term_manager.h> /* vt_get_all_terms */
#endif

#include "version.h"
#include "daemon.h"

#if defined(__ANDROID__) || defined(USE_QUARTZ) || (defined(USE_SDL2) && defined(__APPLE__)) || \
  defined(USE_WIN32API)
/* Shrink unused memory */
#define bl_conf_add_opt(conf, a, b, c, d, e) bl_conf_add_opt(conf, a, b, c, d, "")
#endif

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

static int8_t is_genuine_daemon;

/* --- static functions --- */

/*
 * signal handlers.
 */
#ifndef USE_WIN32API
static void sig_fatal(int sig) {
#ifdef DEBUG
  bl_warn_printf(BL_DEBUG_TAG "signal %d is received\n", sig);
#endif

  /* Remove ~/.mlterm/socket. */
  daemon_final();

  /* reset */
  signal(sig, SIG_DFL);

  kill(getpid(), sig);
}
#endif /* USE_WIN32API */

static int get_font_size_range(u_int *min, u_int *max, const char *str) {
  char *str_p;
  char *p;

  if ((str_p = bl_str_alloca_dup(str)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " alloca() failed.\n");
#endif

    return 0;
  }

  /* bl_str_sep() never returns NULL because str_p isn't NULL. */
  p = bl_str_sep(&str_p, "-");

  if (str_p == NULL) {
    bl_msg_printf("max font size is missing.\n");

    return 0;
  }

  if (!bl_str_to_uint(min, p)) {
    bl_msg_printf("min font size %s is not valid.\n", p);

    return 0;
  }

  if (!bl_str_to_uint(max, str_p)) {
    bl_msg_printf("max font size %s is not valid.\n", str_p);

    return 0;
  }

  return 1;
}

#ifdef USE_LIBSSH2
static void ssh_keepalive(void) { vt_pty_ssh_keepalive(100); }
#endif

/* --- global functions --- */

int main_loop_init(int argc, char **argv) {
  ui_main_config_t main_config;
  bl_conf_t *conf;
  char *value;
#ifdef USE_XLIB
  int use_xim;
#endif
  u_int max_screens_multiple;
  u_int num_startup_screens;
  u_int depth;
  char *invalid_msg = "%s %s is not valid.\n";
  char *orig_argv;
#if defined(USE_FRAMEBUFFER) || defined(USE_WAYLAND) || defined(USE_SDL2)
  int use_aafont = 0;
#endif

  if (!bl_locale_init("")) {
    bl_msg_printf("locale settings failed.\n");
  }

  bl_sig_child_init();

  bl_init_prog(argv[0],
               DETAIL_VERSION
               "\nFeatures:"
#if defined(USE_XLIB) && defined(NO_DYNAMIC_LOAD_TYPE)
#ifndef USE_TYPE_XFT
               " no-xft"
#endif
#ifndef USE_TYPE_CAIRO
               " no-cairo"
#endif
#endif
#ifdef NO_DYNAMIC_LOAD_CTL
#ifndef USE_FRIBIDI
               " no-bidi"
#endif
#ifndef USE_IND
               " no-indic"
#endif
#endif
#ifdef USE_OT_LAYOUT
               " otl"
#endif
#ifdef USE_LIBSSH2
               " ssh"
#endif
#ifdef USE_FREETYPE
               " freetype"
#endif
#ifdef USE_FONTCONFIG
               " fontconfig"
#endif
#ifdef USE_IM_PLUGIN
               " implugin"
#endif
#ifdef BUILTIN_IMAGELIB
               " imagelib(builtin)"
#endif
#ifdef USE_UTMP
               " utmp"
#endif
#ifdef USE_BRLAPI
               " brlapi"
#endif
  );

  if ((conf = bl_conf_new()) == NULL) {
    return 0;
  }

  ui_prepare_for_main_config(conf);

  /*
   * Same processing as vte_terminal_class_init().
   * Following options are not possible to specify as arguments of mlclient.
   * 1) Options which are used only when mlterm starts up and which aren't
   *    changed dynamically. (e.g. "startup_screens")
   * 2) Options which change status of all ptys or windows. (Including ones
   *    which are possible to change dynamically.)
   *    (e.g. "font_size_range")
   */

  bl_conf_add_opt(conf, '@', "screens", 0, "startup_screens",
                  "number of screens to open in start up [1]");
  bl_conf_add_opt(conf, 'h', "help", 1, "help", "show this help message");
  bl_conf_add_opt(conf, 'v', "version", 1, "version", "show version message");
  bl_conf_add_opt(conf, 'R', "fsrange", 0, "font_size_range",
                  "font size range for GUI configurator [6-30]");
#ifdef COMPOSE_DECSP_FONT
  bl_conf_add_opt(conf, 'Y', "decsp", 1, "compose_dec_special_font",
                  "compose dec special font [false]");
#endif
#ifdef USE_XLIB
  bl_conf_add_opt(conf, 'i', "xim", 1, "use_xim", "use XIM (X Input Method) [true]");
#endif
#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
  bl_conf_add_opt(conf, 'c', "cp932", 1, "use_cp932_ucs_for_xft",
                  "use CP932-Unicode mapping table for JISX0208 [false]");
#endif
#ifndef USE_WIN32API
  bl_conf_add_opt(conf, 'j', "daemon", 0, "daemon_mode",
#ifdef USE_WIN32GUI
                  /* 'genuine' is not supported in win32. */
                  "start as a daemon (none/blend) [none]"
#else
                  "start as a daemon (none/blend/genuine) [none]"
#endif
                  );
#endif
  bl_conf_add_opt(conf, '\0', "depth", 0, "depth", "visual depth");
  bl_conf_add_opt(conf, '\0', "maxptys", 0, "max_ptys",
                  "max ptys to open simultaneously (multiple of 32)");
#ifdef USE_LIBSSH2
  bl_conf_add_opt(conf, '\0', "keepalive", 0, "ssh_keepalive_interval",
                  "interval seconds to send keepalive. [0 = not send]");
#endif
  bl_conf_add_opt(conf, '\0', "deffont", 0, "default_font", "default font");
#ifdef SUPPORT_POINT_SIZE_FONT
  bl_conf_add_opt(conf, '\0', "point", 1, "use_point_size",
                  "treat fontsize as point instead of pixel [false]");
#endif
#if defined(USE_FREETYPE) && defined(USE_FONTCONFIG)
  bl_conf_add_opt(conf, '\0', "aafont", 1, "use_aafont",
                  "use [tv]aafont files with the use of fontconfig"
#if defined(USE_WAYLAND) || defined(USE_SDL2)
                  " [true]"
#else /* USE_FRAMEBUFFER */
                  " [false]"
#endif
                  );
#endif

  orig_argv = argv;
  if (!bl_conf_parse_args(conf, &argc, &argv,
#if defined(USE_QUARTZ) || (defined(USE_SDL2) && defined(__APPLE__))
                          1
#else
                          0
#endif
      )) {
    bl_conf_delete(conf);

    return 0;
  }

  if ((value = bl_conf_get_value(conf, "font_size_range"))) {
    u_int min_font_size;
    u_int max_font_size;

    if (get_font_size_range(&min_font_size, &max_font_size, value)) {
      ui_set_font_size_range(min_font_size, max_font_size);
    } else {
      bl_msg_printf(invalid_msg, "font_size_range", value);
    }
  }

  is_genuine_daemon = 0;

#ifndef USE_WIN32API
  if ((value = bl_conf_get_value(conf, "daemon_mode"))) {
    int ret;

    ret = 0;

#ifndef USE_WIN32GUI
    /* 'genuine' is not supported in win32. */
    if (strcmp(value, "genuine") == 0) {
      if ((ret = daemon_init()) > 0) {
        is_genuine_daemon = 1;
      }
    } else
#endif
        if (strcmp(value, "blend") == 0) {
      ret = daemon_init();
    }
#if 0
    else if (strcmp(value, "none") == 0) {
    }
#endif

    if (ret == -1) {
      execvp("mlclient", orig_argv);
    }
  }
#endif

#ifdef USE_XLIB
  use_xim = 1;

  if ((value = bl_conf_get_value(conf, "use_xim"))) {
    if (strcmp(value, "false") == 0) {
      use_xim = 0;
    }
  }

  ui_xim_init(use_xim);
#endif

#ifdef COMPOSE_DECSP_FONT
  if ((value = bl_conf_get_value(conf, "compose_dec_special_font"))) {
    if (strcmp(value, "true") == 0) {
      ui_compose_dec_special_font();
    }
  }
#endif

#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
  if ((value = bl_conf_get_value(conf, "use_cp932_ucs_for_xft")) == NULL ||
      strcmp(value, "true") == 0) {
    ui_use_cp932_ucs_for_xft();
  }
#endif

  if ((value = bl_conf_get_value(conf, "depth"))) {
    bl_str_to_uint(&depth, value);
  } else {
    depth = 0;
  }

  max_screens_multiple = 1;

  if ((value = bl_conf_get_value(conf, "max_ptys"))) {
    u_int max_ptys;

    if (bl_str_to_uint(&max_ptys, value)) {
      u_int multiple;

      multiple = max_ptys / 32;

      if (multiple * 32 != max_ptys) {
        bl_msg_printf("max_ptys %s is not multiple of 32.\n", value);
      }

      if (multiple > 1) {
        max_screens_multiple = multiple;
      }
    } else {
      bl_msg_printf(invalid_msg, "max_ptys", value);
    }
  }

  num_startup_screens = 1;

  if ((value = bl_conf_get_value(conf, "startup_screens"))) {
    u_int n;

    if (!bl_str_to_uint(&n, value) || (!is_genuine_daemon && n == 0)) {
      bl_msg_printf(invalid_msg, "startup_screens", value);
    } else {
      num_startup_screens = n;
    }
  }

#ifdef USE_LIBSSH2
  if ((value = bl_conf_get_value(conf, "ssh_keepalive_interval"))) {
    u_int interval;

    if (bl_str_to_uint(&interval, value) && interval > 0) {
      vt_pty_ssh_set_keepalive_interval(interval);
      ui_event_source_add_fd(-1, ssh_keepalive);
    }
  }
#endif

#ifdef KEY_REPEAT_BY_MYSELF
  if ((value = bl_conf_get_value(conf, "kbd_repeat_1"))) {
    extern int kbd_repeat_1;

    bl_str_to_int(&kbd_repeat_1, value);
  }

  if ((value = bl_conf_get_value(conf, "kbd_repeat_N"))) {
    extern int kbd_repeat_N;

    bl_str_to_int(&kbd_repeat_N, value);
  }
#endif
#ifdef USE_FRAMEBUFFER
#if defined(__OpenBSD__) || defined(__NetBSD__)
  if ((value = bl_conf_get_value(conf, "fb_resolution"))) {
    extern u_int fb_width;
    extern u_int fb_height;
    extern u_int fb_depth;

    sscanf(value, "%dx%dx%d", &fb_width, &fb_height, &fb_depth);
  }
#endif
#endif
#if defined(USE_FREETYPE) && defined(USE_FONTCONFIG)
#if defined(USE_WAYLAND) || defined(USE_SDL2)
  if (!(value = bl_conf_get_value(conf, "use_aafont")) || strcmp(value, "false") != 0) {
    ui_use_aafont();
    use_aafont = 1;
  }
#else /* USE_FRAMEBUFFER */
  if ((value = bl_conf_get_value(conf, "use_aafont")) && strcmp(value, "true") == 0) {
    ui_use_aafont();
    use_aafont = 1;
  }
#endif
#endif

  ui_main_config_init(&main_config, conf, argc, argv);

  if ((value = bl_conf_get_value(conf, "default_font"))) {
#if defined(USE_WAYLAND) || defined(USE_FRAMEBUFFER) || defined(USE_SDL2)
    ui_customize_font_file(use_aafont ? "aafont" : "font", "DEFAULT", value, 0);
    ui_customize_font_file(use_aafont ? "aafont" : "font", "ISO10646_UCS4_1", value, 0);
#else
    ui_customize_font_file(main_config.type_engine == TYPE_XCORE ? "font" : "aafont", "DEFAULT",
                           value, 0);
    ui_customize_font_file(main_config.type_engine == TYPE_XCORE ? "font" : "aafont",
                           "ISO10646_UCS4_1", value, 0);
#endif
  }

#ifdef SUPPORT_POINT_SIZE_FONT
  if ((value = bl_conf_get_value(conf, "use_point_size"))) {
    if (strcmp(value, "true") == 0) {
      ui_font_use_point_size(1);
    }
  }
#endif

  bl_conf_delete(conf);

  if (!ui_screen_manager_init("MLTERM=" VERSION, depth, max_screens_multiple,
                              num_startup_screens, &main_config)) {
    daemon_final();
#ifdef USE_XLIB
    ui_xim_final();
#endif
    bl_sig_child_final();
    bl_locale_final();

    return 0;
  }

  ui_event_source_init();

#ifdef USE_BRLAPI
  ui_brltty_init();
#endif

  bl_alloca_garbage_collect();

#ifndef USE_WIN32API
  signal(SIGHUP, sig_fatal);
  signal(SIGINT, sig_fatal);
  signal(SIGQUIT, sig_fatal);
  signal(SIGTERM, sig_fatal);
  signal(SIGPIPE, SIG_IGN);
#endif

  return 1;
}

int main_loop_final(void) {
#ifdef USE_BRLAPI
  ui_brltty_final();
#endif

  ui_event_source_final();

  ui_screen_manager_final();

  daemon_final();

  vt_free_word_separators();

  ui_free_mod_meta_prefix();

  bl_set_msg_log_file_name(NULL);

#ifdef USE_XLIB
  ui_xim_final();
#endif

  bl_sig_child_final();

  bl_locale_final();

  return 1;
}

int main_loop_start(void) {
  if (ui_screen_manager_startup() == 0) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " open_screen_intern() failed.\n");
#endif

    if (!is_genuine_daemon) {
      bl_msg_printf("Unable to open screen.\n");

      return 0;
    }
  }

  while (1) {
    bl_alloca_begin_stack_frame();

    if (!ui_event_source_process() && !is_genuine_daemon) {
#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Exiting...\n");
#endif

      break;
    }
#ifdef USE_CONSOLE
    else if (vt_get_all_terms(NULL) == 0) {
      break;
    }
#endif

    bl_alloca_end_stack_frame();
  }

  return 1;
}
