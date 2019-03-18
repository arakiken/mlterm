/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <pobl/bl_config.h> /* HAVE_WINDOWS_H, WORDS_BIGENDIAN */
#ifdef HAVE_WINDOWS_H
#include <windows.h> /* In Cygwin <windows.h> is not included and error happens in jni.h. */
#endif

#include "mlterm.h"

#include <unistd.h> /* select */
#include <pobl/bl_locale.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_sig_child.h>
#include <pobl/bl_mem.h> /* alloca */
#include <pobl/bl_str.h> /* bl_str_to_uint */
#include <pobl/bl_conf.h>
#include <pobl/bl_conf_io.h>
#include <pobl/bl_path.h>   /* bl_basename */
#include <pobl/bl_args.h>   /* _bl_arg_str_to_array */
#include "vt_term_manager.h"

#if defined(USE_WIN32API)
#define CONFIG_PATH "."
#elif defined(SYSCONFDIR)
#define CONFIG_PATH SYSCONFDIR
#else
#define CONFIG_PATH "/etc"
#endif

#if 0
#define USE_LOCAL_ECHO_BY_DEFAULT
#endif

#if 0
#define __DEBUG
#endif

/* --- global functions --- */

vt_term_t *mlterm_open(char *host, char *pass, int cols, int rows, u_int log_size,
                       char *encoding_str, char **argv,
                       vt_xterm_event_listener_t *xterm_listener,
                       vt_config_event_listener_t *config_listener,
                       vt_screen_event_listener_t *screen_listener,
                       vt_pty_event_listener_t *pty_listener, int open_pty) {
  vt_term_t *term;
  vt_char_encoding_t encoding;
  int is_auto_encoding;
  char *envv[4];
  char *cmd_path;

  static int wsa_inited = 0;
  static u_int tab_size;
  static vt_char_encoding_t encoding_default;
  static int is_auto_encoding_default;
  static vt_unicode_policy_t unicode_policy;
  static u_int col_size_a;
  static int use_char_combining;
  static int use_multi_col_char;
  static int use_login_shell;
  static int logging_vt_seq;
  static int use_local_echo;
  static int use_auto_detect;
  static int use_ansi_colors;
  static char *term_type;
  static char *public_key;
  static char *private_key;
  static char **default_argv;
  static char *default_cmd_path;

  if (!wsa_inited) {
    bl_conf_t *conf;

#if defined(USE_WIN32API) && defined(USE_LIBSSH2)
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 0), &wsadata);

    /*
     * Prevent vt_pty_ssh from spawning a thread to watch pty.
     * If both vt_pty_ssh and MLTerm.java spawn threads to watch ptys,
     * problems will happen in reloading (that is, finalizing and
     * starting simultaneously) an applet page (mltermlet.html)
     * in a web browser.
     *
     * This hack doesn't make sense to vt_pty_pipewin32.c.
     * In the first place, if vt_pty_pipewin32 is used,
     * Java_mlterm_MLTermPty_waitForReading() returns JNI_FALSE
     * and PtyWather thread of MLTerm.java immediately exits.
     * So threads in vt_pty_pipewin32 should not be prevented.
     */
    CreateEvent(NULL, FALSE, FALSE, "PTY_READ_READY");
#endif

    bl_init_prog("mlterm", "3.8.7");
    bl_set_sys_conf_dir(CONFIG_PATH);
    bl_locale_init("");
    bl_sig_child_start();
    vt_term_manager_init(1);
    vt_color_config_init();

    tab_size = 8;
    encoding_default = vt_get_char_encoding("auto");
    is_auto_encoding_default = 1;

    if (strcmp(bl_get_lang(), "ja") == 0) {
      col_size_a = 2;
    } else {
      col_size_a = 1;
    }

    use_char_combining = 1;
    use_multi_col_char = 1;
#ifdef USE_LOCAL_ECHO_BY_DEFAULT
    use_local_echo = 1;
#endif
    use_ansi_colors = 1;

    if ((conf = bl_conf_new())) {
      char *rcpath;
      char *value;

      if ((rcpath = bl_get_sys_rc_path("mlterm/main"))) {
        bl_conf_read(conf, rcpath);
        free(rcpath);
      }

      if ((rcpath = bl_get_user_rc_path("mlterm/main"))) {
        bl_conf_read(conf, rcpath);
        free(rcpath);
      }

      if ((value = bl_conf_get_value(conf, "logging_msg")) && strcmp(value, "false") == 0) {
        bl_set_msg_log_file_name(NULL);
      } else {
        bl_set_msg_log_file_name("mlterm/msg.log");
      }

      if ((value = bl_conf_get_value(conf, "logging_vt_seq")) && strcmp(value, "true") == 0) {
        logging_vt_seq = 1;
      }

      if ((value = bl_conf_get_value(conf, "tabsize"))) {
        bl_str_to_uint(&tab_size, value);
      }

      if ((value = bl_conf_get_value(conf, "encoding"))) {
        vt_char_encoding_t e;
        if ((e = vt_get_char_encoding(value)) != VT_UNKNOWN_ENCODING) {
          encoding_default = e;

          if (strcmp(value, "auto") == 0) {
            is_auto_encoding_default = 1;
          } else {
            is_auto_encoding_default = 0;
          }
        }
      }

      if ((value = bl_conf_get_value(conf, "not_use_unicode_font"))) {
        if (strcmp(value, "true") == 0) {
          unicode_policy = NOT_USE_UNICODE_FONT;
        }
      }

      if ((value = bl_conf_get_value(conf, "only_use_unicode_font"))) {
        if (strcmp(value, "true") == 0) {
          if (unicode_policy == NOT_USE_UNICODE_FONT) {
            unicode_policy = 0;
          } else {
            unicode_policy = ONLY_USE_UNICODE_FONT;
          }
        }
      }

      if ((value = bl_conf_get_value(conf, "col_size_of_width_a"))) {
        bl_str_to_uint(&col_size_a, value);
      }

      if ((value = bl_conf_get_value(conf, "use_combining"))) {
        if (strcmp(value, "false") == 0) {
          use_char_combining = 0;
        }
      }

      /* use_multi_col_char=false causes corrupt screen on SWT or vim terminal. */
#if 0
      if ((value = bl_conf_get_value(conf, "use_muti_col_char"))) {
        if (strcmp(value, "false") == 0) {
          use_multi_col_char = 0;
        }
      }
#endif

      if ((value = bl_conf_get_value(conf, "use_login_shell"))) {
        if (strcmp(value, "true") == 0) {
          use_login_shell = 1;
        }
      }

      if ((value = bl_conf_get_value(conf, "use_local_echo"))) {
#ifdef USE_LOCAL_ECHO_BY_DEFAULT
        if (strcmp(value, "false") == 0) {
          use_local_echo = 0;
        }
#else
        if (strcmp(value, "true") == 0) {
          use_local_echo = 1;
        }
#endif
      }

      if ((value = bl_conf_get_value(conf, "use_alt_buffer"))) {
        if (strcmp(value, "false") == 0) {
          vt_set_use_alt_buffer(0);
        }
      }

      if ((value = bl_conf_get_value(conf, "use_ansi_colors"))) {
        if (strcmp(value, "false") == 0) {
          use_ansi_colors = 0;
        }
      }

      if ((value = bl_conf_get_value(conf, "auto_detect_encodings"))) {
        vt_set_auto_detect_encodings(value);
      }

      if ((value = bl_conf_get_value(conf, "use_auto_detect"))) {
        if (strcmp(value, "true") == 0) {
          use_auto_detect = 1;
        }
      }

      if ((value = bl_conf_get_value(conf, "termtype"))) {
        term_type = strdup(value);
      } else {
        term_type = "xterm";
      }

#ifdef USE_LIBSSH2
      if ((value = bl_conf_get_value(conf, "ssh_public_key"))) {
        public_key = strdup(value);
      }

      if ((value = bl_conf_get_value(conf, "ssh_private_key"))) {
        private_key = strdup(value);
      }

      if ((value = bl_conf_get_value(conf, "cipher_list"))) {
        vt_pty_ssh_set_cipher_list(strdup(value));
      }

      if ((value = bl_conf_get_value(conf, "ssh_keepalive_interval"))) {
        u_int keepalive_interval;

        if (bl_str_to_uint(&keepalive_interval, value) && keepalive_interval > 0) {
          vt_pty_ssh_set_keepalive_interval(keepalive_interval);
        }
      }

      if ((value = bl_conf_get_value(conf, "ssh_x11_forwarding"))) {
        if (strcmp(value, "true") == 0) {
          vt_pty_ssh_set_use_x11_forwarding(NULL, 1);
        }
      }

      if ((value = bl_conf_get_value(conf, "allow_scp"))) {
        if (strcmp(value, "true") == 0) {
          vt_set_use_scp_full(1);
        }
      }
#endif

#if 0
      /* XXX How to get password ? */
      if ((value = bl_conf_get_value(conf, "default_server"))) {
        default_server = strdup(value);
      }
#endif

      if (((value = bl_conf_get_value(conf, "exec_cmd"))
#if defined(USE_WIN32API) && !defined(USE_LIBSSH2)
           || (value = plink)
#endif
               ) &&
          (default_argv = malloc(sizeof(char *) * bl_count_char_in_str(value, ' ') + 2))) {
        int argc;

        bl_arg_str_to_array(default_argv, &argc, strdup(value));
        default_cmd_path = default_argv[0];
      }

      bl_conf_destroy(conf);
    }

    unicode_policy |= ONLY_USE_UNICODE_BOXDRAW_FONT;

    wsa_inited = 1;
  }

  encoding = encoding_default;
  is_auto_encoding = is_auto_encoding_default;

  if (encoding_str) {
    vt_char_encoding_t e;

    if ((e = vt_get_char_encoding(encoding_str)) != VT_UNKNOWN_ENCODING) {
      encoding = e;

      if (strcmp(encoding_str, "auto") == 0) {
        is_auto_encoding = 1;
      } else {
        is_auto_encoding = 0;
      }
    }
  }

  if (!(term = vt_create_term(term_type, cols, rows, tab_size, log_size, encoding, is_auto_encoding,
                              use_auto_detect, logging_vt_seq, unicode_policy, col_size_a,
                              use_char_combining, use_multi_col_char, 0 /* use_ctl */,
                              0 /* bidi_mode */, NULL /* bidi_separators */,
                              0 /* use_dynamic_comb */, BSM_STATIC, 0 /* vertical_mode */,
                              use_local_echo, NULL, NULL, use_ansi_colors, 0 /* alt_color_mode */,
                              0 /* use_ot_layout */, 0 /* cursor_style */,
                              0 /* ignore_broadcasted_chars */))) {
    goto error;
  }

  vt_term_attach(term, xterm_listener, config_listener, screen_listener, pty_listener);

  if (!open_pty) {
    return term;
  }

  if (!host && !(host = getenv("DISPLAY"))) {
    host = ":0.0";
  }

  if (argv) {
    cmd_path = argv[0];
  } else if (default_argv) {
    argv = default_argv;
    cmd_path = default_cmd_path;
  } else {
#ifndef USE_WIN32API
    if (pass)
#endif
    {
      cmd_path = NULL;
      argv = alloca(sizeof(char *));
      argv[0] = NULL;
    }
#ifndef USE_WIN32API
    else {
      cmd_path = getenv("SHELL");
      argv = alloca(sizeof(char *) * 2);
      argv[1] = NULL;
    }
#endif
  }

  if (cmd_path) {
    if (use_login_shell) {
      argv[0] = alloca(strlen(cmd_path) + 2);
      sprintf(argv[0], "-%s", bl_basename(cmd_path));
    } else {
      argv[0] = bl_basename(cmd_path);
    }
  }

  envv[0] = alloca(8 + strlen(host) + 1);
  sprintf(envv[0], "DISPLAY=%s", host);
  envv[1] = "TERM=xterm";
  envv[2] = "COLORFGBG=default;default";
  envv[3] = NULL;

  if (vt_term_open_pty(term, cmd_path, argv, envv, host, NULL, pass, public_key, private_key,
                       0, 0)) {
    return term;
  } else {
    vt_destroy_term(term);
  }

error:
#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Failed to open pty.\n");
#endif

  return NULL;
}

void mlterm_final(void) {
  vt_term_manager_final();
  vt_color_config_final();
  vt_free_word_separators();
  bl_sig_child_final();
  bl_locale_final();
}
