/*
 *	$Id$
 */

#include  "kik_utmp.h"

#include  <stdio.h>		/* NULL */

/* --- global functions --- */

kik_utmp_t
kik_utmp_new(
	char *  tty ,
	char *  host ,
	int  pty_fd
	)
{
	return  NULL ;
}

int
kik_utmp_delete(
	kik_utmp_t  utmp
	)
{
	return  1 ;
}
