/*
 *	$Id$
 */

#include  "ml_color_custom.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_conf_io.h>


/* --- global functions --- */

int
ml_color_custom_init(
	ml_color_custom_t *  color_custom
	)
{
	memset( color_custom->rgbs , 0 , sizeof( color_custom->rgbs)) ;
	
	return  1 ;
}

int
ml_color_custom_final(
	ml_color_custom_t *  color_custom
	)
{
	ml_color_t  color ;
	
	for( color = 0 ; color < MAX_ACTUAL_COLORS ; color ++)
	{
		free( color_custom->rgbs[color]) ;
	}

	return  1 ;
}

int
ml_color_custom_set_rgb(
	ml_color_custom_t *  color_custom ,
	ml_color_t  color ,
	u_short  red ,
	u_short  green ,
	u_short  blue
	)
{
	if( color_custom->rgbs[color])
	{
		free( color_custom->rgbs[color]) ;
		color_custom->rgbs[color] = NULL ;
	}
	
	if( ( color_custom->rgbs[color] = malloc( sizeof( *color_custom->rgbs))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  0 ;
	}

	color_custom->rgbs[color]->red = red ;
	color_custom->rgbs[color]->green = green ;
	color_custom->rgbs[color]->blue = blue ;

	return  1 ;
}

int
ml_color_custom_read_conf(
	ml_color_custom_t *  color_custom ,
	char *  filename
	)
{
	kik_file_t *  from ;
	char *  name ;
	char *  rgb ;
	u_int  red ;
	u_int  green ;
	u_int  blue ;
	ml_color_t  color ;

	if( ! ( from = kik_file_open( filename , "r")))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s couldn't be opened.\n" , filename) ;
	#endif
	
		return  0 ;
	}

	while( kik_conf_io_read( from , &name , &rgb))
	{
		if( sscanf( rgb , "%x-%x-%x" , &red , &green , &blue) != 3)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " illegal rgblist format (%s,%s)\n" ,
				name , rgb) ;
		#endif

			continue ;
		}

	#ifdef  __DEBUG
		kik_debug_printf( "%s = red %x green %x blue %x\n" , name , red , green , blue) ;
	#endif

		if( ( color = ml_get_color( name)) != MLC_UNKNOWN_COLOR)
		{
			ml_color_custom_set_rgb( color_custom , color , red , green , blue) ;
		}
	}

	kik_file_close( from) ;

	return  1 ;
}

int
ml_color_custom_get_rgb(
	ml_color_custom_t *  color_custom ,
	u_short *  red ,
	u_short *  green ,
	u_short *  blue ,
	ml_color_t  color
	)
{
	if( 0 <= color && color < MLC_PRIVATE_FG_COLOR && color_custom->rgbs[color])
	{
		*red = color_custom->rgbs[color]->red ;
		*blue = color_custom->rgbs[color]->blue ;
		*green = color_custom->rgbs[color]->green ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}
