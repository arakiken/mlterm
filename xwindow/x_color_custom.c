/*
 *	$Id$
 */

#include  "x_color_custom.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_str.h>	/* strdup */


/* --- global functions --- */

int
x_color_custom_init(
	x_color_custom_t *  color_custom
	)
{
	kik_map_new_with_size( char * , x_rgb_t , color_custom->color_rgb_table ,
		kik_map_hash_str , kik_map_compare_str , 16) ;
	
	return  1 ;
}

int
x_color_custom_final(
	x_color_custom_t *  color_custom
	)
{
	int  count ;
	KIK_PAIR( x_color_rgb) *  array ;
	u_int  size ;
	
	kik_map_get_pairs_array( color_custom->color_rgb_table , array , size) ;
	
	for( count = 0 ; count < size ; count ++)
	{
		free( array[count]->key) ;
	}
	
	kik_map_delete( color_custom->color_rgb_table) ;

	return  1 ;
}

int
x_color_custom_set_rgb(
	x_color_custom_t *  color_custom ,
	char *  color ,
	u_short  red ,
	u_short  green ,
	u_short  blue
	)
{
	int  result ;
	KIK_PAIR( x_color_rgb)  pair ;
	x_rgb_t  rgb ;

	rgb.red = red ;
	rgb.green = green ;
	rgb.blue = blue ;
	
	kik_map_get( result , color_custom->color_rgb_table , color , pair) ;
	if( result)
	{
		pair->value = rgb ;
	}
	else
	{
		char *  _color ;

		_color = strdup( color) ;

		kik_map_set( result , color_custom->color_rgb_table , _color , rgb) ;
	}

	return  1 ;
}

int
x_color_custom_get_rgb(
	x_color_custom_t *  color_custom ,
	u_short *  red ,
	u_short *  green ,
	u_short *  blue ,
	char *  color
	)
{
	KIK_PAIR( x_color_rgb)  pair ;
	int  result ;

	kik_map_get( result , color_custom->color_rgb_table , color , pair) ;
	if( ! result)
	{
		return  0 ;
	}

	*red = pair->value.red ;
	*blue = pair->value.blue ;
	*green = pair->value.green ;

	return  1 ;
}

int
x_color_custom_read_conf(
	x_color_custom_t *  color_custom ,
	char *  filename
	)
{
	kik_file_t *  from ;
	char *  color ;
	char *  rgb ;
	u_int  red ;
	u_int  green ;
	u_int  blue ;

	if( ! ( from = kik_file_open( filename , "r")))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s couldn't be opened.\n" , filename) ;
	#endif
	
		return  0 ;
	}

	while( kik_conf_io_read( from , &color , &rgb))
	{
		if( strlen( rgb) == 14 &&
			sscanf( rgb , "%4x-%4x-%4x" , &red , &green , &blue) == 3)
		{
			/* do nothing */
		}
		else if( strlen( rgb) == 7 &&
				sscanf( rgb , "#%2x%2x%2x" , &red , &green , &blue) == 3)
		{
			red <<= 8 ;
			green <<= 8 ;
			blue <<= 8 ;
		}
		else
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " illegal rgblist format (%s,%s)\n" ,
				color , rgb) ;
		#endif

			continue ;
		}

	#ifdef  __DEBUG
		kik_debug_printf( "%s = red %x green %x blue %x\n" , color , red , green , blue) ;
	#endif

		x_color_custom_set_rgb( color_custom , color , red , green , blue) ;
	}

	kik_file_close( from) ;

	return  1 ;
}
