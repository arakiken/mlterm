/*
 *	$Id$
 */

#ifndef __VT_PTY_H__
#define __VT_PTY_H__

#include <pobl/bl_def.h>   /* USE_WIN32API */
#include <pobl/bl_types.h> /* u_int/u_char */

#ifdef USE_LIBSSH2

#include "vt_char_encoding.h"

/*
 * defined(__CYGWIN__) is not to link libpthread to mlterm for now.
 * OPEN_PTY_SYNC is defined in java/Makefile.in
 */
#if (defined(USE_WIN32API) && !defined(OPEN_PTY_SYNC)) || \
    (defined(HAVE_PTHREAD) && defined(__CYGWIN__))
#define OPEN_PTY_ASYNC
#endif

#endif /* USE_LIBSSH2 */

typedef struct vt_pty_event_listener {
  void *self;

  /* Called when vt_pty_delete. */
  void (*closed)(void *);

  void (*show_config)(void *, char *);

} vt_pty_event_listener_t;

typedef struct vt_pty_hook {
  void *self;

  size_t (*pre_write)(void *, u_char *, size_t);

} vt_pty_hook_t;

typedef struct vt_pty *vt_pty_ptr_t;

vt_pty_ptr_t vt_pty_new(const char *cmd_path, char **cmd_argv, char **env, const char *host,
                        const char *work_dir, const char *pass, const char *pubkey,
                        const char *privkey, u_int cols, u_int rows, u_int width_pix,
                        u_int height_pix);

vt_pty_ptr_t vt_pty_new_with(int master, int slave, pid_t child_pid, u_int cols, u_int rows,
                             u_int width_pix, u_int height_pix);

int vt_pty_delete(vt_pty_ptr_t pty);

int vt_pty_set_listener(vt_pty_ptr_t pty, vt_pty_event_listener_t *pty_listener);

int vt_set_pty_winsize(vt_pty_ptr_t pty, u_int cols, u_int rows, u_int width_pix, u_int height_pix);

size_t vt_write_to_pty(vt_pty_ptr_t pty, u_char *buf, size_t len);

size_t vt_read_pty(vt_pty_ptr_t pty, u_char *buf, size_t left);

void vt_response_config(vt_pty_ptr_t pty, char *key, char *value, int to_menu);

pid_t vt_pty_get_pid(vt_pty_ptr_t pty);

int vt_pty_get_master_fd(vt_pty_ptr_t pty);

int vt_pty_get_slave_fd(vt_pty_ptr_t pty);

char *vt_pty_get_slave_name(vt_pty_ptr_t pty);

int vt_start_config_menu(vt_pty_ptr_t pty, char *cmd_path, int x, int y, char *display);

char *vt_pty_get_cmd_line(vt_pty_ptr_t pty);

void vt_pty_set_hook(vt_pty_ptr_t pty, vt_pty_hook_t *hook);

#ifdef USE_LIBSSH2
void *vt_search_ssh_session(const char *host, const char *port, const char *user);

int vt_pty_set_use_loopback(vt_pty_ptr_t pty, int use);

int vt_pty_ssh_scp(vt_pty_ptr_t pty, vt_char_encoding_t pty_encoding,
                   vt_char_encoding_t path_encoding, char *dst_path, char *src_path,
                   int use_scp_full);

void vt_pty_ssh_set_cipher_list(const char *list);

void vt_pty_ssh_set_keepalive_interval(u_int interval_sec);

int vt_pty_ssh_keepalive(u_int spent_msec);

void vt_pty_ssh_set_use_x11_forwarding(void *session, int use_x11_forwarding);

int vt_pty_ssh_poll(void *fds);

u_int vt_pty_ssh_get_x11_fds(int **fds);

int vt_pty_ssh_send_recv_x11(int idx, int bidirection);
#endif

#endif
