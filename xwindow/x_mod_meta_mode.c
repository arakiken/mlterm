/*
 *	$Id$
 */

#include  "x_mod_meta_mode.h"

#include  <string.h>		/* strcmp */


/* --- global functions --- */

x_mod_meta_mode_t
x_get_mod_meta_mode(
	char *  name
	)
{
	if( strcmp( name , "8bit") == 0)
	{
		return  MOD_META_SET_MSB ;
	}
	else if( strcmp( name , "esc") == 0)
	{
		return  MOD_META_OUTPUT_ESC ;
	}
	else
	{
		return  MOD_META_NONE ;
	}
}

char *
x_get_mod_meta_mode_name(
	x_mod_meta_mode_t  mode
	)
{
	if( mode == MOD_META_SET_MSB)
	{
		return  "8bit" ;
	}
	else if( mode == MOD_META_OUTPUT_ESC)
	{
		return  "esc" ;
	}
	else
	{
		return  "none" ;
	}
}
