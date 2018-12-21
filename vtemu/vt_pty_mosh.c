/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_pty_intern.h"

#ifndef NO_DYNAMIC_LOAD_SSH

#include <stdio.h> /* NULL */
#include <pobl/bl_dlfcn.h>
#include <pobl/bl_debug.h>

#ifndef LIBDIR
#define SSHLIB_DIR "/usr/local/lib/mlterm/"
#else
#define SSHLIB_DIR LIBDIR "/mlterm/"
#endif

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

static vt_pty_ptr_t (*mosh_new)(const char *, char **, char **, const char *, const char *,
                                const char *, const char *, u_int, u_int, u_int, u_int);

static int is_tried;
static bl_dl_handle_t handle;

/* --- static functions --- */

static void load_library(void) {
  is_tried = 1;

  if (!(handle = bl_dl_open(SSHLIB_DIR, "ptymosh")) && !(handle = bl_dl_open("", "ptymosh"))) {
    bl_error_printf("MOSH: Could not load.\n");

    return;
  }

  bl_dl_close_at_exit(handle);

  mosh_new = bl_dl_func_symbol(handle, "vt_pty_mosh_new");
}

#endif

/* --- global functions --- */

vt_pty_ptr_t vt_pty_mosh_new(const char *cmd_path, char **cmd_argv, char **env, const char *uri,
                             const char *pass, const char *pubkey, const char *privkey, u_int cols,
                             u_int rows, u_int width_pix, u_int height_pix) {
#ifndef NO_DYNAMIC_LOAD_SSH
  if (!is_tried) {
    load_library();
  }

  if (mosh_new) {
    return (*mosh_new)(cmd_path, cmd_argv, env, uri, pass, pubkey, privkey, cols, rows, width_pix,
                       height_pix);
  }
#endif

  return NULL;
}
