/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <pobl/bl_locale.h> /* bl_get_codeset() */

#ifdef NO_DYNAMIC_LOAD_SSH
#include "libptyssh/vt_pty_ssh.c"
#else /* NO_DYNAMIC_LOAD_SSH */

#include "vt_pty_intern.h"

#include <stdio.h>  /* snprintf */
#include <string.h> /* strcmp */
#include <pobl/bl_types.h>
#include <pobl/bl_dlfcn.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_path.h> /* bl_basename */
#include <pobl/bl_mem.h>  /* alloca */

#ifndef LIBDIR
#define SSHLIB_DIR "/usr/local/lib/mlterm/"
#else
#define SSHLIB_DIR LIBDIR "/mlterm/"
#endif

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

static vt_pty_ptr_t (*ssh_new)(const char *, char **, char **, const char *, const char *,
                               const char *, const char *, u_int, u_int, u_int, u_int);
static void *(*search_ssh_session)(const char *, const char *, const char *);
static int (*set_use_loopback)(vt_pty_ptr_t, int);
static int (*ssh_scp)(vt_pty_ptr_t, int, char *, char *);
static void (*ssh_set_cipher_list)(const char *);
static void (*ssh_set_keepalive_interval)(u_int);
static int (*ssh_keepalive)(u_int);
static void (*ssh_set_use_x11_forwarding)(void *, int);
static int (*ssh_poll)(void *);
static u_int (*ssh_get_x11_fds)(int **);
static int (*ssh_send_recv_x11)(int, int);
static void (*ssh_set_use_auto_reconnect)(int);

static int is_tried;
static bl_dl_handle_t handle;

/* --- static functions --- */

static void load_library(void) {
  is_tried = 1;

  if (!(handle = bl_dl_open(SSHLIB_DIR, "ptyssh")) && !(handle = bl_dl_open("", "ptyssh"))) {
    bl_error_printf("SSH: Could not load.\n");

    return;
  }

  bl_dl_close_at_exit(handle);

  ssh_new = bl_dl_func_symbol(handle, "vt_pty_ssh_new");
  search_ssh_session = bl_dl_func_symbol(handle, "vt_search_ssh_session");
  set_use_loopback = bl_dl_func_symbol(handle, "vt_pty_set_use_loopback");
  ssh_scp = bl_dl_func_symbol(handle, "vt_pty_ssh_scp_intern");
  ssh_set_cipher_list = bl_dl_func_symbol(handle, "vt_pty_ssh_set_cipher_list");
  ssh_set_keepalive_interval = bl_dl_func_symbol(handle, "vt_pty_ssh_set_keepalive_interval");
  ssh_keepalive = bl_dl_func_symbol(handle, "vt_pty_ssh_keepalive");
  ssh_set_use_x11_forwarding = bl_dl_func_symbol(handle, "vt_pty_ssh_set_use_x11_forwarding");
  ssh_poll = bl_dl_func_symbol(handle, "vt_pty_ssh_poll");
  ssh_get_x11_fds = bl_dl_func_symbol(handle, "vt_pty_ssh_get_x11_fds");
  ssh_send_recv_x11 = bl_dl_func_symbol(handle, "vt_pty_ssh_send_recv_x11");
  ssh_set_use_auto_reconnect = bl_dl_func_symbol(handle, "vt_pty_ssh_set_use_auto_reconnect");
}

/* --- global functions --- */

vt_pty_ptr_t vt_pty_ssh_new(const char *cmd_path, char **cmd_argv, char **env, const char *uri,
                            const char *pass, const char *pubkey, const char *privkey, u_int cols,
                            u_int rows, u_int width_pix, u_int height_pix) {
  if (!is_tried) {
    load_library();
  }

  if (ssh_new) {
    return (*ssh_new)(cmd_path, cmd_argv, env, uri, pass, pubkey, privkey, cols, rows, width_pix,
                      height_pix);
  } else {
    return NULL;
  }
}

void *vt_search_ssh_session(const char *host, const char *port, /* can be NULL */
                            const char *user                    /* can be NULL */
                            ) {
  if (search_ssh_session) {
    return (*search_ssh_session)(host, port, user);
  } else {
    return NULL;
  }
}

int vt_pty_set_use_loopback(vt_pty_ptr_t pty, int use) {
  if (set_use_loopback) {
    return (*set_use_loopback)(pty, use);
  } else {
    return 0;
  }
}

void vt_pty_ssh_set_cipher_list(const char *list) {
  /* This function can be called before vt_pty_ssh_new() */
  if (!is_tried) {
    load_library();
  }

  if (ssh_set_cipher_list) {
    (*ssh_set_cipher_list)(list);
  }
}

void vt_pty_ssh_set_keepalive_interval(u_int interval_sec) {
  /* This function can be called before vt_pty_ssh_new() */
  if (!is_tried) {
    load_library();
  }

  if (ssh_set_keepalive_interval) {
    (*ssh_set_keepalive_interval)(interval_sec);
  }
}

int vt_pty_ssh_keepalive(u_int spent_msec) {
  if (ssh_keepalive) {
    return (*ssh_keepalive)(spent_msec);
  } else {
    return 0;
  }
}

void vt_pty_ssh_set_use_x11_forwarding(void *session, int use) {
  /* This function can be called before vt_pty_ssh_new() */
  if (!is_tried) {
    load_library();
  }

  if (ssh_set_use_x11_forwarding) {
    (*ssh_set_use_x11_forwarding)(session, use);
  }
}

int vt_pty_ssh_poll(void *fds) {
  if (ssh_poll) {
    return (*ssh_poll)(fds);
  } else {
    return 0;
  }
}

u_int vt_pty_ssh_get_x11_fds(int **fds) {
  if (ssh_get_x11_fds) {
    return (*ssh_get_x11_fds)(fds);
  } else {
    return 0;
  }
}

int vt_pty_ssh_send_recv_x11(int idx, int bidirection) {
  if (ssh_send_recv_x11) {
    return (*ssh_send_recv_x11)(idx, bidirection);
  } else {
    return 0;
  }
}

void vt_pty_ssh_set_use_auto_reconnect(int use) {
  /* This function can be called before vt_pty_ssh_new() */
  if (!is_tried) {
    load_library();
  }

  if (ssh_set_use_auto_reconnect) {
    (*ssh_set_use_auto_reconnect)(use);
  }
}

#endif /* NO_DYNAMIC_LOAD_SSH */

int vt_pty_ssh_scp(vt_pty_ptr_t pty, vt_char_encoding_t pty_encoding, /* Not VT_UNKNOWN_ENCODING */
                   vt_char_encoding_t path_encoding,                  /* Not VT_UNKNOWN_ENCODING */
                   char* dst_path, char* src_path, int use_scp_full) {
  int dst_is_remote;
  int src_is_remote;
  char* file;
  char* _dst_path;
  char* _src_path;
  size_t len;
  char* p;
  vt_char_encoding_t locale_encoding;

  if (strncmp(dst_path, "remote:", 7) == 0) {
    dst_path += 7;
    dst_is_remote = 1;
  } else if (strncmp(dst_path, "local:", 6) == 0) {
    dst_path += 6;
    dst_is_remote = 0;
  } else {
    dst_is_remote = -1;
  }

  if (strncmp(src_path, "local:", 6) == 0) {
    src_path += 6;
    src_is_remote = 0;
  } else if (strncmp(src_path, "remote:", 7) == 0) {
    src_path += 7;
    src_is_remote = 1;
  } else {
    if (dst_is_remote == -1) {
      src_is_remote = 0;
      dst_is_remote = 1;
    } else {
      src_is_remote = (!dst_is_remote);
    }
  }

  if (dst_is_remote == -1) {
    dst_is_remote = (!src_is_remote);
  } else if (dst_is_remote == src_is_remote) {
    bl_msg_printf("SCP: Destination host(%s) and source host(%s) is the same.\n", dst_path,
                  src_path);

    return 0;
  }

  if (pty->stored /* using loopback */ || use_scp_full) {
    /* do nothing */
  } else if (IS_RELATIVE_PATH_UNIX(dst_path)) /* dst_path is always unix style. */
  {
    char* prefix;

    if (strstr(dst_path, "..")) {
      /* insecure file name */
      return 0;
    }

    prefix = bl_get_home_dir();

    if (!(p = alloca(strlen(prefix) + 13 + strlen(dst_path) + 1))) {
      return 0;
    }

    /* mkdir ~/.mlterm/scp in advance. */
    if (!dst_is_remote) {
#ifdef USE_WIN32API
      sprintf(p, "%s\\mlterm\\scp\\%s", prefix, dst_path);
#else
      sprintf(p, "%s/.mlterm/scp/%s", prefix, dst_path);
#endif
    } else {
      sprintf(p, ".mlterm/scp/%s", dst_path);
    }

    dst_path = p;
  }

  /* scp /tmp/TEST /home/user => scp /tmp/TEST /home/user/TEST */
  file = bl_basename(src_path);
  if ((p = alloca(strlen(dst_path) + strlen(file) + 2))) {
#ifdef USE_WIN32API
    if (!dst_is_remote) {
      sprintf(p, "%s\\%s", dst_path, file);
    } else
#endif
    {
      sprintf(p, "%s/%s", dst_path, file);
    }

    dst_path = p;
  }

#if 0
  bl_debug_printf("SCP: %s%s -> %s%s\n", src_is_remote ? "remote:" : "local:", src_path,
                  dst_is_remote ? "remote:" : "local:", dst_path);
#endif

  if (path_encoding != pty_encoding) {
    /* convert to terminal encoding */
    len = strlen(dst_path);
    if ((_dst_path = alloca(len * 2 + 1))) {
      _dst_path[vt_char_encoding_convert(_dst_path, len * 2, pty_encoding, dst_path, len,
                                         path_encoding)] = '\0';
    }
    len = strlen(src_path);
    if ((_src_path = alloca(len * 2 + 1))) {
      _src_path[vt_char_encoding_convert(_src_path, len * 2, pty_encoding, src_path, len,
                                         path_encoding)] = '\0';
    }
  } else {
    _dst_path = dst_path;
    _src_path = src_path;
  }

  if ((locale_encoding = vt_get_char_encoding(bl_get_codeset())) == VT_UNKNOWN_ENCODING) {
    locale_encoding = path_encoding;
  }

  if (src_is_remote) {
    /* Remote: convert to terminal encoding */
    if (_src_path) {
      src_path = _src_path;
    }

    /* Local: convert to locale encoding */
    len = strlen(dst_path);
    if (locale_encoding != path_encoding && (p = alloca(len * 2 + 1))) {
      p[vt_char_encoding_convert(p, len * 2, locale_encoding, dst_path, len, path_encoding)] = '\0';
      dst_path = p;
    }
  } else /* if( dst_is_remote) */
  {
    /* Remote: convert to terminal encoding */
    if (_dst_path) {
      dst_path = _dst_path;
    }

    /* Local: convert to locale encoding */
    len = strlen(src_path);
    if (locale_encoding != path_encoding && (p = alloca(len * 2 + 1))) {
      p[vt_char_encoding_convert(p, len * 2, locale_encoding, src_path, len, path_encoding)] = '\0';
      src_path = p;
    }
  }

#ifdef NO_DYNAMIC_LOAD_SSH
  if (vt_pty_ssh_scp_intern(pty, src_is_remote, dst_path, src_path))
#else
  if (ssh_scp && (*ssh_scp)(pty, src_is_remote, dst_path, src_path))
#endif
  {
    return 1;
  } else {
    return 0;
  }
}
