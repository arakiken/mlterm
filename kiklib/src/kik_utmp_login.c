/*
 *	$Id$
 */

#include  "kik_utmp.h"

#include  <stdio.h>	/* NULL */
#include  <pwd.h>
#include  <sys/types.h>
#include  <unistd.h>	/* getuid */
#include  <string.h>	/* strncmp */
#include  <time.h>	/* time */
#if  1
/* *BSD has utmp.h anyway though login/logout aren't defined in it */
#include  <utmp.h>	/* login/logout(glibc2) you have to link libutil. */
#endif
#if  0
/* glibc(linux) doesn't have util.h */
#include  <util.h>	/* login/logout(*BSD) you have to link libutil. */
#endif

#include  "kik_util.h"		/* K_MIN */
#include  "kik_mem.h"		/* malloc/free */
#include  "kik_config.h"	/* HAVE_SETUTENT */
#include  "kik_privilege.h"

struct  kik_utmp
{
	char  ut_line[UT_LINESIZE] ;
} ;


/* --- global functions --- */

kik_utmp_t
kik_utmp_new(
	char *  tty ,
	char *  host ,
	int  pty_fd
	)
{
	kik_utmp_t  utmp ;
	struct utmp ut;
	struct passwd *  pwent;
	char *  pw_name;

	if( ( utmp = malloc( sizeof( *utmp))) == NULL)
	{
		return  NULL ;
	}

/* unnecessary ? */
#if  0
#ifdef  HAVE_SETUTENT
	setutent();
#endif
#endif

	memset( &ut , 0 , sizeof( ut)) ;

	if( ( pwent = getpwuid( getuid())) == NULL || pwent->pw_name == NULL)
	{
		pw_name = "?" ;
	}
	else
	{
		pw_name = pwent->pw_name ;
	}

	/*
	 * user name field is named ut_name in *BSD and is ut_user in glibc2 ,
	 * but glibc2 also defines ut_name as an alias of ut_user for backward
	 * compatibility.
	 */
	strncpy( ut.ut_name , pw_name , K_MIN(sizeof( ut.ut_name),strlen(pw_name))) ;
	
	if( strncmp( tty, "/dev/", K_MIN(5,strlen(tty))) == 0)
	{
		/* skip /dev/ prefix */
		tty += 5 ;
	}
	
	if( strncmp( tty, "pts", K_MIN(3,strlen(tty))) != 0 &&
	    strncmp( tty, "pty", K_MIN(3,strlen(tty))) != 0 &&
	    strncmp( tty, "tty", K_MIN(3,strlen(tty))) != 0)
	{
		free(utmp);

		return NULL;
	}

#ifndef  HAVE_SETUTENT
	/* ut.ut_line must be filled before login() on bsd. */
	memcpy( ut.ut_line , tty , K_MIN(sizeof(ut.ut_line),strlen(tty))) ;
#endif
	
	ut.ut_time =  time(NULL);
	memcpy( ut.ut_host , host , K_MIN(sizeof( ut.ut_host),strlen(host)));
	kik_priv_restore_euid() ;/* useless? */
	kik_priv_restore_egid() ;

	/* login does not give us error information... */
	login(&ut);
	
	kik_priv_change_euid( getuid()) ;
	kik_priv_change_egid( getgid()) ;
	memcpy(utmp->ut_line , ut.ut_line ,  sizeof( utmp->ut_line)) ;

	return utmp;
}

int
kik_utmp_delete(
	kik_utmp_t  utmp
	)
{
	kik_priv_restore_euid() ;
	kik_priv_restore_egid() ;

	logout(utmp->ut_line);
	logwtmp(utmp->ut_line,"","") ;
	
	kik_priv_change_euid( getuid()) ;
	kik_priv_change_egid( getgid()) ;

/* unnecessary ? */
#if  0
#ifdef  HAVE_SETUTENT
	endutent();
#endif
#endif

	free(utmp) ;

	return  1 ;
}
