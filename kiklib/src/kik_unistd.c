/*
 *	$Id$
 */

#include  "kik_unistd.h"

#include  <time.h>
#include  <unistd.h>	/* select */


/* --- global functions --- */

#ifndef  HAVE_USLEEP

int
__kik_usleep(
	u_int  microseconds
	)
{
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
}

#endif
