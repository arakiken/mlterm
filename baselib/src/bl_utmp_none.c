/*
 *	$Id$
 */

#include "bl_utmp.h"

#include <stdio.h> /* NULL */

/* --- global functions --- */

bl_utmp_t bl_utmp_new(const char *tty, const char *host, int pty_fd) { return NULL; }

int bl_utmp_delete(bl_utmp_t utmp) { return 1; }
