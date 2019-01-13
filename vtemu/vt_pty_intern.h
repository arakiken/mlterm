/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_PTY_INTERN_H__
#define __VT_PTY_INTERN_H__

#include "vt_pty.h"
#include "vt_config_menu.h"

/* See android/jni/ui_event_source.c */
#ifdef __ANDROID__
#undef vt_config_menu_write
extern char *android_config_response;
#define vt_config_menu_write(config_menu, buf, len) \
        (android_config_response = strncpy(calloc(len + 1, 1), buf, len))
#endif

typedef struct vt_pty {
  int master; /* master pty fd */
  int slave;  /* slave pty fd */
  pid_t child_pid;

  /* Used in vt_write_to_pty */
  u_char *buf;
  size_t left;
  size_t size;

  int (*final)(vt_pty_ptr_t);
  int (*set_winsize)(vt_pty_ptr_t, u_int, u_int, u_int, u_int);
  ssize_t (*write)(vt_pty_ptr_t, u_char*, size_t);
  ssize_t (*read)(vt_pty_ptr_t, u_char*, size_t);

  vt_pty_event_listener_t *pty_listener;
  vt_pty_hook_t *hook;

  vt_config_menu_t config_menu;

  struct _stored {
    int master;
    int slave;
    ssize_t (*write)(vt_pty_ptr_t, u_char*, size_t);
    ssize_t (*read)(vt_pty_ptr_t, u_char*, size_t);

    u_int ref_count;

  } *stored;

  char *cmd_line;

  vt_pty_mode_t mode;

} vt_pty_t;

vt_pty_t *vt_pty_unix_new(const char *cmd_path, char **cmd_argv, char **env, const char *host,
                          const char *work_dir, u_int cols, u_int rows, u_int width_pix,
                          u_int height_pix);

vt_pty_t *vt_pty_unix_new_with(int master, int slave, pid_t child_pid, const char *host, u_int cols,
                               u_int rows, u_int width_pix, u_int height_pix);

vt_pty_t *vt_pty_ssh_new(const char *cmd_path, char **cmd_argv, char **env, const char *host,
                         const char *pass, const char *pubkey, const char *privkey, u_int cols,
                         u_int rows, u_int width_pix, u_int height_pix);

vt_pty_t *vt_pty_pipe_new(const char *cmd_path, char **cmd_argv, char **env, const char *host,
                          const char *pass, u_int cols, u_int rows);

vt_pty_t *vt_pty_mosh_new(const char *cmd_path, char **cmd_argv, char **env, const char *host,
                         const char *pass, const char *pubkey, const char *privkey, u_int cols,
                         u_int rows, u_int width_pix, u_int height_pix);

#endif
