/*
 *	$Id$
 */

#include  "kik_utmp.h"

#include  <stdio.h>	/* NULL */
#include  "kik_mem.h"		/* malloc/free */


struct  kik_utmp
{
	char  *tty;
	int  fd ;

} ;

/* --- global functions --- */

kik_utmp_t
kik_utmp_new(
	char *  tty ,
	char *  host,
	int pty_fd
	)
{
	kik_utmp_t  utmp ;

	if( ( utmp = malloc( sizeof( *utmp))) == NULL)
	{
		return  NULL ;
	}
		
	if( ( utmp->tty = (char *) malloc( strlen( tty ) + 1)) == NULL)
	{
		return  NULL ;
	}

	strcpy(utmp->tty, tty);
	utmp->fd = pty_fd;

	addToUtmp(tty, host, pty_fd);

	return  utmp ;
}

int
kik_utmp_delete(
	kik_utmp_t  utmp
	)
{
	removeLineFromUtmp(utmp->tty, utmp->fd);
	free (utmp->tty);
	free (utmp);
	return  1 ;
}
