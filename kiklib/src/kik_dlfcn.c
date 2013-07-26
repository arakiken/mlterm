/*
 *	$Id$
 */

#include  "kik_dlfcn.h"

#include  <stdlib.h>	/* atexit, realloc(Don't include kik_mem.h) */

#include  "kik_types.h"	/* u_int */


/* --- static variables --- */

static kik_dl_handle_t *  handles ;
static u_int  num_of_handles ;


/* --- static functions --- */

static void
close_at_exit(void)
{
	u_int  count ;

	for( count = 0 ; count < num_of_handles ; count++)
	{
		kik_dl_close( handles[count]) ;
	}

	free( handles) ;
}


/* --- global functions --- */

int
kik_dl_close_at_exit(
	kik_dl_handle_t  handle
	)
{
	void *  p ;

	if( ! ( p = realloc( handles , sizeof(kik_dl_handle_t) * (num_of_handles + 1))))
	{
		return  0 ;
	}

	handles = p ;

	if( num_of_handles == 0)
	{
		atexit( close_at_exit) ;
	}
	else
	{
		u_int  count ;

		for( count = 0 ; count < num_of_handles ; count++)
		{
			if( handles[count] == handle)
			{
				kik_dl_close( handle) ;

				return  1 ;
			}
		}
	}

	handles[num_of_handles++] = handle ;

	return  1 ;
}
