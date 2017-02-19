/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "bl_pty.h"

/* --- global functions --- */

pid_t bl_pty_fork(int *master, int *slave) {
  /* do nothing. */
  return 0;
}

int bl_pty_close(int master) { return 0; }

void bl_pty_helper_set_flag(int lastlog, int utmp, int wtmp) {}
