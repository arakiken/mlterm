/*
 *	$Id$
 */

#include  "ml_bel_mode.h"

#include  <string.h>		/* strcmp */


/* --- global functions --- */

ml_bel_mode_t
ml_get_bel_mode(
	char *  name
	)
{
	if( strcmp( name , "none") == 0)
	{
		return  BEL_NONE ;
	}
	else if( strcmp( name , "visual") == 0)
	{
		return  BEL_VISUAL ;
	}
	else /* if( strcmp( name , "sound") == 0) */
	{
		/* default */
		
		return  BEL_SOUND ;
	}
}

char *
ml_get_bel_mode_name(
	ml_bel_mode_t  mode
	)
{
	if( mode == BEL_NONE)
	{
		return  "none" ;
	}
	else if( mode == BEL_VISUAL)
	{
		return  "visual" ;
	}
	else /* if( mode == BEL_SOUND) */
	{
		return  "sound" ;
	}
}
