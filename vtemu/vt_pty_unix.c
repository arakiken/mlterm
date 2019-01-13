/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_pty_intern.h"

#include <stdio.h>  /* sprintf */
#include <unistd.h> /* close */
#include <sys/ioctl.h>
#include <termios.h>
#include <signal.h> /* signal/SIGWINCH */
#include <string.h> /* strchr/memcpy */
#include <stdlib.h> /* putenv */
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h> /* realloc/alloca */
#include <pobl/bl_str.h> /* strdup */
#include <pobl/bl_pty.h>
#include <pobl/bl_path.h>   /* bl_basename */
#include <pobl/bl_unistd.h> /* bl_killpg */
#ifdef USE_UTMP
#include <pobl/bl_utmp.h>
#endif
#ifdef __APPLE__
#include <errno.h>
#include <pobl/bl_sig_child.h>
#endif

#if 0
#define __DEBUG
#endif

typedef struct vt_pty_unix {
  vt_pty_t pty;
#ifdef USE_UTMP
  bl_utmp_t utmp;
#endif

} vt_pty_unix_t;

/* --- static functions --- */

static int final(vt_pty_t *pty) {
#ifdef USE_UTMP
  if (((vt_pty_unix_t*)pty)->utmp) {
    bl_utmp_delete(((vt_pty_unix_t*)pty)->utmp);
  }
#endif

#ifdef __DEBUG
  bl_debug_printf("PTY fd %d is closed\n", pty->master);
#endif

  bl_pty_close(pty->master);
  close(pty->slave);

  return 1;
}

static int set_winsize(vt_pty_t *pty, u_int cols, u_int rows, u_int width_pix, u_int height_pix) {
  struct winsize ws;

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " win size cols %d rows %d width %d height %d.\n", cols, rows,
                  width_pix, height_pix);
#endif

  ws.ws_col = cols;
  ws.ws_row = rows;
  ws.ws_xpixel = width_pix;
  ws.ws_ypixel = height_pix;

  if (ioctl(pty->master, TIOCSWINSZ, &ws) < 0) {
#ifdef DBEUG
    bl_warn_printf(BL_DEBUG_TAG " ioctl(TIOCSWINSZ) failed.\n");
#endif

    return 0;
  }

  if (pty->child_pid > 1) {
    int pgrp;

#ifdef TIOCGPGRP
    if (ioctl(pty->master, TIOCGPGRP, &pgrp) != -1) {
      if (pgrp > 0) {
        bl_killpg(pgrp, SIGWINCH);
      }
    } else
#endif
    {
      bl_killpg(pty->child_pid, SIGWINCH);
    }
  }

  return 1;
}

static ssize_t write_to_pty(vt_pty_t *pty, u_char *buf, size_t len) {
#ifdef __APPLE__
  ssize_t ret;

  /*
   * XXX
   * If a child shell exits by 'exit' command, mlterm doesn't receive SIG_CHLD on
   * iOS 4.3 with japanese software keyboard.
   */
  if ((ret = write(pty->master, buf, len)) < 0 && errno == EIO) {
    bl_trigger_sig_child(pty->child_pid);
  }

  return ret;
#else
  return write(pty->master, buf, len);
#endif
}

static ssize_t read_pty(vt_pty_t *pty, u_char *buf, size_t len) {
  return read(pty->master, buf, len);
}

/* --- global functions --- */

vt_pty_t *vt_pty_unix_new(const char *cmd_path, /* If NULL, child prcess is not exec'ed. */
                          char **cmd_argv,      /* can be NULL(only if cmd_path is NULL) */
                          char **env,           /* can be NULL */
                          const char *host, const char *work_dir, u_int cols, u_int rows,
                          u_int width_pix, u_int height_pix) {
  vt_pty_t *pty;
  int master;
  int slave;
  pid_t pid;

  pid = bl_pty_fork(&master, &slave);

  if (pid == -1) {
    return NULL;
  }

  if (pid == 0) {
    /* child process */

    if (work_dir) {
      chdir(work_dir);
    }

    /* reset signals and spin off the command interpreter */
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);

    /*
     * setting environmental variables.
     */
    if (env) {
      while (*env) {
        /*
         * an argument string of putenv() must be allocated memory.
         * (see SUSV2)
         */
        putenv(strdup(*env));

        env++;
      }
    }

#if 0
    /*
     * XXX is this necessary ?
     *
     * mimick login's behavior by disabling the job control signals.
     * a shell that wants them can turn them back on
     */
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
#endif

    if (!cmd_path) {
      goto return_pty;
    }

    if (strchr(cmd_path, '/') == NULL) {
      execvp(cmd_path, cmd_argv);
    } else {
      execv(cmd_path, cmd_argv);
    }

#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " exec(%s) failed.\n", cmd_path);
#endif

    exit(1);
  }

/* parent process */

return_pty:
  if (!(pty = vt_pty_unix_new_with(master, slave, pid, host, cols, rows, width_pix, height_pix))) {
    close(master);
    close(slave);
  }

  return pty;
}

vt_pty_t *vt_pty_unix_new_with(int master, int slave, pid_t child_pid, const char *host, u_int cols,
                               u_int rows, u_int width_pix, u_int height_pix) {
  vt_pty_t *pty;

  if ((pty = calloc(1, sizeof(vt_pty_unix_t))) == NULL) {
    return NULL;
  }

  pty->final = final;
  pty->set_winsize = set_winsize;
  pty->write = write_to_pty;
  pty->read = read_pty;
  pty->master = master;
  pty->slave = slave;
  pty->mode = PTY_LOCAL;

  if ((pty->child_pid = child_pid) > 0) {
/* Parent process */

#ifdef USE_UTMP
    if ((((vt_pty_unix_t*)pty)->utmp =
             bl_utmp_new(vt_pty_get_slave_name(pty), host, pty->master)) == NULL) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG "utmp failed.\n");
#endif
    }
#endif

    if (set_winsize(pty, cols, rows, width_pix, height_pix) == 0) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " vt_set_pty_winsize() failed.\n");
#endif
    }
  }

  return pty;
}
