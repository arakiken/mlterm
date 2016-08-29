/*
 *	$Id$
 */

#ifndef __BL_UTMP_H__
#define __BL_UTMP_H__

typedef struct bl_utmp* bl_utmp_t;

bl_utmp_t bl_utmp_new(const char* tty, const char* host, int pty_fd);

int bl_utmp_delete(bl_utmp_t utmp);

#endif
