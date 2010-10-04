/*
 *	$Id$
 */

#include  "kik_pty.h"

/* --- global functions --- */

pid_t
kik_pty_fork(
	int *  master ,
	int *  slave
	)
{
	/* do nothing. */
	return  0 ;
}

int
kik_pty_helper_close(
	int  pty
	)
{
	return  0 ;
}

void
kik_pty_helper_set_flag(
	int  lastlog ,
	int  utmp ,
	int  wtmp
	)
{
}
