/*
 *	$Id$
 */

#include  "x_sb_mode.h"

#include  <string.h>
#include  <kiklib/kik_types.h>	/* u_int */


/* --- static variables --- */

/* Order of this table must be same as x_sb_mode_t. */
static char *   sb_mode_name_table[] =
{
	"none" , "left" , "right" , "autohide" ,
} ;


/* --- global functions --- */

x_sb_mode_t
x_get_sb_mode_by_name(
	char *  name
	)
{
#ifndef  USE_CONSOLE
	x_sb_mode_t  mode ;

	for( mode = 0 ; mode < SBM_MAX ; mode++)
	{
		if( strcmp( sb_mode_name_table[mode] , name) == 0)
		{
			return  mode ;
		}
	}
#endif

	/* default value */
	return  SBM_NONE ;
}

char *
x_get_sb_mode_name(
	x_sb_mode_t  mode
	)
{
	if( (u_int)mode >= SBM_MAX)
	{
		/* default value */
		mode = SBM_NONE ;
	}

	return  sb_mode_name_table[mode] ;
}
