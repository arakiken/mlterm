/*
 *	$Id$
 */

#include  "ml_sb_mode.h"


/* --- global functions --- */

ml_sb_mode_t
ml_get_sb_mode(
	char *  name
	)
{
	if( strcmp( name , "right") == 0)
	{
		return  SB_RIGHT ;
	}
	else if( strcmp( name , "left") == 0)
	{
		return  SB_LEFT ;
	}
	else /* if( strcmp( name , "none") == 0) */
	{
		return  SB_NONE ;
	}
}

char *
ml_get_sb_mode_name(
	ml_sb_mode_t  mode
	)
{
	if( mode == SB_RIGHT)
	{
		return  "right" ;
	}
	else if( mode == SB_LEFT)
	{
		return  "left" ;
	}
	else /* if( mode == SB_NONE) */
	{
		return  "none" ;
	}
}
