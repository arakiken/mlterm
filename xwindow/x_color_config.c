/*
 *	$Id$
 */

#include  "x_color_config.h"

#include  <stdio.h>		/* sscanf */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_str.h>	/* strdup */
#include  <ml_color.h>


/* --- static variables --- */

static char *  color_file = "mlterm/color" ;


/* --- static functions --- */

static KIK_PAIR( x_color_rgb)
get_color_rgb_pair(
	KIK_MAP( x_color_rgb)  table ,
	char *  color
	)
{
	int  result ;
	KIK_PAIR( x_color_rgb)  pair ;

	kik_map_get( result , table , color , pair) ;
	if( result)
	{
		return  pair ;
	}
	else
	{
		return  NULL ;
	}
}

static int
parse_conf(
	x_color_config_t *  color_config ,
	char *  color ,
	char *  rgb
	)
{
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;

	if( *rgb == '\0')
	{
		KIK_PAIR(x_color_rgb)  pair ;
		char *  key ;
		int  result ;

		if( ( pair = get_color_rgb_pair( color_config->color_rgb_table , color)) == NULL)
		{
			return  0 ;
		}
		
		/* pair->key is refered and pair itself is deleted in kik_map_erase.*/
		key = pair->key ;
		kik_map_erase_simple( result , color_config->color_rgb_table , color) ;
		free( key) ;
		
		return  1 ;
	}
	else if( ! ml_color_parse_rgb_name( &red, &green, &blue, rgb))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " illegal rgblist format (%s,%s)\n" ,
			color , rgb) ;
	#endif

		return  0 ;
	}

#ifdef  __DEBUG
	kik_debug_printf( "%s = red %x green %x blue %x\n" , color , red , green , blue) ;
#endif

	return  x_color_config_set_rgb( color_config , color , red , green , blue) ;
}

static int
read_conf(
	x_color_config_t *  color_config ,
	const char *  filename
	)
{
	kik_file_t *  from ;
	char *  color ;
	char *  rgb ;

	if( ! ( from = kik_file_open( filename , "r")))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s couldn't be opened.\n" , filename) ;
	#endif
	
		return  0 ;
	}

	while( kik_conf_io_read( from , &color , &rgb))
	{
		parse_conf( color_config, color, rgb) ;
	}

	kik_file_close( from) ;

	return  1 ;
}


/* --- global functions --- */

int
x_color_config_init(
	x_color_config_t *  color_config
	)
{
	char *  rcpath ;
	
	kik_map_new_with_size( char * , x_rgb_t , color_config->color_rgb_table ,
		kik_map_hash_str , kik_map_compare_str , 16) ;

	if( ( rcpath = kik_get_sys_rc_path( color_file)))
	{
		read_conf( color_config , rcpath) ;
		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( color_file)))
	{
		read_conf( color_config , rcpath) ;
		free( rcpath) ;
	}
	
	return  1 ;
}

int
x_color_config_final(
	x_color_config_t *  color_config
	)
{
	int  count ;
	KIK_PAIR( x_color_rgb) *  array ;
	u_int  size ;
	
	kik_map_get_pairs_array( color_config->color_rgb_table , array , size) ;
	
	for( count = 0 ; count < size ; count ++)
	{
		free( array[count]->key) ;
	}
	
	kik_map_delete( color_config->color_rgb_table) ;

	return  1 ;
}

int
x_color_config_set_rgb(
	x_color_config_t *  color_config ,
	char *  color ,
	u_int8_t  red ,
	u_int8_t  green ,
	u_int8_t  blue
	)
{
	ml_color_t  _color ;
	KIK_PAIR( x_color_rgb)  pair ;
	x_rgb_t  rgb ;

	/*
	 * Illegal color name is rejected.
	 */
	if( ( _color = ml_get_color( color)) == ML_UNKNOWN_COLOR)
	{
		return  0 ;
	}

	rgb.red = red ;
	rgb.green = green ;
	rgb.blue = blue ;

	if( ( pair = get_color_rgb_pair( color_config->color_rgb_table , color)))
	{
		if( pair->value.red == red && pair->value.green == green &&
			pair->value.blue == blue)
		{
			/* Not changed */
			
			return  0 ;
		}

		pair->value = rgb ;
	}
	else
	{
		int  result ;

		/*
		 * 256 color: Same rgb as default is rejected.
		 * Sys color: Any rgb is accepted, because RGB of ml_get_color_rgb is not
		 *            always same as that of XAllocNamedColor(=rgb.txt)
		 */
		if( IS_256_COLOR( _color))
		{
			u_int8_t  _red ;
			u_int8_t  _green ;
			u_int8_t  _blue ;

			if( ! ml_get_color_rgb( _color, &_red, &_green, &_blue))
			{
				return  0 ;
			}

			if( red == _red && green == _green && blue == _blue)
			{
				/* Not changed */

			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG
					" color %d'rgb(%02x%02x%02x) not changed.\n",
					_color, red, green, blue) ;
			#endif

				return  0 ;
			}
		#ifdef  DEBUG
			else
			{
				kik_debug_printf( KIK_DEBUG_TAG
					" color %d's rgb(%02x%02x%02x) changed => %02x%02x%02x.\n",
					_color, _red, _green, _blue, red, green, blue) ;
			}
		#endif
		}
		
		if( ! ( color = strdup( color)))
		{
			return  0 ;
		}
		
		kik_map_set( result , color_config->color_rgb_table , color , rgb) ;
		
		if( ! result)
		{
			free( color) ;
			
			return  0 ;
		}
	}

	return  1 ;
}

int
x_color_config_get_rgb(
	x_color_config_t *  color_config ,
	u_int8_t *  red ,
	u_int8_t *  green ,
	u_int8_t *  blue ,
	char *  color
	)
{
	KIK_PAIR( x_color_rgb)  pair ;

	if( ( pair = get_color_rgb_pair( color_config->color_rgb_table , color)) == NULL)
	{
		return  0 ;
	}

	*red = pair->value.red ;
	*blue = pair->value.blue ;
	*green = pair->value.green ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s's rgb => %d %d %d\n", color , *red, *blue, *green) ;
#endif

	return  1 ;
}

/*
 * Return value 0 means customization failed or not changed.
 * Return value -1 means saving failed.
 */
int
x_customize_color_file(
	x_color_config_t *  color_config ,
	char *  color ,
	char *  rgb ,
	int  save
	)
{
	if( ! parse_conf( color_config, color, rgb))
	{
		return  0 ;
	}
	
	if( save)
	{
		char *  path ;
		kik_conf_write_t *  conf ;

		if( ( path = kik_get_user_rc_path( color_file)) == NULL)
		{
			return  -1 ;
		}

		conf = kik_conf_write_open( path) ;
		free( path) ;
		if( conf == NULL)
		{
			return  -1 ;
		}

		kik_conf_io_write( conf , color , rgb) ;

		kik_conf_write_close( conf) ;
	}

	return  1 ;
}
