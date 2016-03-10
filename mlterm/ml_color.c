/*
 *	$Id$
 */

#include  "ml_color.h"

#include  <stdio.h>		/* sscanf */
#include  <string.h>		/* strcmp */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_map.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_str.h>	/* strdup */


typedef struct rgb
{
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;
	u_int8_t  alpha ;

} rgb_t ;

KIK_MAP_TYPEDEF( color_rgb , ml_color_t , rgb_t) ;


/* --- static variables --- */

static char *  color_name_table[] =
{
	"hl_black" ,
	"hl_red" ,
	"hl_green" ,
	"hl_yellow" ,
	"hl_blue" ,
	"hl_magenta" ,
	"hl_cyan" ,
	"hl_white" ,
} ;

static char  color_name_256[4] ;

static u_int8_t vtsys_color_rgb_table[][3] =
{
	{ 0x00, 0x00, 0x00 },
	{ 0xcd, 0x00, 0x00 },
	{ 0x00, 0xcd, 0x00 },
	{ 0xcd, 0xcd, 0x00 },
	{ 0x00, 0x00, 0xee },
	{ 0xcd, 0x00, 0xcd },
	{ 0x00, 0xcd, 0xcd },
	{ 0xe5, 0xe5, 0xe5 },

	{ 0x7f, 0x7f, 0x7f },
	{ 0xff, 0x00, 0x00 },
	{ 0x00, 0xff, 0x00 },
	{ 0xff, 0xff, 0x00 },
	{ 0x5c, 0x5c, 0xff },
	{ 0xff, 0x00, 0xff },
	{ 0x00, 0xff, 0xff },
	{ 0xff, 0xff, 0xff },
} ;

#if  0
static u_int8_t color256_rgb_table[][3] =
{
	/* CUBE COLOR(0x10-0xe7) */
	{ 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x5f },
	{ 0x00, 0x00, 0x87 },
	{ 0x00, 0x00, 0xaf },
	{ 0x00, 0x00, 0xd7 },
	{ 0x00, 0x00, 0xff },
	{ 0x00, 0x5f, 0x00 },
	{ 0x00, 0x5f, 0x5f },
	{ 0x00, 0x5f, 0x87 },
	{ 0x00, 0x5f, 0xaf },
	{ 0x00, 0x5f, 0xd7 },
	{ 0x00, 0x5f, 0xff },
	{ 0x00, 0x87, 0x00 },
	{ 0x00, 0x87, 0x5f },
	{ 0x00, 0x87, 0x87 },
	{ 0x00, 0x87, 0xaf },
	{ 0x00, 0x87, 0xd7 },
	{ 0x00, 0x87, 0xff },
	{ 0x00, 0xaf, 0x00 },
	{ 0x00, 0xaf, 0x5f },
	{ 0x00, 0xaf, 0x87 },
	{ 0x00, 0xaf, 0xaf },
	{ 0x00, 0xaf, 0xd7 },
	{ 0x00, 0xaf, 0xff },
	{ 0x00, 0xd7, 0x00 },
	{ 0x00, 0xd7, 0x5f },
	{ 0x00, 0xd7, 0x87 },
	{ 0x00, 0xd7, 0xaf },
	{ 0x00, 0xd7, 0xd7 },
	{ 0x00, 0xd7, 0xff },
	{ 0x00, 0xff, 0x00 },
	{ 0x00, 0xff, 0x5f },
	{ 0x00, 0xff, 0x87 },
	{ 0x00, 0xff, 0xaf },
	{ 0x00, 0xff, 0xd7 },
	{ 0x00, 0xff, 0xff },
	{ 0x5f, 0x00, 0x00 },
	{ 0x5f, 0x00, 0x5f },
	{ 0x5f, 0x00, 0x87 },
	{ 0x5f, 0x00, 0xaf },
	{ 0x5f, 0x00, 0xd7 },
	{ 0x5f, 0x00, 0xff },
	{ 0x5f, 0x5f, 0x00 },
	{ 0x5f, 0x5f, 0x5f },
	{ 0x5f, 0x5f, 0x87 },
	{ 0x5f, 0x5f, 0xaf },
	{ 0x5f, 0x5f, 0xd7 },
	{ 0x5f, 0x5f, 0xff },
	{ 0x5f, 0x87, 0x00 },
	{ 0x5f, 0x87, 0x5f },
	{ 0x5f, 0x87, 0x87 },
	{ 0x5f, 0x87, 0xaf },
	{ 0x5f, 0x87, 0xd7 },
	{ 0x5f, 0x87, 0xff },
	{ 0x5f, 0xaf, 0x00 },
	{ 0x5f, 0xaf, 0x5f },
	{ 0x5f, 0xaf, 0x87 },
	{ 0x5f, 0xaf, 0xaf },
	{ 0x5f, 0xaf, 0xd7 },
	{ 0x5f, 0xaf, 0xff },
	{ 0x5f, 0xd7, 0x00 },
	{ 0x5f, 0xd7, 0x5f },
	{ 0x5f, 0xd7, 0x87 },
	{ 0x5f, 0xd7, 0xaf },
	{ 0x5f, 0xd7, 0xd7 },
	{ 0x5f, 0xd7, 0xff },
	{ 0x5f, 0xff, 0x00 },
	{ 0x5f, 0xff, 0x5f },
	{ 0x5f, 0xff, 0x87 },
	{ 0x5f, 0xff, 0xaf },
	{ 0x5f, 0xff, 0xd7 },
	{ 0x5f, 0xff, 0xff },
	{ 0x87, 0x00, 0x00 },
	{ 0x87, 0x00, 0x5f },
	{ 0x87, 0x00, 0x87 },
	{ 0x87, 0x00, 0xaf },
	{ 0x87, 0x00, 0xd7 },
	{ 0x87, 0x00, 0xff },
	{ 0x87, 0x5f, 0x00 },
	{ 0x87, 0x5f, 0x5f },
	{ 0x87, 0x5f, 0x87 },
	{ 0x87, 0x5f, 0xaf },
	{ 0x87, 0x5f, 0xd7 },
	{ 0x87, 0x5f, 0xff },
	{ 0x87, 0x87, 0x00 },
	{ 0x87, 0x87, 0x5f },
	{ 0x87, 0x87, 0x87 },
	{ 0x87, 0x87, 0xaf },
	{ 0x87, 0x87, 0xd7 },
	{ 0x87, 0x87, 0xff },
	{ 0x87, 0xaf, 0x00 },
	{ 0x87, 0xaf, 0x5f },
	{ 0x87, 0xaf, 0x87 },
	{ 0x87, 0xaf, 0xaf },
	{ 0x87, 0xaf, 0xd7 },
	{ 0x87, 0xaf, 0xff },
	{ 0x87, 0xd7, 0x00 },
	{ 0x87, 0xd7, 0x5f },
	{ 0x87, 0xd7, 0x87 },
	{ 0x87, 0xd7, 0xaf },
	{ 0x87, 0xd7, 0xd7 },
	{ 0x87, 0xd7, 0xff },
	{ 0x87, 0xff, 0x00 },
	{ 0x87, 0xff, 0x5f },
	{ 0x87, 0xff, 0x87 },
	{ 0x87, 0xff, 0xaf },
	{ 0x87, 0xff, 0xd7 },
	{ 0x87, 0xff, 0xff },
	{ 0xaf, 0x00, 0x00 },
	{ 0xaf, 0x00, 0x5f },
	{ 0xaf, 0x00, 0x87 },
	{ 0xaf, 0x00, 0xaf },
	{ 0xaf, 0x00, 0xd7 },
	{ 0xaf, 0x00, 0xff },
	{ 0xaf, 0x5f, 0x00 },
	{ 0xaf, 0x5f, 0x5f },
	{ 0xaf, 0x5f, 0x87 },
	{ 0xaf, 0x5f, 0xaf },
	{ 0xaf, 0x5f, 0xd7 },
	{ 0xaf, 0x5f, 0xff },
	{ 0xaf, 0x87, 0x00 },
	{ 0xaf, 0x87, 0x5f },
	{ 0xaf, 0x87, 0x87 },
	{ 0xaf, 0x87, 0xaf },
	{ 0xaf, 0x87, 0xd7 },
	{ 0xaf, 0x87, 0xff },
	{ 0xaf, 0xaf, 0x00 },
	{ 0xaf, 0xaf, 0x5f },
	{ 0xaf, 0xaf, 0x87 },
	{ 0xaf, 0xaf, 0xaf },
	{ 0xaf, 0xaf, 0xd7 },
	{ 0xaf, 0xaf, 0xff },
	{ 0xaf, 0xd7, 0x00 },
	{ 0xaf, 0xd7, 0x5f },
	{ 0xaf, 0xd7, 0x87 },
	{ 0xaf, 0xd7, 0xaf },
	{ 0xaf, 0xd7, 0xd7 },
	{ 0xaf, 0xd7, 0xff },
	{ 0xaf, 0xff, 0x00 },
	{ 0xaf, 0xff, 0x5f },
	{ 0xaf, 0xff, 0x87 },
	{ 0xaf, 0xff, 0xaf },
	{ 0xaf, 0xff, 0xd7 },
	{ 0xaf, 0xff, 0xff },
	{ 0xd7, 0x00, 0x00 },
	{ 0xd7, 0x00, 0x5f },
	{ 0xd7, 0x00, 0x87 },
	{ 0xd7, 0x00, 0xaf },
	{ 0xd7, 0x00, 0xd7 },
	{ 0xd7, 0x00, 0xff },
	{ 0xd7, 0x5f, 0x00 },
	{ 0xd7, 0x5f, 0x5f },
	{ 0xd7, 0x5f, 0x87 },
	{ 0xd7, 0x5f, 0xaf },
	{ 0xd7, 0x5f, 0xd7 },
	{ 0xd7, 0x5f, 0xff },
	{ 0xd7, 0x87, 0x00 },
	{ 0xd7, 0x87, 0x5f },
	{ 0xd7, 0x87, 0x87 },
	{ 0xd7, 0x87, 0xaf },
	{ 0xd7, 0x87, 0xd7 },
	{ 0xd7, 0x87, 0xff },
	{ 0xd7, 0xaf, 0x00 },
	{ 0xd7, 0xaf, 0x5f },
	{ 0xd7, 0xaf, 0x87 },
	{ 0xd7, 0xaf, 0xaf },
	{ 0xd7, 0xaf, 0xd7 },
	{ 0xd7, 0xaf, 0xff },
	{ 0xd7, 0xd7, 0x00 },
	{ 0xd7, 0xd7, 0x5f },
	{ 0xd7, 0xd7, 0x87 },
	{ 0xd7, 0xd7, 0xaf },
	{ 0xd7, 0xd7, 0xd7 },
	{ 0xd7, 0xd7, 0xff },
	{ 0xd7, 0xff, 0x00 },
	{ 0xd7, 0xff, 0x5f },
	{ 0xd7, 0xff, 0x87 },
	{ 0xd7, 0xff, 0xaf },
	{ 0xd7, 0xff, 0xd7 },
	{ 0xd7, 0xff, 0xff },
	{ 0xff, 0x00, 0x00 },
	{ 0xff, 0x00, 0x5f },
	{ 0xff, 0x00, 0x87 },
	{ 0xff, 0x00, 0xaf },
	{ 0xff, 0x00, 0xd7 },
	{ 0xff, 0x00, 0xff },
	{ 0xff, 0x5f, 0x00 },
	{ 0xff, 0x5f, 0x5f },
	{ 0xff, 0x5f, 0x87 },
	{ 0xff, 0x5f, 0xaf },
	{ 0xff, 0x5f, 0xd7 },
	{ 0xff, 0x5f, 0xff },
	{ 0xff, 0x87, 0x00 },
	{ 0xff, 0x87, 0x5f },
	{ 0xff, 0x87, 0x87 },
	{ 0xff, 0x87, 0xaf },
	{ 0xff, 0x87, 0xd7 },
	{ 0xff, 0x87, 0xff },
	{ 0xff, 0xaf, 0x00 },
	{ 0xff, 0xaf, 0x5f },
	{ 0xff, 0xaf, 0x87 },
	{ 0xff, 0xaf, 0xaf },
	{ 0xff, 0xaf, 0xd7 },
	{ 0xff, 0xaf, 0xff },
	{ 0xff, 0xd7, 0x00 },
	{ 0xff, 0xd7, 0x5f },
	{ 0xff, 0xd7, 0x87 },
	{ 0xff, 0xd7, 0xaf },
	{ 0xff, 0xd7, 0xd7 },
	{ 0xff, 0xd7, 0xff },
	{ 0xff, 0xff, 0x00 },
	{ 0xff, 0xff, 0x5f },
	{ 0xff, 0xff, 0x87 },
	{ 0xff, 0xff, 0xaf },
	{ 0xff, 0xff, 0xd7 },
	{ 0xff, 0xff, 0xff },

	/* GRAY SCALE COLOR(0xe8-0xff) */
	{ 0x08, 0x08, 0x08 },
	{ 0x12, 0x12, 0x12 },
	{ 0x1c, 0x1c, 0x1c },
	{ 0x26, 0x26, 0x26 },
	{ 0x30, 0x30, 0x30 },
	{ 0x3a, 0x3a, 0x3a },
	{ 0x44, 0x44, 0x44 },
	{ 0x4e, 0x4e, 0x4e },
	{ 0x58, 0x58, 0x58 },
	{ 0x62, 0x62, 0x62 },
	{ 0x6c, 0x6c, 0x6c },
	{ 0x76, 0x76, 0x76 },
	{ 0x80, 0x80, 0x80 },
	{ 0x8a, 0x8a, 0x8a },
	{ 0x94, 0x94, 0x94 },
	{ 0x9e, 0x9e, 0x9e },
	{ 0xa8, 0xa8, 0xa8 },
	{ 0xb2, 0xb2, 0xb2 },
	{ 0xbc, 0xbc, 0xbc },
	{ 0xc6, 0xc6, 0xc6 },
	{ 0xd0, 0xd0, 0xd0 },
	{ 0xda, 0xda, 0xda },
	{ 0xe4, 0xe4, 0xe4 },
	{ 0xee, 0xee, 0xee },
};
#endif

static char *  color_file = "mlterm/color" ;
static KIK_MAP( color_rgb)  color_config ;
static u_int  num_of_changed_256_colors ;

static struct
{
	u_int  is_changed: 1 ;
	u_int  mark: 7 ;
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;

} *  ext_color_table ;
static u_int  ext_color_mark = 1 ;
static int  use_approximate_vt_color ;


/* --- static functions --- */

static KIK_PAIR( color_rgb)
get_color_rgb_pair(
	ml_color_t  color
	)
{
	KIK_PAIR( color_rgb)  pair ;

	kik_map_get( color_config , color , pair) ;

	return  pair ;
}

static int
color_config_set_rgb(
	ml_color_t  color ,
	u_int8_t  red ,
	u_int8_t  green ,
	u_int8_t  blue ,
	u_int8_t  alpha
	)
{
	KIK_PAIR( color_rgb)  pair ;
	rgb_t  rgb ;

	rgb.red = red ;
	rgb.green = green ;
	rgb.blue = blue ;
	rgb.alpha = alpha ;

	if( ( pair = get_color_rgb_pair( color)))
	{
		if( pair->value.red == red && pair->value.green == green &&
			pair->value.blue == blue && pair->value.alpha == alpha)
		{
			/* Not changed */

			return  0 ;
		}

		pair->value = rgb ;

		return  1 ;
	}
	else
	{
		int  result ;
		u_int8_t  r ;
		u_int8_t  g ;
		u_int8_t  b ;

		/*
		 * The same rgb as the default is rejected in 256 color.
		 * The same rgb as the default is not rejected in sys color
		 * for backward compatibility with 3.1.5 or before. (If rejected,
		 * mlterm -fg hl_white doesn't work even if hl_white is defined
		 * in ~/.mlterm/color.)
		 */

		if( IS_256_COLOR(color))
		{
			if( ! ml_get_color_rgba( color , &r , &g , &b , NULL))
			{
				return  0 ;
			}

			if( red == r && green == g && blue == b && alpha == 0xff)
			{
				/* Not changed */

			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG
					" color %d'rgb(%02x%02x%02x%02x) not changed.\n",
					color , red , green , blue , alpha) ;
			#endif

				return  0 ;
			}
		#ifdef  DEBUG
			else
			{
				kik_debug_printf( KIK_DEBUG_TAG
					" color %d's rgb(%02x%02x%02x) changed => %02x%02x%02x.\n",
					color , r , g , b , red , green , blue) ;
			}
		#endif

			num_of_changed_256_colors ++ ;
		}

		kik_map_set( result , color_config , color , rgb) ;

		return  result ;
	}
}

static int
color_config_get_rgb(
	ml_color_t  color ,
	u_int8_t *  red ,
	u_int8_t *  green ,
	u_int8_t *  blue ,
	u_int8_t *  alpha	/* can be NULL */
	)
{
	KIK_PAIR( color_rgb)  pair ;

	if( ( pair = get_color_rgb_pair( color)) == NULL)
	{
		return  0 ;
	}

	*red = pair->value.red ;
	*blue = pair->value.blue ;
	*green = pair->value.green ;
	if( alpha)
	{
		*alpha = pair->value.alpha ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %d's rgb => %d %d %d\n", color , *red, *blue, *green) ;
#endif

	return  1 ;
}

static int
parse_conf(
	char *  color_name ,
	char *  rgb
	)
{
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;
	u_int8_t  alpha ;
	ml_color_t  color ;

	/*
	 * Illegal color name is rejected.
	 */
	if( ( color = ml_get_color( color_name)) == ML_UNKNOWN_COLOR)
	{
		return  0 ;
	}

	if( *rgb == '\0')
	{
		if( ! get_color_rgb_pair( color))
		{
			return  0 ;
		}
		else
		{
			int  result ;

			if( IS_256_COLOR(color))
			{
				num_of_changed_256_colors -- ;
			}

			kik_map_erase_simple( result , color_config , color) ;

			return  1 ;
		}
	}
	else if( ! ml_color_parse_rgb_name( &red , &green , &blue , &alpha , rgb))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " illegal rgblist format (%s,%s)\n" ,
			color_name , rgb) ;
	#endif

		return  0 ;
	}

#ifdef  __DEBUG
	kik_debug_printf( "%s(%d) = red %x green %x blue %x\n" ,
		color_name , color , red , green , blue) ;
#endif

	return  color_config_set_rgb( color , red , green , blue , alpha) ;
}

static int
read_conf(
	const char *  filename
	)
{
	kik_file_t *  from ;
	char *  color_name ;
	char *  rgb ;

	if( ! ( from = kik_file_open( filename , "r")))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s couldn't be opened.\n" , filename) ;
	#endif

		return  0 ;
	}

	while( kik_conf_io_read( from , &color_name , &rgb))
	{
		parse_conf( color_name , rgb) ;
	}

	kik_file_close( from) ;

	return  1 ;
}


/* --- global functions --- */

void
ml_set_use_approximate_vt_color(
	int  use
	)
{
	use_approximate_vt_color = use ;
}

int
ml_color_config_init(void)
{
	char *  rcpath ;

	kik_map_new_with_size( ml_color_t , rgb_t , color_config ,
		kik_map_hash_int , kik_map_compare_int , 16) ;

	if( ( rcpath = kik_get_sys_rc_path( color_file)))
	{
		read_conf( rcpath) ;
		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( color_file)))
	{
		read_conf( rcpath) ;
		free( rcpath) ;
	}

	return  1 ;
}

int
ml_color_config_final(void)
{
	kik_map_delete( color_config) ;
	color_config = NULL ;

	return  1 ;
}

/*
 * Return value 0 means customization failed or not changed.
 * Return value -1 means saving failed.
 */
int
ml_customize_color_file(
	char *  color ,
	char *  rgb ,
	int  save
	)
{
	if( ! color_config || ! parse_conf( color , rgb))
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

char *
ml_get_color_name(
	ml_color_t  color
	)
{
	if( IS_VTSYS_COLOR(color))
	{
		if( color & ML_BOLD_COLOR_MASK)
		{
			return  color_name_table[color & ~ML_BOLD_COLOR_MASK] ;
		}
		else
		{
			return  color_name_table[color] + 3 ;
		}
	}
	else if( IS_256_COLOR(color))
	{
		/* XXX Not reentrant */
		
		snprintf( color_name_256, sizeof( color_name_256), "%d", color) ;

		return  color_name_256 ;
	}
	else
	{
		return  NULL ;
	}
}

ml_color_t
ml_get_color(
	const char *  name
	)
{
	ml_color_t  color ;

	if( sscanf( name, "%d", (int*) &color) == 1)
	{
		if( IS_VALID_COLOR_EXCEPT_SPECIAL_COLORS(color))
		{
			return  color ;
		}
	}

	for( color = ML_BLACK ; color <= ML_WHITE ; color++)
	{
		if( strcmp( name, color_name_table[color] + 3) == 0)
		{
			return  color ;
		}
		else if( strcmp( name, color_name_table[color]) == 0)
		{
			return  color | ML_BOLD_COLOR_MASK ;
		}
	}

	return  ML_UNKNOWN_COLOR ;
}

int
ml_get_color_rgba(
	ml_color_t  color ,
	u_int8_t *  red ,
	u_int8_t *  green ,
	u_int8_t *  blue ,
	u_int8_t *  alpha	/* can be NULL */
	)
{
	if( ! IS_VALID_COLOR_EXCEPT_SPECIAL_COLORS(color))
	{
		return  0 ;
	}

	if( IS_EXT_COLOR(color))
	{
		if( ext_color_table && ext_color_table[EXT_COLOR_TO_INDEX(color)].mark > 0)
		{
			*red = ext_color_table[EXT_COLOR_TO_INDEX(color)].red ;
			*green = ext_color_table[EXT_COLOR_TO_INDEX(color)].green ;
			*blue = ext_color_table[EXT_COLOR_TO_INDEX(color)].blue ;

			goto  end ;
		}

		return  0 ;
	}

	if( color_config && color_config_get_rgb( color , red , green , blue , alpha))
	{
		return  1 ;
	}
	else if( IS_VTSYS_COLOR(color))
	{
		*red = vtsys_color_rgb_table[ color][0] ;
		*green = vtsys_color_rgb_table[ color][1] ;
		*blue = vtsys_color_rgb_table[ color][2] ;
	}
	else if( color <= 0xe7)
	{
		u_int8_t  tmp ;

		tmp = (color - 0x10) % 6 ;
		*blue = tmp ? (tmp * 40 + 55) & 0xff : 0 ;

		tmp = ((color - 0x10) / 6) % 6 ;
		*green = tmp ? (tmp * 40 + 55) & 0xff : 0 ;

		tmp = ((color - 0x10) / 36) % 6 ;
		*red = tmp ? (tmp * 40 + 55) & 0xff : 0 ;
	}
	else /* if( color >= 0xe8) */
	{
		u_int8_t  tmp ;

		tmp = (color - 0xe8) * 10 + 8 ;

		*blue = tmp ;
		*green = tmp ;
		*red = tmp ;
	}

end:
	if( alpha)
	{
		*alpha = 0xff ;
	}
	
	return  1 ;
}

int
ml_color_parse_rgb_name(
	u_int8_t *  red ,
	u_int8_t *  green ,
	u_int8_t *  blue ,
	u_int8_t *  alpha ,
	const char *  name
	)
{
	int  r ;
	int  g ;
	int  b ;
	int  a ;
	size_t  name_len ;
	char *  format ;
	int  has_alpha ;
	int  long_color ;

#if   1
	/* Backward compatibility with mlterm-3.1.5 or before. */
	if( color_config)
	{
		/* If name is defined in ~/.mlterm/color, the defined rgb is returned. */
		ml_color_t  color ;

		if( ( color = ml_get_color( name)) != ML_UNKNOWN_COLOR &&
		    color_config_get_rgb( color , red , green , blue , alpha))
		{
			return  1 ;
		}
	}
#endif

	a = 0xffff ;
	has_alpha = 0 ;
	long_color = 0 ;

	name_len = strlen( name) ;

	if( name_len >= 14)
	{
		/*
		 * XXX
		 * "RRRR-GGGG-BBBB" length is 14, but 2.4.0 or before accepts
		 * "RRRR-GGGG-BBBB....."(trailing any characters) format and
		 * what is worse "RRRR-GGGG-BBBB;" appears in etc/color sample file.
		 * So, more than 14 length is also accepted for backward compatiblity.
		 */
		if( sscanf( name, "%4x-%4x-%4x" , &r , &g , &b) == 3)
		{
			goto  end ;
		}
		else if( name_len == 16)
		{
			format = "rgba:%2x/%2x/%2x/%2x" ;
			has_alpha = 1 ;
		}
		else if( name_len == 17)
		{
			format = "#%4x%4x%4x%4x" ;
			has_alpha = 1 ;
			long_color = 1 ;
		}
		else if( name_len == 18)
		{
			format = "rgb:%4x/%4x/%4x" ;
			long_color = 1 ;
		}
		else if( name_len == 24)
		{
			format = "rgba:%4x/%4x/%4x/%4x" ;
			long_color = 1 ;
			has_alpha = 1 ;
		}
		else
		{
			return  0 ;
		}
	}
	else
	{
		if( name_len == 7)
		{
			format = "#%2x%2x%2x" ;
		}
		else if( name_len == 9)
		{
			format = "#%2x%2x%2x%2x" ;
			has_alpha = 1 ;
		}
		else if( name_len == 12)
		{
			format = "rgb:%2x/%2x/%2x" ;
		}
		else if( name_len == 13)
		{
			format = "#%4x%4x%4x" ;
			long_color = 1 ;
		}
		else
		{
			return  0 ;
		}
	}

	if( sscanf( name , format , &r , &g , &b , &a) != (3 + has_alpha))
	{
		return  0 ;
	}
	
end:
	if( long_color)
	{
		*red = (r >> 8) & 0xff ;
		*green = (g >> 8) & 0xff ;
		*blue = (b >> 8) & 0xff ;
		*alpha = (a >> 8) & 0xff ;
	}
	else
	{
		*red = r ;
		*green = g ;
		*blue = b ;
		*alpha = a & 0xff ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s => %x %x %x %x\n" ,
		name , *red , *green , *blue , *alpha) ;
#endif

	return  1 ;
}

#if  0
#undef  COLOR_DISTANCE_THRESHOLD
#define  COLOR_DISTANCE_THRESHOLD  100
#endif

/*
 * Return the number of colors which should be searched after this function.
 */
u_int
ml_get_closest_256_color(
	ml_color_t *  closest ,
	u_int *  min_diff ,
	u_int8_t  red ,
	u_int8_t  green ,
	u_int8_t  blue
	)
{
	int  r , g , b ;
	int  tmp ;
	ml_color_t  color ;
	u_int8_t  rgb[3] ;
	u_int  diff ;
	int  diff_r , diff_g , diff_b ;
	int  count ;
	int  num ;

	if( num_of_changed_256_colors > 0)
	{
		return  256 ;
	}

	/*
	 * 0 - 47 => 0
	 * 48 - 114 => 1
	 * 115 - 154 => 2
	 * ...
	 */
	tmp = (red <= 114 ? (red >= 48) : ((red - 55) + 20) / 40) ;
	r = tmp ? (tmp * 40 + 55) & 0xff : 0 ;
	color = tmp * 36 ;
	tmp = (green <= 114 ? (green >= 48) : ((green - 55) + 20) / 40) ;
	g = tmp ? (tmp * 40 + 55) & 0xff : 0 ;
	color += tmp * 6 ;
	tmp = (blue <= 114 ? (blue >= 48) : ((blue - 55) + 20) / 40) ;
	b = tmp ? (tmp * 40 + 55) & 0xff : 0 ;
	color += tmp ;
	/* lazy color-space conversion */
	diff_r = red - r ;
	diff_g = green - g ;
	diff_b = blue - b ;
	diff = COLOR_DISTANCE(diff_r,diff_g,diff_b) ;
	if( diff < *min_diff)
	{
		*min_diff = diff ;
		*closest = color + 0x10 ;
		if( diff < COLOR_DISTANCE_THRESHOLD)
		{
			return  0 ;
		}
	}

	num = 1 ;
	rgb[0] = red ;

	if( red != green)
	{
		rgb[num++] = green ;
	}

	if( red != blue && green != blue)
	{
		rgb[num++] = blue ;
	}

	for( count = 0 ; count < num ; count++)
	{
		tmp = (rgb[count] >= 233 ?
			23 : (rgb[count] <= 12 ? 0 : ((rgb[count] - 8) + 5) / 10)) ;
		r = g = b = tmp * 10 + 8 ;
		/* lazy color-space conversion */
		diff_r = red - r ;
		diff_g = green - g ;
		diff_b = blue - b ;
		diff = COLOR_DISTANCE(diff_r,diff_g,diff_b) ;
		if( diff < *min_diff)
		{
			*min_diff = diff ;
			*closest = tmp + 0xe8 ;
			if( diff < COLOR_DISTANCE_THRESHOLD)
			{
				return  0 ;
			}
		}
	}

	return  16 ;
}

ml_color_t
ml_get_closest_color(
	u_int8_t  red ,
	u_int8_t  green ,
	u_int8_t  blue
	)
{
	ml_color_t  closest = ML_UNKNOWN_COLOR ;
	ml_color_t  color ;
	u_int  linear_search_max ;
	u_int  min = 0xffffff ;
	ml_color_t  oldest_color ;
	u_int  oldest_mark ;
	u_int  max = 0 ;

#ifdef  __DEBUG
	ml_color_t  hit_closest = ML_UNKNOWN_COLOR ;
	ml_get_closest_256_color( &hit_closest , &min , red , green , blue) ;
	min = 0xffffff ;

	for( color = 16 ; color < 256 ; color++)
#else
	if( ( linear_search_max = ml_get_closest_256_color( &closest , &min ,
					red , green , blue)) == 0)
	{
		return  closest ;
	}

	for( color = 0 ; color < linear_search_max ; color++)
#endif
	{
		u_int8_t  r ;
		u_int8_t  g ;
		u_int8_t  b ;
		u_int8_t  a ;

		if( ml_get_color_rgba( color , &r , &g , &b , &a) && a == 0xff)
		{
			u_int  diff ;
			int  diff_r , diff_g , diff_b ;

			/* lazy color-space conversion */
			diff_r = red - r ;
			diff_g = green - g ;
			diff_b = blue - b ;
			diff = COLOR_DISTANCE(diff_r,diff_g,diff_b) ;
			if( diff < min)
			{
				min = diff ;
				closest = color ;
				if( diff < COLOR_DISTANCE_THRESHOLD)
				{
				#ifdef  __DEBUG
					/*
					 * XXX
					 * The result of linear search of boundary values
					 * (115, 195 etc) is different from that of
					 * ml_get_closest_256_color() of it.
					 */
					if( closest != hit_closest)
					{
						kik_debug_printf( "ERROR %x %x %x -> %x<=>%x\n" ,
							red , green , blue ,
							closest , hit_closest) ;
					}
				#endif

					return  closest ;
				}
			}
		}
	}

#ifdef  __DEBUG
	/*
	 * XXX
	 * The result of linear search of boundary values (115, 195 etc) is
	 * different from that of ml_get_closest_256_color() of it.
	 */
	if( closest != hit_closest)
	{
		kik_debug_printf( "ERROR %x %x %x -> %x<=>%x\n" ,
			red , green , blue , closest , hit_closest) ;
	}
#endif

	if( use_approximate_vt_color ||
	    ( ! ext_color_table &&
	      ! ( ext_color_table = calloc( 240 , sizeof(*ext_color_table)))))
	{
		return  closest ;
	}

	if( ( oldest_mark = ext_color_mark / 2) == 120)
	{
		oldest_mark = 1 ;
	}
	else
	{
		oldest_mark ++ ;
	}

	if( ext_color_mark == MAX_EXT_COLORS)
	{
		ext_color_mark = 2 ;
	}
	else
	{
		ext_color_mark ++ ;
	}

	color = 0 ;
	while( ext_color_table[color].mark)
	{
		u_int  diff ;
		int  diff_r , diff_g , diff_b ;

		/* lazy color-space conversion */
		diff_r = red - ext_color_table[color].red ;
		diff_g = green - ext_color_table[color].green ;
		diff_b = blue - ext_color_table[color].blue ;
		diff = COLOR_DISTANCE(diff_r,diff_g,diff_b) ;
		if( diff < min)
		{
			min = diff ;
			if( diff < COLOR_DISTANCE_THRESHOLD)
			{
				ext_color_table[color].mark = ext_color_mark / 2 ;

			#ifdef  __DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " Use cached ext color %x\n" ,
					INDEX_TO_EXT_COLOR(color)) ;
			#endif

				return  INDEX_TO_EXT_COLOR(color) ;
			}
		}

		if( max == MAX_EXT_COLORS / 2)
		{
			/* do nothing */
		}
		else if( ext_color_table[color].mark == oldest_mark)
		{
			max = MAX_EXT_COLORS / 2 ;
			oldest_color = color ;
		}
		else
		{
			if( ext_color_table[color].mark < oldest_mark)
			{
				diff = oldest_mark - ext_color_table[color].mark ;
			}
			else /* if( ext_color_table[color].mark > oldest_mark) */
			{
				diff = oldest_mark + (MAX_EXT_COLORS / 2) -
					ext_color_table[color].mark ;
			}

			if( diff > max)
			{
				max = diff ;
				oldest_color = color ;
			}
		}

		if( ++color == MAX_EXT_COLORS)
		{
			color = oldest_color ;
			ext_color_table[color].is_changed = 1 ;

		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Delete ext color %x\n" ,
				INDEX_TO_EXT_COLOR(color)) ;
		#endif

			break ;
		}
	}

	ext_color_table[color].mark = ext_color_mark / 2 ;
	ext_color_table[color].red = red ;
	ext_color_table[color].green = green ;
	ext_color_table[color].blue = blue ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "New ext color %.2x: r%.2x g%x b%.2x\n" ,
		INDEX_TO_EXT_COLOR(color) , red , green , blue) ;
#endif

	return  INDEX_TO_EXT_COLOR(color) ;
}

int
ml_ext_color_is_changed(
	ml_color_t  color
	)
{
	if( ext_color_table[EXT_COLOR_TO_INDEX(color)].is_changed)
	{
		ext_color_table[EXT_COLOR_TO_INDEX(color)].is_changed = 0 ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}
