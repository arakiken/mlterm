/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __BL_PRIVILEGE_H__
#define __BL_PRIVILEGE_H__

#include "bl_types.h" /* uid_t / gid_t */

int bl_priv_change_euid(uid_t uid);

int bl_priv_restore_euid(void);

int bl_priv_change_egid(gid_t gid);

int bl_priv_restore_egid(void);

#endif
