/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_term_manager.h"

#include <stdio.h>  /* sprintf/sscanf */
#include <unistd.h> /* fork/exec */
#include <signal.h>
#include <pobl/bl_str.h> /* bl_snprintf */
#include <pobl/bl_mem.h> /* malloc */
#include <pobl/bl_sig_child.h>
#include <pobl/bl_util.h> /* BL_DIGIT_STR */
#include <pobl/bl_debug.h>
#include <pobl/bl_file.h>   /* bl_file_unset_cloexec */
#include <pobl/bl_unistd.h> /* bl_setenv/bl_unsetenv */

#define MAX_TERMS (MTU * max_terms_multiple) /* Default MAX_TERMS is 32. */
#define MTU (8 * sizeof(*dead_mask))         /* MAX_TERMS unit */

#ifndef BINDIR
#define BINDIR "/usr/local/bin"
#endif

#if 0
#define __DEBUG
#endif

#if 0
#define INFINIT_RESTART
#endif

/* --- static variables --- */

static u_int max_terms_multiple;
static u_int32_t *dead_mask;

/*
 * 'terms' pointer must not be changed because vt_get_all_terms returns it
 * directly.
 * So 'terms' array must be allocated only once.
 */
static vt_term_t **terms;
static u_int num_terms;

static char *pty_list;

static int zombie_pty;

static char *auto_restart_cmd;

/* --- static functions --- */

#if !defined(USE_WIN32API) && !defined(DEBUG)
static void sig_error(int sig) {
  u_int count;
  char env[1024];
  size_t len;

  env[0] = '\0';
  len = 0;
  for (count = 0; count < num_terms; count++) {
    int master;

    if ((master = vt_term_get_master_fd(terms[count])) >= 0) {
      int slave;
      size_t n;

      slave = vt_term_get_slave_fd(terms[count]);
      snprintf(env + len, 1024 - len, "%d %d %d,", master, slave,
               vt_term_get_child_pid(terms[count]));
      n = strlen(env + len);
      if (n + len >= 1024) {
        env[len] = '\0';
        break;
      } else {
        len += n;
      }

      bl_file_unset_cloexec(master);
      bl_file_unset_cloexec(slave);
    }
  }

  if (len > 0) {
    pid_t pid;

    pid = fork();

    if (pid < 0) {
      return;
    }

    if (pid == 0) {
      /* child process */
      for (count = 0; count < num_terms; count++) {
        vt_term_write_content(terms[count], vt_term_get_slave_fd(terms[count]),
                              terms[count]->parser->cc_conv, 1, WCA_ALL);
      }

      exit(0);
    }

    bl_setenv("INHERIT_PTY_LIST", env, 1);

    if (auto_restart_cmd) {
      execlp(auto_restart_cmd, auto_restart_cmd, NULL);
    }

    execl(BINDIR "/mlterm", BINDIR "/mlterm", NULL);

    bl_error_printf("Failed to restart mlterm.\n");
  }

  exit(1);
}
#endif

static void sig_child(void *p, pid_t pid) {
  u_int count;

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " SIG_CHILD received [PID:%d].\n", pid);
#endif

  if (pid == -1) {
    /*
     * Note:
     * If term->pty is NULL, vt_term_get_child_pid() returns -1.
     * waitpid() in bl_sig_child.c might return -1.
     *
     * (Don't check by "pid < 0" above, because vt_term_get_child_pid()
     * might return minus value if it is a ssh channel.)
     */

    return;
  }

  for (count = 0; count < num_terms; count++) {
    if (pid == vt_term_get_child_pid(terms[count])) {
      u_int idx;

#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " pty %d is dead.\n", count);
#endif

      idx = count / MTU;
      dead_mask[idx] |= (1 << (count - MTU * idx));
    }
  }
}

/* --- global functions --- */

int vt_term_manager_init(u_int multiple) {
  if (multiple > 0) {
    max_terms_multiple = multiple;
  } else {
    max_terms_multiple = 1;
  }

  if ((terms = malloc(sizeof(vt_term_t*) * MAX_TERMS)) == NULL) {
    return 0;
  }

  if ((dead_mask = calloc(sizeof(*dead_mask), max_terms_multiple)) == NULL) {
    goto error1;
  }

  if (!vt_term_init()) {
    goto error2;
  }

  bl_add_sig_child_listener(NULL, sig_child);

  return 1;

error2:
  free(dead_mask);
  dead_mask = NULL;

error1:
  free(terms);
  terms = NULL;

  return 0;
}

void vt_term_manager_final(void) {
  int count;

#ifdef USE_OT_LAYOUT
  vt_set_ot_layout_attr(NULL, OT_SCRIPT);
  vt_set_ot_layout_attr(NULL, OT_FEATURES);
#endif

  bl_remove_sig_child_listener(NULL, sig_child);
  vt_term_final();

  for (count = num_terms - 1; count >= 0; count--) {
#if 0
    /*
     * All windows may be invalid before vt_term_manager_final() is called.
     * Without this vt_term_detach(), if terms[count] is not detached,
     * pty_listener::pty_closed() which is called in vt_pty_destroy() can
     * operate invalid window.
     */
    vt_term_detach(terms[count]);
#endif

    vt_term_destroy(terms[count]);
  }

  free(terms);

  free(dead_mask);
  free(pty_list);
  free(auto_restart_cmd);
}

void vt_set_auto_restart_cmd(const char *cmd) {
#if !defined(USE_WIN32API) && !defined(DEBUG)
  char *env;

  if (
#ifndef INFINIT_RESTART
      (!(env = getenv("INHERIT_PTY_LIST")) || *env == '\0') &&
#endif
      cmd && *cmd) {
    if (!auto_restart_cmd) {
      struct sigaction act;

#if 0
      /*
       * sa_sigaction which is called instead of sa_handler
       * if SA_SIGINFO is set to sa_flags is not defined in
       * some environments.
       */
      act.sa_sigaction = NULL;
#endif
      act.sa_handler = sig_error;
      sigemptyset(&act.sa_mask); /* Not blocking any signals for child. */
      act.sa_flags = SA_NODEFER; /* Not blocking any signals for child. */
      sigaction(SIGBUS, &act, NULL);
      sigaction(SIGSEGV, &act, NULL);
      sigaction(SIGFPE, &act, NULL);
      sigaction(SIGILL, &act, NULL);

      free(auto_restart_cmd);
    }

    auto_restart_cmd = strdup(cmd);
  } else if (auto_restart_cmd) {
    signal(SIGBUS, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGILL, SIG_DFL);
    free(auto_restart_cmd);
    auto_restart_cmd = NULL;
  }
#endif
}

vt_term_t *vt_create_term(const char *term_type, u_int cols, u_int rows, u_int tab_size,
                          u_int log_size, vt_char_encoding_t encoding, int is_auto_encoding,
                          int use_auto_detect, int logging_vt_seq, vt_unicode_policy_t policy,
                          int col_size_a, int use_char_combining, int use_multi_col_char,
                          int use_ctl, vt_bidi_mode_t bidi_mode, const char *bidi_separators,
                          int use_dynamic_comb, vt_bs_mode_t bs_mode,
                          vt_vertical_mode_t vertical_mode, int use_local_echo,
                          const char *win_name, const char *icon_name,
                          int use_ansi_colors, vt_alt_color_mode_t alt_color_mode,
                          int use_ot_layout, vt_cursor_style_t cursor_style,
                          int ignore_broadcasted_chars) {
#if !defined(USE_WIN32API) && !defined(DEBUG)
  char *list;
  char *list_tmp;
#endif

  if (num_terms == MAX_TERMS) {
    return NULL;
  }

#if !defined(USE_WIN32API) && !defined(DEBUG)
  if ((list_tmp = getenv("INHERIT_PTY_LIST")) && (list = alloca(strlen(list_tmp) + 1))) {
    int master;
    int slave;
    pid_t child_pid;
    char *p;

    strcpy(list, list_tmp);
    while ((p = bl_str_sep(&list, ","))) {
      vt_pty_ptr_t pty;

      if (sscanf(p, "%d %d %d", &master, &slave, &child_pid) == 3) {
        /*
         * cols + 1 is for redrawing screen by vt_set_pty_winsize() below.
         */
        if ((pty = vt_pty_new_with(master, slave, child_pid, cols + 1, rows, 0, 0))) {
          if ((terms[num_terms] = vt_term_new(
                   term_type, cols, rows, tab_size, log_size, encoding, is_auto_encoding,
                   use_auto_detect, logging_vt_seq, policy, col_size_a, use_char_combining,
                   use_multi_col_char, use_ctl, bidi_mode, bidi_separators, use_dynamic_comb,
                   bs_mode, vertical_mode, use_local_echo, win_name, icon_name, use_ansi_colors,
                   alt_color_mode, use_ot_layout, cursor_style, ignore_broadcasted_chars))) {
            vt_term_plug_pty(terms[num_terms++], pty);
            vt_set_pty_winsize(pty, cols, rows, 0, 0);

            continue;
          } else {
            vt_pty_destroy(pty);
          }
        }

        close(master);
        close(slave);
      }
    }

#ifdef INFINIT_RESTART
    bl_unsetenv("INHERIT_PTY_LIST");
#endif

    if (num_terms > 0) {
      return terms[num_terms - 1];
    }
  }
#endif

  /*
   * Before modifying terms and num_terms, do vt_close_dead_terms().
   */
  vt_close_dead_terms();

  /*
   * XXX
   * If sig_child here...
   */

  if (!(terms[num_terms] = vt_term_new(term_type, cols, rows, tab_size, log_size, encoding,
                                          is_auto_encoding, use_auto_detect, logging_vt_seq,
                                          policy, col_size_a, use_char_combining,
                                          use_multi_col_char, use_ctl, bidi_mode, bidi_separators,
                                          use_dynamic_comb, bs_mode, vertical_mode, use_local_echo,
                                          win_name, icon_name, use_ansi_colors, alt_color_mode,
                                          use_ot_layout, cursor_style, ignore_broadcasted_chars))) {
    return NULL;
  }

  return terms[num_terms++];
}

void vt_destroy_term(vt_term_t *term) {
  u_int count;

  /*
   * Before modifying terms and num_terms, do vt_close_dead_terms().
   */
  vt_close_dead_terms();

  /*
   * XXX
   * If sig_child here...
   */

  for (count = 0; count < num_terms; count++) {
    if (terms[count] == term) {
      terms[count] = terms[--num_terms];

      break;
    }
  }

  vt_term_destroy(term);
}

vt_term_t *vt_get_term(const char *dev) {
  int count;

  for (count = 0; count < num_terms; count++) {
    if (dev == NULL || strcmp(dev, vt_term_get_slave_name(terms[count])) == 0) {
      return terms[count];
    }
  }

  return NULL;
}

vt_term_t *vt_get_detached_term(const char *dev) {
  int count;

  for (count = 0; count < num_terms; count++) {
    if (!vt_term_is_attached(terms[count]) &&
        (dev == NULL || strcmp(dev, vt_term_get_slave_name(terms[count])) == 0)) {
      return terms[count];
    }
  }

  return NULL;
}

vt_term_t *vt_next_term(vt_term_t *term /* is detached */
                        ) {
  int count;

  for (count = 0; count < num_terms; count++) {
    if (terms[count] == term) {
      int old;

      old = count;

      for (count++; count < num_terms; count++) {
        if (!vt_term_is_attached(terms[count])) {
          return terms[count];
        }
      }

      for (count = 0; count < old; count++) {
        if (!vt_term_is_attached(terms[count])) {
          return terms[count];
        }
      }

      return NULL;
    }
  }

  return NULL;
}

vt_term_t *vt_prev_term(vt_term_t *term /* is detached */
                        ) {
  int count;

  for (count = 0; count < num_terms; count++) {
    if (terms[count] == term) {
      int old;

      old = count;

      for (count--; count >= 0; count--) {
        if (!vt_term_is_attached(terms[count])) {
          return terms[count];
        }
      }

      for (count = num_terms - 1; count > old; count--) {
        if (!vt_term_is_attached(terms[count])) {
          return terms[count];
        }
      }

      return NULL;
    }
  }

  return NULL;
}

/*
 * Return value: Number of opened terms. Don't trust it after vt_create_term(),
 * vt_destroy_term() or vt_close_dead_terms() which can change it is called.
 */
u_int vt_get_all_terms(vt_term_t*** _terms) {
  if (_terms) {
    *_terms = terms;
  }

  return num_terms;
}

void vt_close_dead_terms(void) {
  if (num_terms > 0) {
    int idx;

    for (idx = (num_terms - 1) / MTU; idx >= 0; idx--) {
      if (dead_mask[idx]) {
        int count;

        for (count = MTU - 1; count >= 0; count--) {
          if (dead_mask[idx] & (0x1 << count)) {
            vt_term_t *term;

#ifdef DEBUG
            bl_debug_printf(BL_DEBUG_TAG " closing dead term %d.\n", count);
#endif

            term = terms[idx * MTU + count];
            /*
             * Update terms and num_terms before
             * vt_term_destroy, which calls
             * vt_pty_event_listener::pty_close in which
             * vt_term_manager can be used.
             */
            terms[idx * MTU + count] = terms[--num_terms];
            if (zombie_pty) {
              vt_term_zombie(term);
            } else {
              vt_term_destroy(term);
            }
          }
        }

        memset(&dead_mask[idx], 0, sizeof(dead_mask[idx]));
      }
    }
  }
}

char *vt_get_pty_list(void) {
  int count;
  char *p;
  size_t len;

  free(pty_list);

  /* The length of pty name is under 50. */
  len = (50 + 2) * num_terms;

  if ((pty_list = malloc(len + 1)) == NULL) {
    return "";
  }

  p = pty_list;

  *p = '\0';

  for (count = 0; count < num_terms; count++) {
    bl_snprintf(p, len, "%s:%d;", vt_term_get_slave_name(terms[count]),
                vt_term_is_attached(terms[count]));

    len -= strlen(p);
    p += strlen(p);
  }

  return pty_list;
}

void vt_term_manager_enable_zombie_pty(void) { zombie_pty = 1; }

#ifdef __HAIKU__
int vt_check_sig_child(void) {
  if (num_terms > 0) {
    int idx;

    for (idx = (num_terms - 1) / MTU; idx >= 0; idx--) {
      if (dead_mask[idx]) {
        return 1;
      }
    }
  }

  return 0;
}
#endif
