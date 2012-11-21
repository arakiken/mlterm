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


/* --- static functions --- */

static KIK_PAIR( color_rgb)
get_color_rgb_pair(
	ml_color_t  color
	)
{
	int  result ;
	KIK_PAIR( color_rgb)  pair ;

	kik_map_get( result , color_config , color , pair) ;
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
		 * Same rgb as default is rejected.
		 */

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
	kik_debug_printf( KIK_DEBUG_TAG " %s's rgb => %d %d %d\n", color , *red, *blue, *green) ;
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
	kik_debug_printf( "%s = red %x green %x blue %x\n" , color_name , red , green , blue) ;
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
		if( IS_VALID_COLOR_EXCEPT_FG_BG(color))
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
	if( ! IS_VALID_COLOR_EXCEPT_FG_BG(color))
	{
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

ml_color_t
ml_get_closest_color(
	u_int8_t  red ,
	u_int8_t  green ,
	u_int8_t  blue
	)
{
	ml_color_t  closest = ML_UNKNOWN_COLOR ;
	ml_color_t  color ;
	u_long  min = 0xffffff ;

	for( color = 0 ; color < 256 ; color++)
	{
		u_int8_t  r ;
		u_int8_t  g ;
		u_int8_t  b ;
		u_int8_t  a ;

		/* 16 is treated as 0 and 231 is treated as 15 internally in ml_char.c. */
		if( color != 16 && color != 231 &&
		    ml_get_color_rgba( color , &r , &g , &b , &a) && a == 0xff)
		{
			u_long  diff ;
			int  diff_r , diff_g , diff_b ;

			/* lazy color-space conversion */
			diff_r = red - r ;
			diff_g = green - g ;
			diff_b = blue - b ;
			diff = diff_r * diff_r * 9 + diff_g * diff_g * 30 +
				diff_b * diff_b ;
			if( diff < min)
			{
				min = diff ;
				closest = color ;
				/* no one may notice the difference */
				if( diff < 31)
				{
					break ;
				}
			}
		}
	}

	return  closest ;
}
