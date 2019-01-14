/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_PTY_INTERN_H__
#define __VT_PTY_INTERN_H__

#include "vt_pty.h"

/* See android/jni/ui_event_source.c */
#ifdef __ANDROID__
#undef vt_config_menu_write
extern char *android_config_response;
#define vt_config_menu_write(config_menu, buf, len) \
        (android_config_response = strncpy(calloc(len + 1, 1), buf, len))
#endif

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
