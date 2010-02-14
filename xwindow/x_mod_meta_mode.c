/*
 *	$Id$
 */

#include  "x_mod_meta_mode.h"

#include  <string.h>		/* strcmp */


/* --- static variables --- */

/* Order of this table must be same as x_mod_meta_mode_t. */
static char *   mod_meta_mode_name_table[] =
{
	"none" , "esc" , "8bit" ,
} ;


/* --- global functions --- */

x_mod_meta_mode_t
x_get_mod_meta_mode(
	char *  name
	)
{
	x_mod_meta_mode_t  mode ;

	for( mode = 0 ; mode < MOD_META_MODE_MAX ; mode++)
	{
		if( strcmp( mod_meta_mode_name_table[mode] , name) == 0)
		{
			return  mode ;
		}
	}
	
	/* default value */
	return  MOD_META_NONE ;
}

char *
x_get_mod_meta_mode_name(
	x_mod_meta_mode_t  mode
	)
{
	if( mode < 0 || MOD_META_MODE_MAX <= mode)
	{
		/* default value */
		mode = MOD_META_NONE ;
	}

	return  mod_meta_mode_name_table[mode] ;
}
