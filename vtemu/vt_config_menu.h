/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_CONFIG_MENU_H__
#define __VT_CONFIG_MENU_H__

#include <pobl/bl_types.h> /* pid_t */

#include "vt_pty.h"

typedef struct vt_config_menu {
#ifdef USE_WIN32API
  void *pid; /* HANDLE */
  void *fd;  /* HANDLE */
#else
  pid_t pid;
  int fd;
#endif

#ifdef USE_LIBSSH2
  vt_pty_ptr_t pty;
#endif

} vt_config_menu_t;

#ifdef NO_TOOLS

#define vt_config_menu_init(config_menu) (0)
#define vt_config_menu_final(config_menu) (0)
#define vt_config_menu_start(config_menu, cmd_path, x, y, display, pty) (0)
#define vt_config_menu_write(config_menu, buf, len) (0)

#else /* NO_TOOLS */

void vt_config_menu_init(vt_config_menu_t *config_menu);

void vt_config_menu_final(vt_config_menu_t *config_menu);

int vt_config_menu_start(vt_config_menu_t *config_menu, char *cmd_path, int x, int y, char *display,
                         vt_pty_ptr_t pty);

int vt_config_menu_write(vt_config_menu_t *config_menu, u_char *buf, size_t len);

#endif /* NO_TOOLS */

#endif
