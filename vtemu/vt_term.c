/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_term.h"

#include <pobl/bl_mem.h> /* malloc/free */
#include <pobl/bl_debug.h>
#include <pobl/bl_str.h> /* strdup */
#include <pobl/bl_sig_child.h>
#include <pobl/bl_path.h> /* bl_parse_uri */
#include <pobl/bl_dialog.h>

#include "vt_pty.h"
#include "vt_parser.h"
#include "vt_screen.h"

#ifdef OPEN_PTY_ASYNC

#ifdef USE_WIN32API
#include <windows.h>
#include <process.h> /* _beginthreadex */
#else
#include <pthread.h>
#endif

typedef struct {
  vt_term_t *term;
  char *cmd_path;
  char **argv;
  char **env;
  char *host;
  char *work_dir;
  char *pass;
  char *pubkey;
  char *privkey;
  u_int width_pix;
  u_int height_pix;

} pty_args_t;

#endif

/* --- global variables --- */

#ifndef NO_IMAGE
/* XXX */
void (*vt_term_pty_closed_event)(vt_term_t *);
#endif

#if defined(__ANDROID__) && defined(USE_LIBSSH2)
/* XXX */
int start_with_local_pty = 0;
#endif

/* --- static functions --- */

#ifdef OPEN_PTY_ASYNC

static void pty_args_destroy(pty_args_t *args) {
  int count;

  free(args->cmd_path);
  free(args->host);
  free(args->work_dir);
  free(args->pass);
  free(args->pubkey);
  free(args->privkey);

  if (args->argv) {
    for (count = 0; args->argv[count]; count++) {
      free(args->argv[count]);
    }
    free(args->argv);
  }

  if (args->env) {
    for (count = 0; args->env[count]; count++) {
      free(args->env[count]);
    }
    free(args->env);
  }

  free(args);
}

static pty_args_t *pty_args_new(vt_term_t *term, const char *cmd_path, char **argv, char **env,
                                const char *host, const char *work_dir, const char *pass,
                                const char *pubkey, const char *privkey, u_int width_pix,
                                u_int height_pix) {
  pty_args_t *args;
  u_int num;
  u_int count;

  if (!(args = calloc(1, sizeof(pty_args_t)))) {
    return NULL;
  }

  args->term = term;

  if (cmd_path) {
    args->cmd_path = strdup(cmd_path);
  }

  if (host) {
    args->host = strdup(host);
  }

  if (work_dir) {
    args->work_dir = strdup(work_dir);
  }

  if (pass) {
    args->pass = strdup(pass);
  }

  if (pubkey) {
    args->pubkey = strdup(pubkey);
  }

  if (privkey) {
    args->privkey = strdup(privkey);
  }

  args->width_pix = width_pix;
  args->height_pix = height_pix;

  if (argv) {
    for (num = 0; argv[num]; num++)
      ;

    if ((args->argv = malloc(sizeof(char *) * (num + 1)))) {
      for (count = 0; count < num; count++) {
        args->argv[count] = strdup(argv[count]);
      }
      args->argv[count] = NULL;
    }
  } else {
    args->argv = NULL;
  }

  if (env) {
    for (num = 0; env[num]; num++)
      ;

    if ((args->env = malloc(sizeof(char *) * (num + 1)))) {
      for (count = 0; count < num; count++) {
        args->env[count] = strdup(env[count]);
      }
      args->env[count] = NULL;
    }
  } else {
    args->env = NULL;
  }

  return args;
}

#ifdef USE_WIN32API
static u_int __stdcall
#else
static void *
#endif
    open_pty(void *p) {
  pty_args_t *args;
  vt_pty_t *pty;
#ifdef USE_WIN32API
  static HANDLE mutex;

  if (!mutex) {
    mutex = CreateMutex(NULL, FALSE, NULL);
  }

  WaitForSingleObject(mutex, INFINITE);
#else
  static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

  pthread_detach(pthread_self());
  pthread_mutex_lock(&mutex);
#endif

  args = p;

  pty =
      vt_pty_new(args->cmd_path, args->argv, args->env, args->host, args->work_dir, args->pass,
                 args->pubkey, args->privkey, vt_screen_get_logical_cols(args->term->screen),
                 vt_screen_get_logical_rows(args->term->screen), args->width_pix, args->height_pix);

  if (pty) {
    if (args->pass) {
      args->term->uri = strdup(args->host);
    }

    vt_term_plug_pty(args->term, pty);
  } else {
    bl_dialog(BL_DIALOG_ALERT, "Failed to open pty");

    args->term->return_special_pid = 1;
    bl_trigger_sig_child(-10);
    args->term->return_special_pid = 0;
  }

  pty_args_destroy(args);

#ifdef USE_WIN32API
  ReleaseMutex(mutex);
#else
  pthread_mutex_unlock(&mutex);
#endif

  return 0;
}

#endif

/* Must be called in visual context. */
static void set_use_local_echo(vt_term_t *term, int flag) {
  if (term->use_local_echo != flag && !(term->use_local_echo = flag)) {
    vt_screen_logical(term->screen);
    vt_screen_disable_local_echo(term->screen);
    vt_screen_visual(term->screen);
  }
}

/* --- global functions --- */

void vt_term_final(void) {
  vt_parser_final();
  vt_termcap_final();
}

vt_term_t *vt_term_new(const char *term_type, u_int cols, u_int rows, u_int tab_size,
                       u_int log_size, vt_char_encoding_t encoding, int is_auto_encoding,
                       int use_auto_detect, int logging_vt_seq, vt_unicode_policy_t policy,
                       u_int col_size_a, int use_char_combining, int use_multi_col_char,
                       int use_ctl, vt_bidi_mode_t bidi_mode, const char *bidi_separators,
                       int use_dynamic_comb, vt_bs_mode_t bs_mode, vt_vertical_mode_t vertical_mode,
                       int use_local_echo, const char *win_name, const char *icon_name,
                       int use_ansi_colors, vt_alt_color_mode_t alt_color_mode, int use_ot_layout,
                       vt_cursor_style_t cursor_style, int ignore_broadcasted_chars) {
  vt_termcap_ptr_t termcap;
  vt_term_t *term;

  if (!(termcap = vt_termcap_get(term_type))) {
    return NULL;
  }

  if ((term = calloc(1, sizeof(vt_term_t))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif

    return NULL;
  }

  if (!(term->screen = vt_screen_new(cols, rows, tab_size, log_size,
                                     vt_termcap_bce_is_enabled(termcap), bs_mode))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " vt_screen_new failed.\n");
#endif

    goto error;
  }

  term->use_ot_layout = use_ot_layout;

#ifndef NOT_CONVERT_TO_ISCII
#ifdef USE_HARFBUZZ
  if (!term->use_ot_layout)
#endif
  {
    policy |= CONVERT_UNICODE_TO_ISCII;
  }
#endif

  if (!(term->parser = vt_parser_new(term->screen, termcap, encoding, is_auto_encoding,
                                     use_auto_detect, logging_vt_seq, policy, col_size_a,
                                     use_char_combining, use_multi_col_char, win_name, icon_name,
                                     use_ansi_colors, alt_color_mode, cursor_style,
                                     ignore_broadcasted_chars))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " vt_parser_new failed.\n");
#endif

    goto error;
  }

  if (bidi_separators) {
    term->bidi_separators = bl_str_unescape(bidi_separators);
  }

  term->vertical_mode = vertical_mode;
  term->bidi_mode = bidi_mode;
  term->use_ctl = use_ctl;
  term->use_dynamic_comb = use_dynamic_comb;
  term->use_local_echo = use_local_echo;

  return term;

error:
  if (term->screen) {
    vt_screen_destroy(term->screen);
  }

  if (term->parser) {
    vt_parser_destroy(term->parser);
  }

  free(term);

  return NULL;
}

void vt_term_destroy(vt_term_t *term) {
#ifndef NO_IMAGE
  if (vt_term_pty_closed_event) {
    (*vt_term_pty_closed_event)(term);
  }
#endif

  free(term->user_data);

  if (term->pty) {
    vt_pty_destroy(term->pty);
  } else if (term->pty_listener) {
    (*term->pty_listener->closed)(term->pty_listener->self);
  }

  free(term->uri);
  free(term->icon_path);
  free(term->bidi_separators);

  vt_screen_destroy(term->screen);
  vt_parser_destroy(term->parser);

  free(term);
}

void vt_term_zombie(vt_term_t *term) {
  if (term->pty) {
    vt_pty_t *pty;

    pty = term->pty;

    /* Should be NULL because vt_pty_destroy calls term->pty_listener->closed. */
    term->pty = NULL;

    vt_pty_destroy(pty);
  }
#ifdef DEBUG
  else {
    bl_debug_printf(BL_DEBUG_TAG " term is already zombie.\n");
  }
#endif
}

/* The caller should swap width_pix and height_pix in vertical mode. */
int vt_term_open_pty(vt_term_t *term, const char *cmd_path, char **argv, char **env,
                     const char *host, const char *work_dir, const char *pass, const char *pubkey,
                     const char *privkey, u_int width_pix, u_int height_pix) {
  if (!term->pty) {
#ifdef OPEN_PTY_ASYNC
    char *host_dup;
    char *user;
    char *server;
    char *port;

    if (pass && (host_dup = alloca(strlen(host) + 1)) &&
        bl_parse_uri(NULL, &user, &server, &port, NULL, NULL, strcpy(host_dup, host))
#ifdef USE_LIBSSH2
        && !vt_search_ssh_session(server, port, user)
#endif
        ) {
      pty_args_t *args;

      if (!(args = pty_args_new(term, cmd_path, argv, env, host, work_dir, pass, pubkey, privkey,
                                width_pix, height_pix))) {
        return 0;
      }

#ifdef USE_WIN32API
      {
        HANDLE thrd;
        u_int tid;

        if ((thrd = _beginthreadex(NULL, 0, open_pty, args, 0, &tid))) {
          CloseHandle(thrd);

          return 1;
        }

        return 0;
      }
#else
      {
        pthread_t thrd;

        if (pthread_create(&thrd, NULL, open_pty, args) == 0) {
          return 1;
        } else {
          return 0;
        }
      }
#endif
    } else
#endif /* OPEN_PTY_ASYNC */
    {
      vt_pty_t *pty;

      if (!(pty = vt_pty_new(cmd_path, argv, env, host, work_dir, pass, pubkey, privkey,
                             vt_screen_get_logical_cols(term->screen),
                             vt_screen_get_logical_rows(term->screen), width_pix, height_pix))) {
        bl_dialog(BL_DIALOG_ALERT, "Failed to open pty");

        return 0;
      }

      if (pass) {
        term->uri = strdup(host);
      }

      vt_term_plug_pty(term, pty);
    }
  }

  return 1;
}

int vt_term_plug_pty(vt_term_t *term, vt_pty_t *pty /* Not NULL */) {
  if (!term->pty) {
    if (term->pty_listener) {
      vt_pty_set_listener(pty, term->pty_listener);
      term->pty_listener = NULL;
    }

    vt_parser_set_pty(term->parser, pty);

    term->pty = pty;
  }

  return 1;
}

int vt_term_attach(vt_term_t *term, vt_xterm_event_listener_t *xterm_listener,
                   vt_config_event_listener_t *config_listener,
                   vt_screen_event_listener_t *screen_listener,
                   vt_pty_event_listener_t *pty_listener) {
  if (term->is_attached) {
    /* already attached */
    return 0;
  }

  vt_parser_set_xterm_listener(term->parser, xterm_listener);
  vt_parser_set_config_listener(term->parser, config_listener);
  vt_screen_set_listener(term->screen, screen_listener);

  if (term->pty) {
    vt_pty_set_listener(term->pty, pty_listener);
  } else {
    term->pty_listener = pty_listener;
  }

  term->is_attached = 1;

  return 1;
}

int vt_term_detach(vt_term_t *term) {
  if (!term->is_attached) {
    /* already detached. */
    return 0;
  }

  vt_parser_set_xterm_listener(term->parser, NULL);
  vt_parser_set_config_listener(term->parser, NULL);
  vt_screen_set_listener(term->screen, NULL);

  if (term->pty) {
    vt_pty_set_listener(term->pty, NULL);
  } else {
    term->pty_listener = NULL;
  }

  term->is_attached = 0;

  return 1;
}

void vt_term_set_use_ot_layout(vt_term_t *term, int flag) {
#if defined(USE_HARFBUZZ) && !defined(NOT_CONVERT_TO_ISCII)
  vt_unicode_policy_t policy;

  policy = vt_parser_get_unicode_policy(term->parser);

  if (flag) {
    policy &= ~CONVERT_UNICODE_TO_ISCII;
  } else {
    policy |= CONVERT_UNICODE_TO_ISCII;
  }

  vt_parser_set_unicode_policy(term->parser, policy);
#endif

  term->use_ot_layout = flag;
}

int vt_term_get_master_fd(vt_term_t *term) {
  if (term->pty == NULL) {
    return -1;
  }

  return vt_pty_get_master_fd(term->pty);
}

int vt_term_get_slave_fd(vt_term_t *term) {
  if (term->pty == NULL) {
    return -1;
  }

  return vt_pty_get_slave_fd(term->pty);
}

/*
 * Always return non-NULL value.
 * XXX Static data can be returned. (Not reentrant)
 */
char *vt_term_get_slave_name(vt_term_t *term) {
  if (term->pty == NULL) {
    return "/dev/zombie";
  }

  return vt_pty_get_slave_name(term->pty);
}

pid_t vt_term_get_child_pid(vt_term_t *term) {
  if (term->pty == NULL) {
#ifdef OPEN_PTY_ASYNC
    return term->return_special_pid ? -10 : -1;
#else
    return -1;
#endif
  }

  return vt_pty_get_pid(term->pty);
}

pid_t vt_term_get_pty_mode(vt_term_t *term) {
  if (term->pty == NULL) {
    return PTY_NONE;
  }

  return vt_pty_get_mode(term->pty);
}

size_t vt_term_write(vt_term_t *term, u_char *buf, size_t len) {
  if (term->pty == NULL) {
    return 0;
  }

  if (term->use_local_echo) {
    vt_parser_local_echo(term->parser, buf, len);
  }

  return vt_write_to_pty(term->pty, buf, len);
}

/* The caller should swap width_pix and height_pix in vertical mode. */
int vt_term_resize(vt_term_t *term, u_int cols, u_int rows, u_int width_pix, u_int height_pix) {
  vt_screen_logical(term->screen);
  vt_screen_resize(term->screen, cols, rows);
  vt_screen_render(term->screen);
  vt_screen_visual(term->screen);

  if (term->pty) {
    /* Don't use cols and rows because status line might change rows in vt_screen_resize(). */
    vt_set_pty_winsize(term->pty, vt_screen_get_logical_cols(term->screen),
                       vt_screen_get_logical_rows(term->screen), width_pix, height_pix);
  }

  return 1;
}

int vt_term_unhighlight_cursor(vt_term_t *term, int revert_visual) {
  vt_line_t *line;
  int ret;

#ifdef DEBUG
  if (term->screen->logvis && !term->screen->logvis->is_visual) {
    bl_debug_printf(BL_DEBUG_TAG
                    " vt_term_unhighlight_cursor() should be called in visual context but"
                    " is called in logical context.\n");
  }
#endif

  vt_screen_logical(term->screen);

  if ((line = vt_screen_get_cursor_line(term->screen)) == NULL || vt_line_is_empty(line)) {
    ret = 0;
  } else {
    vt_line_set_modified(line, vt_screen_cursor_char_index(term->screen),
                         vt_screen_cursor_char_index(term->screen));

    ret = 1;
  }

  if (revert_visual) {
    /* vt_screen_render(term->screen); */
    vt_screen_visual(term->screen);
  }

  return ret;
}

/*
 * Not implemented yet.
 */
#if 0
void vt_term_set_modified_region(vt_term_t *term, int beg_char_index, int beg_row, u_int nchars,
                                 u_int nrows) {
  return;
}
#endif

/*
 * Not used.
 */
#if 0
void vt_term_set_modified_region_in_screen(vt_term_t *term, int beg_char_index, int beg_row,
                                           u_int nchars, u_int nrows) {
  int row;
  vt_line_t *line;
  int revert_to_visual;

  /*
   * This function is usually called in visual context, and sometimes
   * called in logical context. (see flush_scroll_cache() in x_screen.c)
   */
  if (!vt_screen_logical_visual_is_reversible(term->screen) && vt_screen_logical(term->screen)) {
    revert_to_visual = 1;
  } else {
    revert_to_visual = 0;
  }

  for (row = beg_row; row < beg_row + nrows; row++) {
    if ((line = vt_screen_get_line_in_screen(term->screen, row))) {
      vt_line_set_modified(line, beg_char_index, beg_char_index + nchars - 1);
    }
  }

  if (revert_to_visual) {
    /* vt_screen_render(term->screen); */
    vt_screen_visual(term->screen);
  }
}
#endif

void vt_term_set_modified_lines(vt_term_t *term, int beg, int end) {
  int row;
  vt_line_t *line;
  int revert_to_visual;

  /*
   * This function is usually called in visual context, and sometimes
   * called in logical context. (see flush_scroll_cache() in x_screen.c)
   */
  if (!vt_screen_logical_visual_is_reversible(term->screen) && vt_screen_logical(term->screen)) {
    revert_to_visual = 1;
  } else {
    revert_to_visual = 0;
  }

  for (row = beg; row <= end; row++) {
    if ((line = vt_screen_get_line(term->screen, row))) {
      vt_line_set_modified_all(line);
    }
  }

  if (revert_to_visual) {
    /* vt_screen_render(term->screen); */
    vt_screen_visual(term->screen);
  }
}

void vt_term_set_modified_lines_in_screen(vt_term_t *term, int beg, int end) {
  int row;
  vt_line_t *line;
  int revert_to_visual;

  /*
   * This function is usually called in visual context, and sometimes
   * called in logical context. (see flush_scroll_cache() in x_screen.c)
   */
  if (!vt_screen_logical_visual_is_reversible(term->screen) && vt_screen_logical(term->screen)) {
    revert_to_visual = 1;
  } else {
    revert_to_visual = 0;
  }

  for (row = beg; row <= end; row++) {
    if ((line = vt_screen_get_line_in_screen(term->screen, row))) {
      vt_line_set_modified_all(line);
    }
  }

  if (revert_to_visual) {
    /* vt_screen_render(term->screen); */
    vt_screen_visual(term->screen);
  }
}

void vt_term_set_modified_all_lines_in_screen(vt_term_t *term) {
  int revert_to_visual;

  /*
   * This function is usually called in visual context, and sometimes
   * called in logical context. (see flush_scroll_cache() in x_screen.c)
   */
  if (!vt_screen_logical_visual_is_reversible(term->screen) && vt_screen_logical(term->screen)) {
    revert_to_visual = 1;
  } else {
    revert_to_visual = 0;
  }

  vt_screen_set_modified_all(term->screen);

  if (revert_to_visual) {
    /* vt_screen_render(term->screen); */
    vt_screen_visual(term->screen);
  }
}

void vt_term_updated_all(vt_term_t *term) {
  int row;
  vt_line_t *line;

#ifdef DEBUG
  if (term->screen->logvis && !term->screen->logvis->is_visual) {
    bl_debug_printf(BL_DEBUG_TAG
                    " vt_term_updated_all() should be called in visual context but"
                    " is called in logical context.\n");
  }
#endif

  if (!vt_screen_logical_visual_is_reversible(term->screen)) {
    vt_screen_logical(term->screen);
  }

  for (row = 0; row < vt_edit_get_rows(term->screen->edit); row++) {
    if ((line = vt_screen_get_line_in_screen(term->screen, row))) {
      vt_line_set_updated(line);
    }
  }

  if (!vt_screen_logical_visual_is_reversible(term->screen)) {
    /* vt_screen_render(term->screen); */
    vt_screen_visual(term->screen);
  }
}

/*
 * Return value:
 *  1 => Updated
 *  0 => Not updated(== not necessary to redraw)
 */
int vt_term_update_special_visual(vt_term_t *term) {
  vt_logical_visual_t *logvis;
  int had_logvis = 0;
  int has_logvis = 0;

  had_logvis = vt_screen_destroy_logical_visual(term->screen);

  if (term->use_dynamic_comb) {
    if ((logvis = vt_logvis_comb_new())) {
      if (vt_screen_add_logical_visual(term->screen, logvis)) {
        has_logvis = 1;

        if (vt_parser_is_using_char_combining(term->parser)) {
          bl_msg_printf(
              "Set use_combining=false forcibly "
              "to enable use_dynamic_comb.\n");
          vt_parser_set_use_char_combining(term->parser, 0);
        }
      } else {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " vt_screen_add_logical_visual failed.\n");
#endif

        (*logvis->destroy)(logvis);
      }
    }
#ifdef DEBUG
    else {
      bl_warn_printf(BL_DEBUG_TAG " vt_logvis_comb_new() failed.\n");
    }
#endif
  }

  /* Vertical mode, BiDi and ISCII can't coexist. */

  /* Similar if-else conditions exist in update_special_visual in x_screen.c. */
  if (term->vertical_mode) {
    if ((logvis = vt_logvis_vert_new(term->vertical_mode))) {
      if (vt_screen_add_logical_visual(term->screen, logvis)) {
        has_logvis = 1;
      } else {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " vt_screen_add_logical_visual failed.\n");
#endif

        (*logvis->destroy)(logvis);
      }
    }
#ifdef DEBUG
    else {
      bl_warn_printf(BL_DEBUG_TAG " vt_logvis_vert_new() failed.\n");
    }
#endif
  } else if (term->use_ctl && (vt_term_get_encoding(term) == VT_UTF8 ||
                               IS_ISCII_ENCODING(vt_term_get_encoding(term)))) {
    if ((logvis = vt_logvis_ctl_new(term->bidi_mode, term->bidi_separators,
                                    term->use_ot_layout ? term : NULL))) {
      if (vt_screen_add_logical_visual(term->screen, logvis)) {
        has_logvis = 1;
      } else {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " vt_screen_add_logical_visual failed.\n");
#endif

        (*logvis->destroy)(logvis);
      }
    }
#ifdef DEBUG
    else {
      bl_warn_printf(BL_DEBUG_TAG " vt_logvis_ctl_new() failed.\n");
    }
#endif
  }

  if (had_logvis || has_logvis) {
    vt_screen_render(term->screen);
    vt_screen_visual(term->screen);

    return 1;
  } else {
    return 0;
  }
}

void vt_term_enter_backscroll_mode(vt_term_t *term) {
  /* XXX */
  if (term->vertical_mode) {
    bl_msg_printf("Not supported backscrolling in vertical mode.\n");

    return;
  }

  return vt_enter_backscroll_mode(term->screen);
}

void vt_term_set_icon_path(vt_term_t *term, const char *path) {
  free(term->icon_path);

  if (path && *path) {
    term->icon_path = strdup(path);
  } else {
    term->icon_path = NULL;
  }
}

void vt_term_set_bidi_separators(vt_term_t *term, const char *bidi_separators) {
  free(term->bidi_separators);

  if (bidi_separators && *bidi_separators) {
    term->bidi_separators = bl_str_unescape(bidi_separators);
  } else {
    term->bidi_separators = NULL;
  }
}

int vt_term_get_config(vt_term_t *term, vt_term_t *output, /* if term == output, NULL is set */
                       char *key, int to_menu, int *flag) {
  char *value;

  if (vt_parser_get_config(term->parser, output ? output->pty : NULL, key, to_menu, flag)) {
    return 1;
  }

  if (strcmp(key, "vertical_mode") == 0) {
    value = vt_get_vertical_mode_name(term->vertical_mode);
  } else if (strcmp(key, "use_dynamic_comb") == 0) {
    if (term->use_dynamic_comb) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "use_ctl") == 0) {
    if (term->use_ctl) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "bidi_mode") == 0) {
    value = vt_get_bidi_mode_name(term->bidi_mode);
  } else if (strcmp(key, "bidi_separators") == 0) {
    if ((value = term->bidi_separators) == NULL) {
      value = "";
    }
  }
#ifdef USE_OT_LAYOUT
  else if (strcmp(key, "use_ot_layout") == 0) {
    if (term->use_ot_layout) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "ot_features") == 0) {
    value = vt_get_ot_layout_attr(OT_FEATURES);
  } else if (strcmp(key, "ot_script") == 0) {
    value = vt_get_ot_layout_attr(OT_SCRIPT);
  }
#endif
  else if (strcmp(key, "pty_name") == 0) {
    if (output) {
      if ((value = vt_get_window_name(term->parser)) == NULL) {
        value = "";
      }
    } else {
      value = vt_term_get_slave_name(term);
    }
  } else if (strcmp(key, "icon_path") == 0) {
    if ((value = term->icon_path) == NULL) {
      value = "";
    }
  } else if (strcmp(key, "use_local_echo") == 0) {
    if (term->use_local_echo) {
      value = "true";
    } else {
      value = "false";
    }
#if defined(__ANDROID__) && defined(USE_LIBSSH2)
  } else if (strcmp(key, "start_with_local_pty") == 0) {
    value = start_with_local_pty ? "true" : "false";
#endif
  } else {
    /* Continue to process it in x_screen.c */
    return 0;
  }

  if (!output) {
    output = term;
  }

/* value is never set NULL above. */
#if 0
  if (!value) {
    vt_response_config(output->pty, "error", NULL, to_menu);
  }
#endif

  if (flag) {
    *flag = value ? true_or_false(value) : -1;
  } else {
    vt_response_config(output->pty, key, value, to_menu);
  }

  return 1;
}

int vt_term_set_config(vt_term_t *term, char *key, char *value) {
  if (vt_parser_set_config(term->parser, key, value)) {
    /* do nothing */
  } else if (strcmp(key, "use_local_echo") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      set_use_local_echo(term, flag);
    }
  }
#ifdef USE_OT_LAYOUT
  else if (strcmp(key, "ot_script") == 0) {
    vt_set_ot_layout_attr(value, OT_SCRIPT);
  } else if (strcmp(key, "ot_features") == 0) {
    vt_set_ot_layout_attr(value, OT_FEATURES);
  }
#endif
  else {
    /* Continue to process it in x_screen.c */
    return 0;
  }

  return 1;
}
