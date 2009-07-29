/*
 *	$Id$
 */

#include  "kik_unistd.h"

#include  <time.h>
#include  <unistd.h>	/* select */

#ifdef  USE_WIN32API
#include  <windows.h>
#endif


/* --- global functions --- */

#ifndef  HAVE_USLEEP

int
__kik_usleep(
	u_int  microseconds
	)
{
#ifndef USE_WIN32API
	struct timeval  tval ;
	
	tval.tv_usec = microseconds % 1000000 ;
	tval.tv_sec = microseconds / 1000000 ;

	if( select( 0 , NULL , NULL , NULL , &tval) == 0)
	{
		return  0 ;
	}
	else
	{
		return  -1 ;
	}
#else
	Sleep(microseconds / 1000);
	return  0 ;
#endif /* USE_WIN32API */
}

#endif

#ifndef  HAVE_UNSETENV

void
__kik_unsetenv(
	char *  name
	)
{
}

#endif


#ifndef  HAVE_GETUID

uid_t
__kik_getuid(void)
{
	return  0 ;
}

#endif


#ifndef  HAVE_GETGID

gid_t
__kik_getgid(void)
{
	return  0 ;
}

#endif
