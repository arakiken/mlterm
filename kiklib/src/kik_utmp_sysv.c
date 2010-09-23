/*
 *	$Id$
 */

/* Sample implementation - who/w  works;
                           finger doesn't (WHY ??)
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
#include "kik_privilege.h"
#include "kik_debug.h"


#ifdef UTMPX
#define LINE_WIDTH 32
#else
#define LINE_WIDTH 12
#endif

struct  kik_utmp
{
        char  ut_line[LINE_WIDTH];
        char  ut_pos[4];
};

/* --- global functions --- */

kik_utmp_t
kik_utmp_new(
	     char * tty,
	     char * host,
	     int    pty_fd
	    )
{
#ifdef UTMPX
  struct utmpx	ut;
#else
  struct utmp	ut;
#endif

  kik_utmp_t      utmp;
  struct passwd * pwent;
  char          * pw_name;
  char          * tty_num;

  struct timeval timenow;

  gettimeofday(&timenow, NULL);

  if ( (utmp = malloc(sizeof(*utmp))) == NULL )
  {
    return NULL;
  }

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

  if((tty_num = strchr(tty, '/')))
  {
    tty_num++;
  }
  else if((strncmp(tty + 1, "typ", K_MIN(3,strlen(tty + 1)))) == 0)
  {
    /* /dev/ttypN or /dev/ptypN */
    tty_num = tty + 4;
  }
  else
  {
    free(utmp);

    return NULL;
  }

  strncpy( ut.ut_user, pw_name, K_MIN(sizeof(ut.ut_user), strlen(pw_name)) );
  memcpy( ut.ut_id, tty_num, K_MIN(sizeof(ut.ut_id), strlen(tty_num)) );
  memcpy( ut.ut_line, tty, K_MIN(sizeof(ut.ut_line), strlen(tty)) );

  ut.ut_pid	= getpid();
  ut.ut_type	= USER_PROCESS;

#ifdef UTMPX
  ut.ut_tv.tv_sec	= timenow.tv_sec;
  ut.ut_tv.tv_usec	= timenow.tv_usec;
#else
  ut.ut_time	= time(NULL);
#endif

#ifdef UTMPX
  memcpy(ut.ut_host, host, K_MIN(sizeof(ut.ut_host), strlen(host)));
  ut.ut_session = getsid(0) ;
#endif

  memcpy( utmp->ut_line, tty,
	  K_MIN(sizeof(utmp->ut_line), strlen(tty)) );
  memcpy( utmp->ut_pos, tty_num,
	  K_MIN(sizeof(utmp->ut_pos), strlen(tty_num)) );

  kik_priv_restore_euid();	/* useless? */
  kik_priv_restore_egid();

  /* reset the input stream to the beginning of the file */
#ifdef UTMPX
  setutxent();
#else
  setutent();
#endif

  /* insert new entry */
#ifdef UTMPX
  if ( !pututxline(&ut) )
  {
    free(utmp);
    utmp = NULL;
  }
#else
  /* pututline doesn't return value in XPG2 and SVID2 */
  pututline(&ut) ;
#endif

#ifdef  DEBUG
  if( ! utmp)
  {
	kik_debug_printf( KIK_DEBUG_TAG "pututline failed. EUID %d EGID %d => " ,
		geteuid() , getegid()) ;
	perror(NULL);
  }
#endif

#ifdef UTMPX
  endutxent();
#else
  endutent();
#endif
  
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

  memset( &ut, 0, sizeof(ut) );

  memcpy( ut.ut_id, utmp->ut_pos,
	  K_MIN(sizeof(ut.ut_id), strlen(utmp->ut_pos)) );
  memcpy( ut.ut_line, utmp->ut_line,
	  K_MIN(sizeof(ut.ut_line), strlen(utmp->ut_line)) );
  memset( ut.ut_user, 0, sizeof(ut.ut_user));

  ut.ut_pid	= getpid();
  ut.ut_type	= DEAD_PROCESS;

#ifdef UTMPX
  ut.ut_tv.tv_sec  = 0;
  ut.ut_tv.tv_usec = 0;
#else
  ut.ut_time	= 0;
#endif

  kik_priv_restore_euid();
  kik_priv_restore_egid();

  /* reset the input stream to the beginning of the file */
#ifdef UTMPX
  setutxent();
  pututxline(&ut);
  endutxent();
#else
  setutent();
  pututline(&ut);
  endutent();
#endif

  kik_priv_change_euid(getuid());
  kik_priv_change_egid(getgid());

  free(utmp);

  return 1;
}
