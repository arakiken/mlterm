/*
 *	$Id$
 */

/* Sample implementation - who/w  works;
                           finger doesn't
 */

#if 1
#define UTMPX
#endif

#include "kik_utmp.h"

#include <stdio.h>	/* NULL */
#include <pwd.h>
#include <string.h>	/* strncmp */
#include <time.h>	/* time */

#ifdef UTMPX
#include <utmpx.h>	/* getut*, setut*, etc */
#else
#include <utmp.h>	/* getut*, setut*, etc */
#endif

#include <sys/time.h>	/* timeval */
#include <unistd.h>	/* getuid */

#include "kik_util.h"	/* K_MIN */
#include "kik_mem.h"	/* malloc/free */

/* --- global functions --- */

kik_utmp_t
kik_utmp_new(
	     char * tty,
	     char * host,
	     int    pty_fd
	    )
{
  kik_utmp_t	utmp;
#ifdef UTMPX
  struct utmpx	ut;
#else
  struct utmp	ut;
#endif
  struct passwd * pwent;
  char		* pw_name;

  struct timeval timenow;

  gettimeofday(&timenow, NULL);

  if ( (utmp = malloc(sizeof(kik_utmp_t))) == NULL )
  {
    return NULL;
  }

  /* reset the input stream to the beginning of the file */
#ifdef UTMPX
  setutxent();
#else
  setutent();
#endif

  memset( &ut, 0, sizeof(ut) );

  if (
      ((pwent = getpwuid(getuid())) == NULL) ||
      (pwent->pw_name == NULL)
     )
  {
    pw_name = "?" ;
  }
  else
  {
    pw_name = pwent->pw_name ;
  }

  if( (strncmp(tty, "/dev/", K_MIN(5,strlen(tty)))) == 0 )
  {
    /* skip /dev/ prefix */
    tty += 5 ;
  }
	
  if (
      (strncmp(tty, "pts", K_MIN(3,strlen(tty))) != 0) &&
      (strncmp(tty, "pty", K_MIN(3,strlen(tty))) != 0) &&
      (strncmp(tty, "tty", K_MIN(3,strlen(tty))) != 0)
     )
  {
    free(utmp);

    return NULL;
  }

  /* fill the ut.ut_line */
#ifdef UTMPX
  strncpy( ut.ut_name, pw_name, K_MIN(sizeof(ut.ut_name), strlen(pw_name)) );
#else
  strncpy( ut.ut_user, pw_name, K_MIN(sizeof(ut.ut_name), strlen(pw_name)) );
#endif
	
  memcpy( ut.ut_line, tty, K_MIN(sizeof(ut.ut_line), strlen(tty)));

  ut.ut_pid	= getpid();
  ut.ut_type	= USER_PROCESS;
#ifdef UTMPX
  ut.ut_tv	= timenow;
#else
  ut.ut_time	= time(NULL);
#endif

#ifdef UTMPX
  memcpy(ut.ut_host, host, K_MIN(sizeof( ut.ut_host), strlen(host)));
#endif

  kik_priv_restore_euid();	/* useless? */
  kik_priv_restore_egid();

/*
  memcpy(utmp->ut_line, ut.ut_line, sizeof(utmp->ut_line));
 */

  /* insert new entry */
#ifdef UTMPX
  if ( !pututxline(&ut) )
#else
  if ( !pututline(&ut) )
#endif
  {
    return NULL;
  }

  kik_priv_change_euid(getuid());
  kik_priv_change_egid(getgid());

  return utmp;
}

int
kik_utmp_delete(
		kik_utmp_t utmp
	       )
{
#ifdef UTMPX
  struct utmpx	ut;
#else
  struct utmp	ut;
#endif

  kik_priv_restore_euid();
  kik_priv_restore_egid();

  /* reset the input stream to the beginning of the file */
#ifdef UTMPX
  setutxent();
#else
  setutent();
#endif

  memset( &ut, 0, sizeof(ut) );

  *ut.ut_user	= 0;
  ut.ut_pid	= getpid();
  ut.ut_type	= DEAD_PROCESS;
#ifdef UTMPX
  ut.ut_tv.tv_sec  = 0;
  ut.ut_tv.tv_usec = 0;
#else
  ut.ut_time	= 0;
#endif

#ifdef UTMPX
  pututxline(&ut);
#else
  pututline(&ut);
#endif

  kik_priv_change_euid(getuid());
  kik_priv_change_egid(getgid());

#ifdef UTMPX
  endutxent();
#else
  endutent();
#endif

  free(utmp);

  return 1;
}
