/*
 *	$Id$
 */

#include  "ml_color_manager.h"

#include  <string.h>		/* memset */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_str.h>	/* strdup */


/* --- global functions --- */

int
ml_color_manager_init(
	ml_color_manager_t *  color_man ,
	Display *  display ,
	int  screen ,
	ml_color_custom_t *  color_custom
	)
{
	memset( color_man , 0 , sizeof( ml_color_manager_t)) ;
	
	color_man->display = display ;
	color_man->screen = screen ;
	color_man->color_custom = color_custom ;

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
		free( color_man->xcolors[color].name) ;
		ml_unload_xcolor( color_man->display , color_man->screen ,
			&color_man->xcolors[color].xcolor) ;
	}

	return  1 ;
}

ml_color_t
ml_get_color(
	ml_color_manager_t *  color_man ,
	char *  name
	)
{
	ml_color_t  color ;

	for( color = 0 ; color < MAX_ACTUAL_COLORS ; color ++)
	{
		if( color_man->xcolors[color].name == NULL)
		{
			u_short  red ;
			u_short  green ;
			u_short  blue ;
			
			if( ml_color_custom_get_rgb( color_man->color_custom , &red , &green , &blue , name))
			{
				if( ml_load_rgb_xcolor( color_man->display , color_man->screen ,
					&color_man->xcolors[color].xcolor , red , green , blue))
				{
					color_man->xcolors[color].name = strdup( name) ;
					
					return  color ;
				}
			}

			if( ml_load_named_xcolor( color_man->display , color_man->screen ,
				&color_man->xcolors[color].xcolor , name))
			{
				color_man->xcolors[color].name = strdup( name) ;
				
				return  color ;
			}

			kik_msg_printf( " Not enough colors available. %s setting problematic.\n" , name) ;

			return  ML_UNKNOWN_COLOR ;
		}
		else if( strcmp( color_man->xcolors[color].name , name) == 0)
		{
			return  color ;
		}
	}

	kik_msg_printf( " Unable to handle more than 14 colors.\n") ;

	/*
	 * XXX
	 * Load a color by "name", then find and replace a color similar to it in
	 * color_man->xcolors.
	 */

	return  ML_UNKNOWN_COLOR ;
}

char *
ml_get_color_name(
	ml_color_manager_t *  color_man ,
	ml_color_t  color
	)
{
	if( 0 <= color && color <= MAX_ACTUAL_COLORS)
	{
		return  color_man->xcolors[color].name ;
	}
	else
	{
		return  NULL ;
	}
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
	x_color_t  xcolor ;

	if( ! ml_load_rgb_xcolor( color_man->display , color_man->screen , &xcolor , red , green , blue))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " load_rgb_xcolor(r 0x%x g 0x%x b 0x%x) failed.\n" ,
			red , green , blue) ;
	#endif
	
		return  0 ;
	}

	ml_unload_xcolor( color_man->display , color_man->screen ,
			&color_man->xcolors[color].xcolor) ;
	
	color_man->xcolors[color].xcolor = xcolor ;

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

	if( fg_color == ML_UNKNOWN_COLOR || bg_color == ML_UNKNOWN_COLOR)
	{
		return  NULL ;
	}
	
	if( ( color_table = malloc( sizeof( x_color_t *) * MAX_COLORS)) == NULL)
	{
		return  NULL ;
	}
	
	for( counter = 0 ; counter < MAX_ACTUAL_COLORS ; counter ++)
	{
		color_table[counter] = &color_man->xcolors[counter].xcolor ;
	}

	color_table[ML_FG_COLOR] = color_table[fg_color] ;
	color_table[ML_BG_COLOR] = color_table[bg_color] ;

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
