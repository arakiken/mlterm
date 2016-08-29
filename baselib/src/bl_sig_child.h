/*
 *	$Id$
 */

#ifndef __BL_SIG_CHILD_H__
#define __BL_SIG_CHILD_H__

#include "bl_types.h" /* pid_t */

int bl_sig_child_init(void);

int bl_sig_child_final(void);

int bl_sig_child_suspend(void);

int bl_sig_child_resume(void);

int bl_add_sig_child_listener(void *self, void (*exited)(void *, pid_t));

int bl_remove_sig_child_listener(void *self, void (*exited)(void *, pid_t));

void bl_trigger_sig_child(pid_t pid);

#endif
