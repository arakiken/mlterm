/*
 *	$Id$
 */

#include "ui_screen_manager.h"

#include <stdio.h>  /* sprintf */
#include <string.h> /* memset/memcpy */
#include <stdlib.h> /* getenv */
#include <unistd.h> /* getuid */

#include <pobl/bl_def.h> /* USE_WIN32API */
#ifndef USE_WIN32API
#include <pwd.h> /* getpwuid */
#endif

#include <pobl/bl_debug.h>
#include <pobl/bl_str.h>  /* bl_str_sep/bl_str_to_int/bl_str_alloca_dup/strdup */
#include <pobl/bl_path.h> /* bl_basename */
#include <pobl/bl_util.h> /* DIGIT_STR_LEN */
#include <pobl/bl_mem.h>  /* alloca/bl_alloca_garbage_collect/malloc/free */
#include <pobl/bl_conf.h>
#include <pobl/bl_conf_io.h>
#include <pobl/bl_types.h> /* u_int */
#include <pobl/bl_args.h>  /* bl_arg_str_to_array */
#include <pobl/bl_sig_child.h>
#include <vt_term_manager.h>
#include <vt_char_encoding.h>

#include "ui_layout.h"
#include "ui_display.h"

#if defined(USE_WIN32API) || defined(USE_LIBSSH2)
#include "ui_connect_dialog.h"
#endif

#define MAX_SCREENS (MSU * max_screens_multiple) /* Default MAX_SCREENS is 32. */
#define MSU (8 * sizeof(dead_mask[0]))           /* MAX_SCREENS_UNIT */

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

static char *mlterm_version;

static u_int max_screens_multiple;
static u_int32_t *dead_mask;

static ui_screen_t **screens;
static u_int num_of_screens;

static u_int depth;

static u_int num_of_startup_screens;

static ui_system_event_listener_t system_listener;

static ui_main_config_t main_config;
#if defined(USE_WIN32API) || defined(USE_LIBSSH2)
static const char *cur_default_server;
#endif

static ui_shortcut_t shortcut;

/* --- static functions --- */

/*
 * Callbacks of vt_config_event_listener_t events.
 */

/*
 * Reload mlterm/main file and reset main_config.
 * Notice: Saved changes are not applied to the screens already opened.
 */
static void config_saved(void) {
  bl_conf_t *conf;
  char *argv[] = {"mlterm", NULL};

  ui_main_config_final(&main_config);

  if ((conf = bl_conf_new()) == NULL) {
    return;
  }

  ui_prepare_for_main_config(conf);
  ui_main_config_init(&main_config, conf, 1, argv);

  bl_conf_delete(conf);
}

static void font_config_updated(void) {
  u_int count;

  ui_font_cache_unload_all();

  for (count = 0; count < num_of_screens; count++) {
    ui_screen_reset_view(screens[count]);
  }
}

static void color_config_updated(void) {
  u_int count;

  ui_color_cache_unload_all();

  ui_display_reset_cmap();

  for (count = 0; count < num_of_screens; count++) {
    ui_screen_reset_view(screens[count]);
  }
}

static vt_term_t *create_term_intern(void) {
  vt_term_t *term;

  if ((term = vt_create_term(
           main_config.term_type, main_config.cols, main_config.rows, main_config.tab_size,
           main_config.num_of_log_lines, main_config.encoding, main_config.is_auto_encoding,
           main_config.use_auto_detect, main_config.logging_vt_seq, main_config.unicode_policy,
           main_config.col_size_of_width_a, main_config.use_char_combining,
           main_config.use_multi_col_char, main_config.use_ctl, main_config.bidi_mode,
           main_config.bidi_separators, main_config.use_dynamic_comb, main_config.bs_mode,
           main_config.vertical_mode, main_config.use_local_echo, main_config.title,
           main_config.icon_name, main_config.alt_color_mode, main_config.use_ot_layout)) == NULL) {
    return NULL;
  }

  if (main_config.icon_path) {
    vt_term_set_icon_path(term, main_config.icon_path);
  }

  if (main_config.unlimit_log_size) {
    vt_term_unlimit_log_size(term);
  }

  return term;
}

static int open_pty_intern(vt_term_t *term, char *cmd_path, char **cmd_argv, char *display,
                           Window window, u_int width_pix, u_int height_pix) {
  char *env[6]; /* MLTERM,TERM,WINDOWID,DISPLAY,COLORFGBG,NULL */
  char **env_p;
  char wid_env[9 + DIGIT_STR_LEN(Window) + 1]; /* "WINDOWID="(9) + [32bit digit] + NULL(1) */
  char *disp_env;
  char *term_env;
  char *uri;
  char *pass;
  int ret;

  env_p = env;

  *(env_p++) = mlterm_version;

  sprintf(wid_env, "WINDOWID=%ld", window);
  *(env_p++) = wid_env;

  /* "DISPLAY="(8) + NULL(1) */
  if (display && (disp_env = alloca(8 + strlen(display) + 1))) {
    sprintf(disp_env, "DISPLAY=%s", display);
    *(env_p++) = disp_env;
  }

  /* "TERM="(5) + NULL(1) */
  if (main_config.term_type && (term_env = alloca(5 + strlen(main_config.term_type) + 1))) {
    sprintf(term_env, "TERM=%s", main_config.term_type);
    *(env_p++) = term_env;
  }

  *(env_p++) = "COLORFGBG=default;default";

  /* NULL terminator */
  *env_p = NULL;

  uri = NULL;
  pass = NULL;

#if !defined(USE_WIN32API) && defined(USE_LIBSSH2)
  if (main_config.default_server)
#endif
#if defined(USE_WIN32API) || defined(USE_LIBSSH2)
  {
    char *user;
    char *host;
    char *port;
    char *encoding;
    char *exec_cmd;
    int x11_fwd;
    void *session;

    x11_fwd = main_config.use_x11_forwarding;

#ifdef USE_LIBSSH2
    if (!main_config.show_dialog &&
#ifdef USE_WIN32API
        main_config.default_server &&
#endif
        bl_parse_uri(NULL, &user, &host, &port, NULL, &encoding,
                     bl_str_alloca_dup(main_config.default_server)) &&
        (session = vt_search_ssh_session(host, port, user))) {
      uri = strdup(main_config.default_server);
      pass = strdup("");
      exec_cmd = NULL;

      if (x11_fwd) {
        vt_pty_ssh_set_use_x11_forwarding(session, x11_fwd);
      }
    } else
#endif
        if (!ui_connect_dialog(&uri, &pass, &exec_cmd, &x11_fwd, display, window,
                               main_config.server_list, main_config.default_server)) {
      bl_msg_printf("Connect dialog is canceled.\n");

      return 0;
    } else {
      if (!bl_parse_uri(NULL, &user, &host, &port, NULL, &encoding, bl_str_alloca_dup(uri))) {
        encoding = NULL;
      }

#ifdef USE_LIBSSH2
      vt_pty_ssh_set_use_x11_forwarding(vt_search_ssh_session(host, port, user), x11_fwd);
#endif
    }

#ifdef __DEBUG
    bl_debug_printf("Connect dialog: URI %s pass %s\n", uri, pass);
#endif

    if (encoding) {
      if (vt_term_is_attached(term)) {
        /*
         * Don't use vt_term_change_encoding() here because
         * encoding change could cause special visual change
         * which should update the state of ui_screen_t.
         */
        char *seq;
        size_t len;

        if ((seq = alloca((len = 16 + strlen(encoding) + 2)))) {
          sprintf(seq, "\x1b]5379;encoding=%s\x07", encoding);
          vt_term_write_loopback(term, seq, len - 1);
        }
      } else {
        vt_term_change_encoding(term, vt_get_char_encoding(encoding));
      }
    }

    if (exec_cmd) {
      int argc;
      char *tmp;

      tmp = exec_cmd;
      exec_cmd = bl_str_alloca_dup(exec_cmd);
      cmd_argv = bl_arg_str_to_array(&argc, exec_cmd);
      cmd_path = cmd_argv[0];

      free(tmp);
    }
  }
#endif

#if 0
  if (cmd_argv) {
    char **p;

    bl_debug_printf(BL_DEBUG_TAG " %s", cmd_path);
    p = cmd_argv;
    while (*p) {
      bl_msg_printf(" %s", *p);
      p++;
    }
    bl_msg_printf("\n");
  }
#endif

  /*
   * If cmd_path and pass are NULL, set default shell as cmd_path.
   * If uri is not NULL (= connecting to ssh/telnet/rlogin etc servers),
   * cmd_path is not changed.
   */
  if (!uri && !cmd_path) {
#ifdef USE_QUARTZ
    char *user;

    if ((user = getenv("USER")) && (cmd_argv = alloca(sizeof(char *) * 4))) {
      cmd_argv[0] = cmd_path = "login";
      cmd_argv[1] = "-fp";
      cmd_argv[2] = user;
      cmd_argv[3] = NULL;
    } else
#endif
    {
      /*
       * SHELL env var -> /etc/passwd -> /bin/sh
       */
      if ((cmd_path = getenv("SHELL")) == NULL || *cmd_path == '\0') {
#ifndef USE_WIN32API
        struct passwd *pw;

        if ((pw = getpwuid(getuid())) == NULL || *(cmd_path = pw->pw_shell) == '\0')
#endif
        {
          cmd_path = "/bin/sh";
        }
      }
    }
  }

  /*
   * Set cmd_argv by cmd_path.
   */
  if (cmd_path && !cmd_argv) {
    char *cmd_file;

    cmd_file = bl_basename(cmd_path);

    if ((cmd_argv = alloca(sizeof(char *) * 2)) == NULL) {
      return 0;
    }

    /* 2 = `-' and NULL */
    if ((cmd_argv[0] = alloca(strlen(cmd_file) + 2)) == NULL) {
      return 0;
    }

    if (main_config.use_login_shell) {
      sprintf(cmd_argv[0], "-%s", cmd_file);
    } else {
      strcpy(cmd_argv[0], cmd_file);
    }

    cmd_argv[1] = NULL;
  }

  ret = vt_term_open_pty(term, cmd_path, cmd_argv, env, uri ? uri : display, main_config.work_dir,
                         pass,
#ifdef USE_LIBSSH2
                         main_config.public_key, main_config.private_key,
#else
                         NULL, NULL,
#endif
                         width_pix, height_pix);

#if defined(USE_WIN32API) || defined(USE_LIBSSH2)
  if (uri) {
    if (ret && bl_compare_str(uri, main_config.default_server) != 0) {
      ui_main_config_add_to_server_list(&main_config, uri);
      if (cur_default_server == main_config.default_server) {
        cur_default_server = uri;
      }
      free(main_config.default_server);
      main_config.default_server = uri;
    } else {
      free(uri);
    }

    free(pass);
  }
#endif

  return ret;
}

#ifndef NO_IMAGE

static vt_char_t *get_picture_data(void *p, char *file_path, int *num_of_cols, /* can be 0 */
                                   int *num_of_rows,                           /* can be 0 */
                                   u_int32_t **sixel_palette) {
  vt_char_t *data;

  if (num_of_screens > 0) {
    vt_term_t *orig_term;

    orig_term = screens[0]->term;
    screens[0]->term = p; /* XXX */
    data = (*screens[0]->xterm_listener.get_picture_data)(
        screens[0]->xterm_listener.self, file_path, num_of_cols, num_of_rows, sixel_palette);
    screens[0]->term = orig_term;
  } else {
    data = NULL;
  }

  return data;
}

static vt_term_t *detach_screen(ui_screen_t *screen) {
  vt_term_t *term;

  if ((term = ui_screen_detach(screen))) {
    vt_xterm_event_listener_t *listener;

    if (!(listener = vt_term_get_user_data(term, term))) {
      if (!(listener = calloc(1, sizeof(vt_xterm_event_listener_t)))) {
        return term;
      }

      listener->self = term;
      listener->get_picture_data = get_picture_data;

      vt_term_set_user_data(term, term, listener);
    }

    /* XXX */
    term->parser->xterm_listener = listener;
  }

  return term;
}

#define ui_screen_detach(screen) detach_screen(screen)

#endif

#ifdef USE_WIN32GUI
static void close_screen_win32(ui_screen_t *screen) {
  int is_orphan;

  if (UI_SCREEN_TO_LAYOUT(screen) && ui_layout_remove_child(UI_SCREEN_TO_LAYOUT(screen), screen)) {
    is_orphan = 1;
  } else {
    is_orphan = 0;

    /*
     * XXX Hack
     * In case SendMessage(WM_CLOSE) causes WM_KILLFOCUS
     * and operates screen->term which was already deleted.
     * (see window_unfocused())
     */
    screen->window.window_unfocused = NULL;
  }

  SendMessage(ui_get_root_window(&screen->window)->my_window, WM_CLOSE, 0, 0);

  if (is_orphan && screen->window.window_deleted) {
    (*screen->window.window_deleted)(&screen->window);
  }
}
#endif

static void close_screen_intern(ui_screen_t *screen) {
  ui_window_t *root;
  ui_display_t *disp;

  if (UI_SCREEN_TO_LAYOUT(screen)) {
    ui_layout_remove_child(UI_SCREEN_TO_LAYOUT(screen), screen);
  }

  ui_screen_detach(screen);
  ui_font_manager_delete(screen->font_man);
  ui_color_manager_delete(screen->color_man);

  root = ui_get_root_window(&screen->window);
  disp = root->disp;

  if (!ui_display_remove_root(disp, root)) {
    ui_window_unmap(root);
    ui_window_final(root);
  } else if (disp->num_of_roots == 0) {
    ui_display_close(disp);
  }
}

static ui_screen_t *open_screen_intern(vt_term_t *term, ui_layout_t *layout, int horizontal,
                                       const char *sep) {
  ui_display_t *disp;
  ui_screen_t *screen;
  ui_font_manager_t *font_man;
  ui_color_manager_t *color_man;
  ui_window_t *root;
  ef_charset_t usascii_font_cs;
  void *p;

  /*
   * these are dynamically allocated.
   */
  disp = NULL;
  font_man = NULL;
  color_man = NULL;
  screen = NULL;
  root = NULL;

  if (MAX_SCREENS <= num_of_screens) {
    return NULL;
  }

  if (!term) {
    if ((!layout || (term = vt_get_detached_term(NULL)) == NULL) &&
        (term = create_term_intern()) == NULL) {
      return NULL;
    }
  }

  if (layout) {
    disp = layout->window.disp;
  } else if ((disp = ui_display_open(main_config.disp_name, depth)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " ui_display_open failed.\n");
#endif

    goto error;
  }

  if (main_config.unicode_policy & NOT_USE_UNICODE_FONT || main_config.iso88591_font_for_usascii) {
    usascii_font_cs = ui_get_usascii_font_cs(VT_ISO8859_1);
  } else if (main_config.unicode_policy & ONLY_USE_UNICODE_FONT) {
    usascii_font_cs = ui_get_usascii_font_cs(VT_UTF8);
  } else {
    usascii_font_cs = ui_get_usascii_font_cs(vt_term_get_encoding(term));
  }

  if ((font_man = ui_font_manager_new(
           disp->display, main_config.type_engine, main_config.font_present, main_config.font_size,
           usascii_font_cs, main_config.use_multi_col_char, main_config.step_in_changing_font_size,
           main_config.letter_space, main_config.use_bold_font, main_config.use_italic_font)) ==
      NULL) {
    char *name;

    name = ui_get_charset_name(usascii_font_cs);

    bl_msg_printf(
        "No fonts found for %s. Please install fonts "
        "and edit the font config file in ~/.mlterm.\n",
        name ? name : "US-ASCII");

#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " ui_font_manager_new() failed.\n");
#endif

    goto error;
  }

  if ((color_man = ui_color_manager_new(
           disp, main_config.fg_color, main_config.bg_color, main_config.cursor_fg_color,
           main_config.cursor_bg_color, main_config.bd_color, main_config.it_color,
           main_config.ul_color, main_config.bl_color, main_config.co_color)) == NULL) {
    goto error;
  }

  if ((screen = ui_screen_new(
           term, font_man, color_man, main_config.brightness, main_config.contrast,
           main_config.gamma, main_config.alpha, main_config.fade_ratio, &shortcut,
           main_config.screen_width_ratio, main_config.screen_height_ratio,
           main_config.mod_meta_key, main_config.mod_meta_mode, main_config.bel_mode,
           main_config.receive_string_via_ucs, main_config.pic_file_path, main_config.use_transbg,
           main_config.use_vertical_cursor, main_config.big5_buggy,
           main_config.use_extended_scroll_shortcut, main_config.borderless, main_config.line_space,
           main_config.input_method, main_config.allow_osc52, main_config.blink_cursor,
           main_config.hmargin, main_config.vmargin, main_config.hide_underline)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " ui_screen_new() failed.\n");
#endif

    goto error;
  }

  /* Override config event listener. */
  screen->config_listener.saved = config_saved;

  ui_set_system_listener(screen, &system_listener);

  if (layout) {
    if (!ui_layout_add_child(layout, screen, horizontal, sep)) {
      layout = NULL;

      goto error;
    }

    root = &layout->window;
  } else {
    if (main_config.use_mdi &&
        (layout = ui_layout_new(screen, main_config.scrollbar_view_name, main_config.sb_fg_color,
                                main_config.sb_bg_color, main_config.sb_mode,
                                main_config.layout_hmargin, main_config.layout_vmargin))) {
      root = &layout->window;
    } else {
      root = &screen->window;
    }

    if (!ui_display_show_root(disp, root, main_config.x, main_config.y, main_config.geom_hint,
                              main_config.app_name, main_config.parent_window)) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " ui_display_show_root() failed.\n");
#endif

      goto error;
    }
  }

  if ((p = realloc(screens, sizeof(ui_screen_t *) * (num_of_screens + 1))) == NULL) {
    /*
     * XXX
     * After ui_display_show_root() screen is not deleted correctly by
     * 'goto error'(see following error handling in open_pty_intern),
     * but I don't know how to do.
     */
    goto error;
  }

  screens = p;

  /*
   * New screen is successfully created here except vt_pty.
   */

  if (vt_term_pty_is_opened(term)) {
#if 0
    /* mlclient /dev/... -e foo */
    if (main_config.cmd_argv) {
      int count;
      for (count = 0; main_config.cmd_argv[count]; count++) {
        vt_term_write(term, main_config.cmd_argv[count], strlen(main_config.cmd_argv[count]), 0);
        vt_term_write(term, " ", 1, 0);
      }

      vt_term_write(term, "\n", 1, 0);
    }
#endif
  } else {
    if (!open_pty_intern(term, main_config.cmd_path, main_config.cmd_argv,
                         DisplayString(disp->display), screen->window.my_window,
                         screen->window.width, screen->window.height)) {
      ui_screen_detach(screen);
      vt_destroy_term(term);

#ifdef USE_WIN32GUI
      screens[num_of_screens++] = screen;
      close_screen_win32(screen);
#else
      close_screen_intern(screen);
#endif

      return NULL;
    }

    if (main_config.init_str) {
      vt_term_write(term, main_config.init_str, strlen(main_config.init_str));
    }
  }

  /* Don't add screen to screens before "return NULL" above unless USE_WIN32GUI.
   */
  screens[num_of_screens++] = screen;

  return screen;

error:
  if (font_man) {
    ui_font_manager_delete(font_man);
  }

  if (color_man) {
    ui_color_manager_delete(color_man);
  }

  if (!root || !ui_display_remove_root(disp, root)) {
    /*
     * If root is still NULL or is not registered to disp yet.
     */

    if (screen) {
      ui_screen_delete(screen);
    }

    if (layout) {
      ui_layout_delete(layout);
    }
  }

  if (disp && disp->num_of_roots == 0) {
    ui_display_close(disp);
  }

  vt_destroy_term(term);

  return NULL;
}

/*
 * callbacks of ui_system_event_listener_t
 */

/*
 * EXIT_PROGRAM shortcut calls this at last.
 * this is for debugging.
 */
#ifdef DEBUG
#include "../main/main_loop.h"

static void __exit(void *p, int status) {
#ifdef USE_WIN32GUI
  u_int count;

  for (count = 0; count < num_of_screens; count++) {
    SendMessage(ui_get_root_window(&screens[count]->window), WM_CLOSE, 0, 0);
  }
#endif
#if 1
  bl_mem_dump_all();
#endif

  main_loop_final();

#if defined(USE_WIN32API) && defined(USE_LIBSSH2)
  WSACleanup();
#endif

  bl_dl_close_all();

  bl_msg_printf("reporting unfreed memories --->\n");

  bl_alloca_garbage_collect();
  bl_mem_free_all();

  exit(status);
}
#endif

static void open_pty(void *p, ui_screen_t *screen, char *dev) {
  vt_term_t *new;

  if (dev) {
    if ((new = vt_get_detached_term(dev)) == NULL) {
      return;
    }
  } else {
    vt_char_encoding_t encoding;
#if defined(USE_WIN32API) || defined(USE_LIBSSH2)
    char *default_server;
    int show_dialog;
    char *new_cmd_line;
    char *cmd_path;
    char **cmd_argv;
#endif
    int ret;

    if ((new = create_term_intern()) == NULL) {
      return;
    }

    encoding = main_config.encoding;
    main_config.encoding = vt_term_get_encoding(screen->term);
#if defined(USE_WIN32API) || defined(USE_LIBSSH2)
    default_server = main_config.default_server;
    show_dialog = main_config.show_dialog;

    if (!main_config.default_server ||
        bl_compare_str(main_config.default_server, cur_default_server) == 0) {
      main_config.default_server = vt_term_get_uri(screen->term);
      /*
       * If show_dialog == 1, main_config.default_server can be
       * free'ed in open_pty_intern.
       */
      main_config.show_dialog = 0;
    }

    if ((new_cmd_line = vt_term_get_cmd_line(screen->term)) &&
        (new_cmd_line = bl_str_alloca_dup(new_cmd_line))) {
      int argc;

      cmd_path = main_config.cmd_path;
      cmd_argv = main_config.cmd_argv;

      main_config.cmd_argv = bl_arg_str_to_array(&argc, new_cmd_line);
      main_config.cmd_path = main_config.cmd_argv[0];
    }
#endif

    ret = open_pty_intern(new, main_config.cmd_path, main_config.cmd_argv,
                          DisplayString(screen->window.disp->display), screen->window.my_window,
                          screen->window.width, screen->window.height);

    main_config.encoding = encoding;
#if defined(USE_WIN32API) || defined(USE_LIBSSH2)
    main_config.default_server = default_server;
    main_config.show_dialog = show_dialog;

    if (new_cmd_line) {
      main_config.cmd_path = cmd_path;
      main_config.cmd_argv = cmd_argv;
    }
#endif

    if (!ret) {
      vt_destroy_term(new);

      return;
    }
  }

  ui_screen_detach(screen);
  ui_screen_attach(screen, new);
}

static void next_pty(void *p, ui_screen_t *screen) {
  vt_term_t *old;
  vt_term_t *new;

  if ((old = ui_screen_detach(screen)) == NULL) {
    return;
  }

  if ((new = vt_next_term(old)) == NULL) {
    ui_screen_attach(screen, old);
  } else {
    ui_screen_attach(screen, new);
  }
}

static void prev_pty(void *p, ui_screen_t *screen) {
  vt_term_t *old;
  vt_term_t *new;

  if ((old = ui_screen_detach(screen)) == NULL) {
    return;
  }

  if ((new = vt_prev_term(old)) == NULL) {
    ui_screen_attach(screen, old);
  } else {
    ui_screen_attach(screen, new);
  }
}

static void close_pty(void *p, ui_screen_t *screen, char *dev) {
  vt_term_t *term;

  if (dev) {
    if ((term = vt_get_term(dev)) == NULL) {
      return;
    }
  } else {
    term = screen->term;
  }

  /*
   * Don't call vt_destroy_term directly, because close_pty() can be called
   * in the context of parsing vt100 sequence.
   */
  bl_trigger_sig_child(vt_term_get_child_pid(term));
}

static void pty_closed(void *p, ui_screen_t *screen /* screen->term was already deleted. */
                       ) {
  int count;

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " pty which is attached to screen %p is closed.\n", screen);
#endif

  for (count = num_of_screens - 1; count >= 0; count--) {
    if (screen == screens[count]) {
      vt_term_t *term;

      if ((term = vt_get_detached_term(NULL)) == NULL) {
#ifdef __DEBUG
        bl_debug_printf(" no detached term. closing screen.\n");
#endif

#ifdef USE_WIN32GUI
        close_screen_win32(screen);
#else
        screens[count] = screens[--num_of_screens];
        close_screen_intern(screen);
#endif
      } else {
#ifdef __DEBUG
        bl_debug_printf(" using detached term.\n");
#endif

        ui_screen_attach(screen, term);
      }

      return;
    }
  }
}

static void open_or_split_screen(ui_screen_t *cur_screen, /* Screen which triggers this event. */
                                 ui_layout_t *layout, int horizontal, const char *sep,
                                 int retain_encoding, int retain_server) {
  char *disp_name;
  vt_char_encoding_t encoding;
#if defined(USE_WIN32API) || defined(USE_LIBSSH2)
  char *default_server;
  int show_dialog;
  char *new_cmd_line;
  char *cmd_path;
  char **cmd_argv;
#endif

  disp_name = main_config.disp_name;
  main_config.disp_name = cur_screen->window.disp->name;
  if (!retain_encoding) {
    encoding = main_config.encoding;
    main_config.encoding = vt_term_get_encoding(cur_screen->term);
  }
#if defined(USE_WIN32API) || defined(USE_LIBSSH2)
  if (!retain_server) {
    default_server = main_config.default_server;
    main_config.default_server = vt_term_get_uri(cur_screen->term);
  }
  /*
   * If show_dialog == 1, main_config.default_server can be
   * free'ed in open_pty_intern.
   */
  show_dialog = main_config.show_dialog;
  main_config.show_dialog = 0;

  if ((new_cmd_line = vt_term_get_cmd_line(cur_screen->term)) &&
      (new_cmd_line = bl_str_alloca_dup(new_cmd_line))) {
    int argc;

    cmd_path = main_config.cmd_path;
    cmd_argv = main_config.cmd_argv;

    main_config.cmd_argv = bl_arg_str_to_array(&argc, new_cmd_line);
    main_config.cmd_path = main_config.cmd_argv[0];
  }
#endif

  if (!open_screen_intern(NULL, layout, horizontal, sep)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " open_screen_intern failed.\n");
#endif
  }

  main_config.disp_name = disp_name;
  if (!retain_encoding) {
    main_config.encoding = encoding;
  }
#if defined(USE_WIN32API) || defined(USE_LIBSSH2)
  if (!retain_server) {
    main_config.default_server = default_server;
  }
  main_config.show_dialog = show_dialog;

  if (new_cmd_line) {
    main_config.cmd_path = cmd_path;
    main_config.cmd_argv = cmd_argv;
  }
#endif
}

static void open_screen(void *p, ui_screen_t *screen /* Screen which triggers this event. */
                        ) {
  open_or_split_screen(screen, NULL, 0, 0, 0, 0);
}

static void split_screen(void *p, ui_screen_t *screen, /* Screen which triggers this event. */
                         int horizontal, const char *sep) {
  if (UI_SCREEN_TO_LAYOUT(screen)) {
    open_or_split_screen(screen, UI_SCREEN_TO_LAYOUT(screen), horizontal, sep, 0, 0);
  }
}

static int close_screen(void *p, ui_screen_t *screen, /* Screen which triggers this event. */
                        int force) {
  u_int count;

  if (!force &&
      (!UI_SCREEN_TO_LAYOUT(screen) || ui_layout_has_one_child(UI_SCREEN_TO_LAYOUT(screen)))) {
    return 0;
  }

  for (count = 0; count < num_of_screens; count++) {
    u_int idx;

    if (screen != screens[count]) {
      continue;
    }

#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " screen %p is registered to be closed.\n", screen);
#endif

    idx = count / MSU; /* count / 8 */
    dead_mask[idx] |= (1 << (count - MSU * idx));

    break;
  }

  return 1;
}

static int next_screen(void *self, ui_screen_t *screen) {
  if (UI_SCREEN_TO_LAYOUT(screen)) {
    return ui_layout_switch_screen(UI_SCREEN_TO_LAYOUT(screen), 0);
  } else {
    return 0;
  }
}

static int prev_screen(void *self, ui_screen_t *screen) {
  if (UI_SCREEN_TO_LAYOUT(screen)) {
    return ui_layout_switch_screen(UI_SCREEN_TO_LAYOUT(screen), 1);
  } else {
    return 0;
  }
}

static int resize_screen(void *self, ui_screen_t *screen, int horizontal, int step) {
  if (UI_SCREEN_TO_LAYOUT(screen)) {
    return ui_layout_resize(UI_SCREEN_TO_LAYOUT(screen), screen, horizontal, step);
  } else {
    return 0;
  }
}

static int mlclient(void *self, ui_screen_t *screen, char *args,
                    FILE *fp /* Stream to output response of mlclient. */
                    ) {
  char **argv;
  int argc;

  argv = bl_arg_str_to_array(&argc, args);

#ifdef __DEBUG
  {
    int i;

    for (i = 0; i < argc; i++) {
      bl_msg_printf("%s\n", argv[i]);
    }
  }
#endif

  if (argc == 0
#if defined(USE_FRAMEBUFFER)
      || screen == NULL
#endif
      ) {
    return 0;
  }

  if (argc == 2 && (strcmp(argv[1], "-P") == 0 || strcmp(argv[1], "--ptylist") == 0)) {
    /*
     * mlclient -P or mlclient --ptylist
     */

    vt_term_t **terms;
    u_int num;
    int count;

    num = vt_get_all_terms(&terms);
    for (count = 0; count < num; count++) {
      fprintf(fp, "#%s", vt_term_get_slave_name(terms[count]));
      if (vt_term_window_name(terms[count])) {
        fprintf(fp, "(whose title is %s)", vt_term_window_name(terms[count]));
      }
      if (vt_term_is_attached(terms[count])) {
        fprintf(fp, " is active:)\n");
      } else {
        fprintf(fp, " is sleeping.zZ\n");
      }
    }
  } else {
    bl_conf_t *conf;
    ui_main_config_t orig_conf;
    char *pty;
    int horizontal;
    int has_km_option;
    int has_serv_option;
    char *sep;
#if defined(USE_WIN32API) || defined(USE_LIBSSH2)
    char **server_list;
#endif

    if (argc >= 2 && *(argv[1]) != '-') {
      /*
       * mlclient [dev] [options...]
       */

      pty = argv[1];
      argv[1] = argv[0];
      argv = &argv[1];
      argc--;
    } else {
      pty = NULL;
    }

    if ((conf = bl_conf_new()) == NULL) {
      return 0;
    }

    ui_prepare_for_main_config(conf);

    bl_conf_add_opt(conf, '\0', "hsep", 0, "hsep", "");
    bl_conf_add_opt(conf, '\0', "vsep", 0, "vsep", "");

    if (!bl_conf_parse_args(conf, &argc, &argv, 1)) {
      bl_conf_delete(conf);

      return 0;
    }

    has_km_option = (bl_conf_get_value(conf, "encoding") != NULL);
    has_serv_option = (bl_conf_get_value(conf, "default_server") != NULL);

    if (screen && UI_SCREEN_TO_LAYOUT(screen)) {
      if ((sep = bl_conf_get_value(conf, "hsep"))) {
        horizontal = 1;
      }
      else if ((sep = bl_conf_get_value(conf, "vsep"))) {
        horizontal = 0;
      }
      else {
        goto end_check_sep;
      }
      sep = bl_str_alloca_dup(sep);
    } else {
      sep = NULL;
    }

  end_check_sep:
    orig_conf = main_config;

    ui_main_config_init(&main_config, conf, argc, argv);
#if defined(USE_WIN32API) || defined(USE_LIBSSH2)
    server_list = main_config.server_list;
    main_config.server_list = orig_conf.server_list;
#endif

    bl_conf_delete(conf);

    if (screen) {
      if (sep) {
        open_or_split_screen(screen, UI_SCREEN_TO_LAYOUT(screen), horizontal, sep,
                             has_km_option, has_serv_option);
      } else {
        open_pty(self, screen, pty);
      }
    } else {
      vt_term_t *term = NULL;

      if ((pty && !(term = vt_get_detached_term(pty))) || !open_screen_intern(term, NULL, 0, 0)) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " open_screen_intern() failed.\n");
#endif
      }
    }

#if defined(USE_WIN32API) || defined(USE_LIBSSH2)
    orig_conf.server_list = main_config.server_list;
    main_config.server_list = server_list;
#endif
    ui_main_config_final(&main_config);

    main_config = orig_conf;
  }

  /* Flush fp stream because write(2) is called after this function is called.
   */
  fflush(fp);

  return 1;
}

/* --- global functions --- */

int ui_screen_manager_init(char *_mlterm_version, u_int _depth, u_int _max_screens_multiple,
                           u_int _num_of_startup_screens, ui_main_config_t *_main_config) {
  mlterm_version = _mlterm_version;

  depth = _depth;

  main_config = *_main_config;
#if defined(USE_WIN32API) || defined(USE_LIBSSH2)
  cur_default_server = main_config.default_server;
#endif

  max_screens_multiple = _max_screens_multiple;

  if ((dead_mask = calloc(sizeof(*dead_mask), max_screens_multiple)) == NULL) {
    return 0;
  }

  if (_num_of_startup_screens > MAX_SCREENS) {
    num_of_startup_screens = MAX_SCREENS;
  } else {
    num_of_startup_screens = _num_of_startup_screens;
  }

  if (!vt_color_config_init()) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " vt_color_config_init failed.\n");
#endif

    return 0;
  }

  if (!ui_shortcut_init(&shortcut)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " ui_shortcut_init failed.\n");
#endif

    return 0;
  }
/* BACKWARD COMPAT (3.1.7 or before) */
#if 1
  else {
    size_t count;
    char key0[] = "Control+Button1";
    char key1[] = "Control+Button2";
    char key2[] = "Control+Button3";
    char key3[] = "Button3";
    char *keys[] = {key0, key1, key2, key3};

    for (count = 0; count < sizeof(keys) / sizeof(keys[0]); count++) {
      if (main_config.shortcut_strs[count]) {
        ui_shortcut_parse(&shortcut, keys[count], main_config.shortcut_strs[count]);
      }
    }
  }
#endif

  if (*main_config.disp_name) {
    /*
     * setting DISPLAY environment variable to match "--display" option.
     */

    char *env;

    if ((env = malloc(strlen(main_config.disp_name) + 9))) {
      sprintf(env, "DISPLAY=%s", main_config.disp_name);
      putenv(env);
    }
  }

  system_listener.self = NULL;
#ifdef DEBUG
  system_listener.exit = __exit;
#else
  system_listener.exit = NULL;
#endif
#if defined(USE_FRAMEBUFFER) || defined(USE_CONSOLE)
  system_listener.open_screen = NULL;
#else
  system_listener.open_screen = open_screen;
#endif
  system_listener.split_screen = split_screen;
  system_listener.close_screen = close_screen;
  system_listener.next_screen = next_screen;
  system_listener.prev_screen = prev_screen;
  system_listener.resize_screen = resize_screen;
  system_listener.open_pty = open_pty;
  system_listener.next_pty = next_pty;
  system_listener.prev_pty = prev_pty;
  system_listener.close_pty = close_pty;
  system_listener.pty_closed = pty_closed;
  system_listener.mlclient = mlclient;
  system_listener.font_config_updated = font_config_updated;
  system_listener.color_config_updated = color_config_updated;

  if (!vt_term_manager_init(max_screens_multiple)) {
    free(dead_mask);

    return 0;
  }

  return 1;
}

int ui_screen_manager_final(void) {
  u_int count;

  ui_main_config_final(&main_config);

  for (count = 0; count < num_of_screens; count++) {
    close_screen_intern(screens[count]);
  }

  free(screens);
  free(dead_mask);

  vt_term_manager_final();

  ui_display_close_all();

  vt_color_config_final();
  ui_shortcut_final(&shortcut);

  return 1;
}

#ifdef __ANDROID__
static int suspended;

int ui_screen_manager_suspend(void) {
  u_int count;

  ui_close_dead_screens();

  for (count = 0; count < num_of_screens; count++) {
    close_screen_intern(screens[count]);
  }

  free(screens);
  screens = NULL;
  num_of_screens = 0;

  ui_display_close_all();

  suspended = 1;

  return 1;
}
#endif

u_int ui_screen_manager_startup(void) {
  u_int count;
  u_int num_started;

  num_started = 0;

#ifdef __ANDROID__
  if (suspended) {
    /* reload ~/.mlterm/main. */
    config_saved();
    ui_shortcut_final(&shortcut);
    ui_shortcut_init(&shortcut);
    vt_color_config_final();
    vt_color_config_init();
  }
#endif

  for (count = 0; count < num_of_startup_screens; count++) {
    if (!open_screen_intern(vt_get_detached_term(NULL), NULL, 0, 0)) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " open_screen_intern() failed.\n");
#endif
    } else {
      num_started++;
    }
  }

  return num_started;
}

int ui_close_dead_screens(void) {
  if (num_of_screens > 0) {
    int idx;

    for (idx = (num_of_screens - 1) / MSU; idx >= 0; idx--) {
      if (dead_mask[idx]) {
        int count;

        for (count = MSU - 1; count >= 0; count--) {
          if (dead_mask[idx] & (0x1 << count)) {
            ui_screen_t *screen;

#ifdef __DEBUG
            bl_debug_printf(BL_DEBUG_TAG " closing screen %d-%d.", idx, count);
#endif

            screen = screens[idx * MSU + count];
            screens[idx * MSU + count] = screens[--num_of_screens];
            close_screen_intern(screen);

#ifdef __DEBUG
            bl_msg_printf(" => Finished. Rest %d\n", num_of_screens);
#endif
          }
        }

        memset(&dead_mask[idx], 0, sizeof(dead_mask[idx]));
      }
    }
  }

  return 1;
}

u_int ui_get_all_screens(ui_screen_t ***_screens) {
  if (_screens) {
    *_screens = screens;
  }

  return num_of_screens;
}

int ui_mlclient(char *args, FILE *fp) { return mlclient(NULL, NULL, args, fp); }
