/*
 *	$Id$
 */

#include  "x_bel_mode.h"

#include  <string.h>		/* strcmp */


/* --- static variables --- */

/* Order of this table must be same as x_bel_mode_t. */
static char *   bel_mode_name_table[] =
{
	"none" , "sound" , "visual" ,
} ;


/* --- global functions --- */

x_bel_mode_t
x_get_bel_mode_by_name(
	char *  name
	)
{
	x_bel_mode_t  mode ;

	for( mode = 0 ; mode < BEL_MODE_MAX ; mode++)
	{
		if( strcmp( bel_mode_name_table[mode] , name) == 0)
		{
			return  mode ;
		}
	}
	
	/* default value */
	return  BEL_SOUND ;
}

char *
x_get_bel_mode_name(
	x_bel_mode_t  mode
	)
{
	if( mode < 0 || BEL_MODE_MAX <= mode)
	{
		/* default value */
		mode = BEL_SOUND ;
	}

	return  bel_mode_name_table[mode] ;
}
