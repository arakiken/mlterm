/*
 *	$Id$
 */

#include  "ml_color.h"

#include  <kiklib/kik_mem.h>


typedef struct  color_table
{
	char *  name ;
	ml_color_t  color ;
	
} color_table_t ;


/* --- static variables --- */

static color_table_t  color_table[] =
{
	{ "black" , MLC_BLACK } ,
	{ "red" , MLC_RED } ,
	{ "green" , MLC_GREEN } ,
	{ "yellow" , MLC_YELLOW } ,
	{ "blue" , MLC_BLUE } ,
	{ "magenta" , MLC_MAGENTA } ,
	{ "cyan" , MLC_CYAN } ,
	{ "white" , MLC_WHITE } ,
	{ "gray" , MLC_GRAY } ,
	{ "lightgray" , MLC_LIGHTGRAY } ,
	{ "pink" , MLC_PINK } ,
	{ "brown" , MLC_BROWN } ,
	{ "priv_fg" , MLC_PRIVATE_FG_COLOR } ,
	{ "priv_bg" , MLC_PRIVATE_BG_COLOR } ,
	
} ;


/* --- global functions --- */

ml_color_table_t
ml_color_table_dup(
	ml_color_table_t  color_table
	)
{
	ml_color_table_t  new_color_table ;

#ifdef  ANTI_ALIAS
	if( ( new_color_table = malloc( sizeof( XftColor *) * MAX_COLORS)) == NULL)
#else
	if( ( new_color_table = malloc( sizeof( XColor *) * MAX_COLORS)) == NULL)
#endif
	{
		return  NULL ;
	}

	memcpy( new_color_table , color_table ,
		#ifdef  ANTI_ALIAS
			sizeof( XftColor *) * MAX_COLORS
		#else
			sizeof( XColor *) * MAX_COLORS
		#endif
			) ;

	return  new_color_table ;
}

char *
ml_get_color_name(
	ml_color_t  color
	)
{
	int  counter ;
	
	/* MLC_PRIVATE_{FG|BG}_COLOR have no color names. */
	for( counter = 0 ; counter < MLC_PRIVATE_FG_COLOR ; counter ++)
	{
		if( color_table[counter].color == color)
		{
			return  color_table[counter].name ;
		}
	}

	return  NULL ;
}

ml_color_t
ml_get_color(
	char *  name
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < sizeof( color_table) / sizeof( color_table_t) ; counter ++)
	{
		if( strcmp( color_table[counter].name , name) == 0)
		{
			return  color_table[counter].color ;
		}
	}

	return  MLC_UNKNOWN_COLOR ;
}
