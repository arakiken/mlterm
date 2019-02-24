/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_PTY_H__
#define __VT_PTY_H__

#include <pobl/bl_def.h>   /* USE_WIN32API */
#include <pobl/bl_types.h> /* u_int/u_char */

#include "vt_config_menu.h"

#if defined(USE_LIBSSH2) && !defined(USE_MOSH)

#include "vt_char_encoding.h"

/*
 * defined(__CYGWIN__) is not to link libpthread to mlterm for now.
 *
 * OPEN_PTY_SYNC is defined in java/Makefile.in
 *
 * Note that bl_dialog() in open_pty() in vt_term.c might cause segfault if
 * OPEN_PTY_ASYNC is defined on platforms other than WIN32GUI,
 */
#if (defined(USE_WIN32API) && !defined(OPEN_PTY_SYNC)) || \
    (defined(HAVE_PTHREAD) && (defined(__CYGWIN__) || defined(__MSYS__)))
#define OPEN_PTY_ASYNC
#endif

#endif /* USE_LIBSSH2 */

typedef enum {
  PTY_NONE,
  PTY_LOCAL,
  PTY_SSH,
  PTY_MOSH,
  PTY_PIPE,
} vt_pty_mode_t;

typedef struct vt_pty_event_listener {
  void *self;

  /* Called when vt_pty_destroy. */
  void (*closed)(void *);

  void (*show_config)(void *, char *);

} vt_pty_event_listener_t;

typedef struct vt_pty_hook {
  void *self;

  size_t (*pre_write)(void *, u_char *, size_t);

} vt_pty_hook_t;

typedef struct vt_pty {
  int master; /* master pty fd */
  int slave;  /* slave pty fd */
  pid_t child_pid;

  /* Used in vt_write_to_pty */
  u_char *buf;
  size_t left;
  size_t size;

  int (*final)(struct vt_pty *);
  int (*set_winsize)(struct vt_pty *, u_int, u_int, u_int, u_int);
  ssize_t (*write)(struct vt_pty *, u_char*, size_t);
  ssize_t (*read)(struct vt_pty *, u_char*, size_t);

  vt_pty_event_listener_t *pty_listener;
  vt_pty_hook_t *hook;

  vt_config_menu_t config_menu;

  struct _stored {
    int master;
    int slave;
    ssize_t (*write)(struct vt_pty *, u_char*, size_t);
    ssize_t (*read)(struct vt_pty *, u_char*, size_t);

    u_int ref_count;

  } *stored;

  char *cmd_line;

  vt_pty_mode_t mode;

} vt_pty_t;

vt_pty_t *vt_pty_new(const char *cmd_path, char **cmd_argv, char **env, const char *host,
                     const char *work_dir, const char *pass, const char *pubkey,
                     const char *privkey, u_int cols, u_int rows, u_int width_pix,
                     u_int height_pix);

vt_pty_t *vt_pty_new_with(int master, int slave, pid_t child_pid, u_int cols, u_int rows,
                          u_int width_pix, u_int height_pix);

int vt_pty_destroy(vt_pty_t *pty);

#define vt_pty_set_listener(pty, listener) ((pty)->pty_listener = listener)

int vt_set_pty_winsize(vt_pty_t *pty, u_int cols, u_int rows, u_int width_pix, u_int height_pix);

size_t vt_write_to_pty(vt_pty_t *pty, u_char *buf, size_t len);

size_t vt_read_pty(vt_pty_t *pty, u_char *buf, size_t left);

void vt_response_config(vt_pty_t *pty, char *key, char *value, int to_menu);

#define vt_pty_get_pid(pty) ((pty)->child_pid)

#define vt_pty_get_master_fd(pty) ((pty)->master)

#define vt_pty_get_slave_fd(pty) ((pty)->slave)

char *vt_pty_get_slave_name(vt_pty_t *pty);

int vt_start_config_menu(vt_pty_t *pty, char *cmd_path, int x, int y, char *display);

#define vt_pty_get_cmd_line(pty) ((pty)->cmd_line)

#define vt_pty_set_hook(pty, hk) ((pty)->hook = hk)

#define vt_pty_get_mode(pty) ((pty)->mode)

#ifdef USE_LIBSSH2
#ifndef USE_MOSH
void *vt_search_ssh_session(const char *host, const char *port, const char *user);

int vt_pty_ssh_set_use_loopback(vt_pty_t *pty, int use);

int vt_pty_ssh_scp(vt_pty_t *pty, vt_char_encoding_t pty_encoding,
                   vt_char_encoding_t path_encoding, char *dst_path, char *src_path,
                   int use_scp_full);

void vt_pty_ssh_set_cipher_list(const char *list);

void vt_pty_ssh_set_keepalive_interval(u_int interval_sec);

u_int vt_pty_ssh_keepalive(u_int spent_msec);

void vt_pty_ssh_set_use_x11_forwarding(void *session, int use_x11_forwarding);

int vt_pty_ssh_poll(void *fds);

u_int vt_pty_ssh_get_x11_fds(int **fds);

int vt_pty_ssh_send_recv_x11(int idx, int bidirection);

void vt_pty_ssh_set_use_auto_reconnect(int flag);

#ifdef USE_WIN32API
void vt_pty_ssh_set_pty_read_trigger(void (*func)(void));
#endif

#endif

int vt_pty_mosh_set_use_loopback(vt_pty_t *pty, int use);

#ifdef USE_WIN32API
void vt_pty_mosh_set_pty_read_trigger(void (*func)(void));
#endif
#endif

#endif
