/*
 *	$Id$
 */

#include  "ml_color_manager.h"

#include  <string.h>		/* memset */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_conf_io.h>


/* --- static functions --- */

static int
load_named_xcolor(
	ml_color_manager_t *  color_man ,
	ml_xcolor_t *  xcolor ,
	char *  name
	)
{
	if( xcolor->is_loaded)
	{
		return  0 ;
	}
	
	if( ml_load_named_xcolor( color_man->display , color_man->screen , &xcolor->xcolor , name))
	{
		xcolor->is_loaded = 1 ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

static int
load_rgb_xcolor(
	ml_color_manager_t *  color_man ,
	ml_xcolor_t *  xcolor ,
	u_short  red ,
	u_short  green ,
	u_short  blue
	)
{
	if( xcolor->is_loaded)
	{
		return  0 ;
	}
	
	if( ml_load_rgb_xcolor( color_man->display , color_man->screen , &xcolor->xcolor ,
		red , green , blue))
	{
		xcolor->is_loaded = 1 ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

static int
unload_xcolor(
	ml_color_manager_t *  color_man ,
	ml_xcolor_t *  xcolor
	)
{
	if( ! xcolor->is_loaded)
	{
		return  0 ;
	}
	
	if( ml_unload_xcolor( color_man->display , color_man->screen , &xcolor->xcolor))
	{
		xcolor->is_loaded = 0 ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}


/* --- global functions --- */

int
ml_color_manager_init(
	ml_color_manager_t *  color_man ,
	Display *  display ,
	int  screen
	)
{
	memset( color_man , 0 , sizeof( ml_color_manager_t)) ;
	
	color_man->display = display ;
	color_man->screen = screen ;

	return  1 ;
}

int
ml_color_manager_final(
	ml_color_manager_t *  color_man
	)
{
	ml_color_t  color ;

	for( color = 0 ; color < MAX_ACTUAL_COLORS ; color ++)
	{
		free( color_man->rgbs[color]) ;
	}
	
	for( color = 0 ; color < MAX_ACTUAL_COLORS ; color ++)
	{
		unload_xcolor( color_man , &color_man->xcolors[color]) ;
	}

	return  1 ;
}

int
ml_color_manager_set_rgb(
	ml_color_manager_t *  color_man ,
	ml_color_t  color ,
	u_short  red ,
	u_short  green ,
	u_short  blue
	)
{
	ml_rgb_t *  rgb ;

	if( color_man->rgbs[color])
	{
		free( color_man->rgbs[color]) ;
		color_man->rgbs[color] = NULL ;
	}
	
	if( ( rgb = malloc( sizeof( ml_rgb_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  0 ;
	}

	rgb->red = red ;
	rgb->green = green ;
	rgb->blue = blue ;

	color_man->rgbs[color] = rgb ;

	return  1 ;
}

int
ml_color_manager_read_conf(
	ml_color_manager_t *  color_man ,
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
			ml_color_manager_set_rgb( color_man , color , red , green , blue) ;
		}
	}

	kik_file_close( from) ;

	return  1 ;
}

int
ml_color_manager_load(
	ml_color_manager_t *  color_man
	)
{
	ml_color_t  color ;
	char *  name ;

	for( color = 0 ; color < MAX_ACTUAL_COLORS ; color ++)
	{
		if( color_man->rgbs[color])
		{
			if( load_rgb_xcolor( color_man , &color_man->xcolors[color] ,
				color_man->rgbs[color]->red , color_man->rgbs[color]->green ,
				color_man->rgbs[color]->blue))
			{
				continue ;
			}
			
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " load_rgb_xfont failed.\n") ;
		#endif
		}
		
		if( ( name = ml_get_color_name( color)))
		{
			if( ! load_named_xcolor( color_man , &color_man->xcolors[color] , name))
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" %s , a basic color , couldn't be loaded.\n" ,
					name) ;
			#endif

				/* color manager cannot go ahead any more ... */
				
				return  0 ;
			}
		}
		else
		{
			/*
			 * white color is used as default value.
			 */
			if( color == MLC_PRIVATE_FG_COLOR)
			{
				color_man->xcolors[color].xcolor =
					color_man->xcolors[MLC_WHITE].xcolor ;
			}
			else if( color == MLC_PRIVATE_BG_COLOR)
			{
				color_man->xcolors[color].xcolor =
					color_man->xcolors[MLC_WHITE].xcolor ;
			}

			/*
			 * this is not a color actually loaded , but only a copy.
			 */
			color_man->xcolors[color].is_loaded = 0 ;
		}
	}

	return  1 ;
}

int
ml_color_manager_change_rgb(
	ml_color_manager_t *  color_man ,
	ml_color_t  color ,
	u_short  red ,
	u_short  green ,
	u_short  blue
	)
{
	ml_xcolor_t  xcolor ;

	xcolor.is_loaded = 0 ;

	if( ! load_rgb_xcolor( color_man , &xcolor , red , green , blue))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " load_rgb_xcolor(r 0x%x g 0x%x b 0x%x) failed.\n" ,
			red , green , blue) ;
	#endif
	
		return  0 ;
	}

	unload_xcolor( color_man , &color_man->xcolors[color]) ;
	
	color_man->xcolors[color] = xcolor ;

	return  1 ;
}

ml_color_table_t 
ml_color_table_new(
	ml_color_manager_t *  color_man ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color
	)
{
	int  counter ;
	ml_color_table_t  color_table ;

	if( ( color_table = malloc( sizeof( x_color_t *) * MAX_COLORS)) == NULL)
	{
		return  NULL ;
	}
	
	for( counter = 0 ; counter < MAX_ACTUAL_COLORS ; counter ++)
	{
		color_table[counter] = &color_man->xcolors[counter].xcolor ;
	}

	if( fg_color == MLC_UNKNOWN_COLOR)
	{
		color_table[MLC_FG_COLOR] = NULL ;
	}
	else
	{
		color_table[MLC_FG_COLOR] = color_table[fg_color] ;
	}

	if( bg_color == MLC_UNKNOWN_COLOR)
	{
		color_table[MLC_BG_COLOR] = NULL ;
	}
	else
	{
		color_table[MLC_BG_COLOR] = color_table[bg_color] ;
	}

	return  color_table ;
}

int
ml_get_color_rgb(
	ml_color_manager_t *  color_man ,
	u_short *  red ,
	u_short *  green ,
	u_short *  blue ,
	ml_color_t  color
	)
{
	return  ml_get_xcolor_rgb( red , green , blue , &color_man->xcolors[color].xcolor) ;
}
