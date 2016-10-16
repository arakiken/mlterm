/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __BL_PTY_H__
#define __BL_PTY_H__

#include "bl_types.h" /* pid_t */

pid_t bl_pty_fork(int* master, int* slave);

int bl_pty_close(int master);

void bl_pty_helper_set_flag(int lastlog, int utmp, int wtmp);

#endif
