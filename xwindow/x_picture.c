/*
 *	$Id$
 */

#include  "x_picture.h"


#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_debug.h>

#include  "x_imagelib.h"


/* --- static varaibles --- */

static x_icon_picture_t **  icon_pics ;
static u_int  num_of_icon_pics ;


/* --- static functions --- */

static x_icon_picture_t *
create_icon_picture(
	Display *  display ,
	char *  file_path	/* Don't specify NULL. */
	)
{
	int  icon_size = 48 ;
	x_icon_picture_t *  pic ;

	if( ( pic = malloc( sizeof( x_icon_picture_t))) == NULL)
	{
		return  NULL ;
	}

	if( ( pic->file_path = strdup( file_path)) == NULL)
	{
		free( pic->file_path) ;
		
		return  NULL ;
	}
	
	if( ! x_imagelib_load_file( display , file_path ,
		&(pic->cardinal) , &(pic->pixmap) , &(pic->mask) , &icon_size , &icon_size))
	{
		free( pic->file_path) ;
		free( pic) ;

		kik_error_printf( " Failed to load icon file(%s).\n" , file_path) ;
		
		return  NULL ;
	}

	pic->display = display ;
	pic->ref_count = 1 ;

#if  0
	kik_debug_printf( KIK_DEBUG_TAG " Successfully loaded icon file %s.\n" , file_path) ;
#endif

	return  pic ;
}

static int
delete_icon_picture(
	x_icon_picture_t *  pic
	)
{
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s icon will be deleted.\n" , pic->file_path) ;
#endif

	if( pic->pixmap)
	{
	#ifndef  USE_WIN32GUI
		XFreePixmap( pic->display , pic->pixmap) ;
	#endif
	}

	if( pic->mask)
	{
	#ifndef  USE_WIN32GUI
		XFreePixmap( pic->display , pic->mask) ;
	#endif
	}

	free( pic->cardinal) ;
	free( pic->file_path) ;

	free( pic) ;

	return  1 ;
}


/* --- global functions --- */

int
x_picture_display_opened(
	Display *  display
	)
{
	if( ! x_imagelib_display_opened( display))
	{
		return  0 ;
	}

#ifndef  USE_WIN32GUI
	/* Want _XROOTPIAMP_ID changed events. */
	XSelectInput( display , DefaultRootWindow(display) , PropertyChangeMask) ;
#endif

	return  1 ;
}

int
x_picture_display_closed(
	Display *  display
	)
{
	if( num_of_icon_pics > 0)
	{
		int  count ;

		for( count = num_of_icon_pics - 1 ; count >= 0 ; count--)
		{
			if( icon_pics[count]->display == display)
			{
				delete_icon_picture( icon_pics[count]) ;
				icon_pics[count] = icon_pics[--num_of_icon_pics] ;
			}
		}

		if( num_of_icon_pics == 0)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " All cached icons were free'ed\n") ;
		#endif
		
			free( icon_pics) ;
			icon_pics = NULL ;
		}
	}
	
	return  x_imagelib_display_closed( display) ;
}

int
x_picture_modifier_is_normal(
	x_picture_modifier_t *  pic_mod
	)
{
	if( pic_mod->brightness == 100 && pic_mod->contrast == 100 && pic_mod->gamma == 100
		&& pic_mod->alpha == 255)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
x_root_pixmap_available(
	Display *  display
	)
{
	return  x_imagelib_root_pixmap_available( display) ;
}

int
x_bg_picture_init(
	x_bg_picture_t *  pic ,
	x_window_t *  win ,
	x_picture_modifier_t *  mod
	)
{
	pic->win = win ;
	pic->mod = mod ;

	pic->pixmap = None ;

	return  1 ;
}

int
x_bg_picture_final(
	x_bg_picture_t *  pic
	)
{
	if( pic->pixmap)
	{
	#ifndef  USE_WIN32GUI
		XFreePixmap(  pic->win->disp->display , pic->pixmap) ;
	#endif
	}

	return  1 ;
}

int
x_bg_picture_load_file(
	x_bg_picture_t *  pic ,
	char *  file_path
	)
{
	if( pic->pixmap)
	{
		/* already loaded */
		
		return  0 ;
	}
	pic->pixmap = x_imagelib_load_file_for_background( pic->win , file_path , pic->mod) ;
	if( pic->pixmap == None)
	{
		return  0 ;
	}
	
	return  1 ;
}

int
x_bg_picture_get_transparency(
	x_bg_picture_t *  pic
	)
{
	if( pic->pixmap)
	{
		/* already loaded */
		
		return  0 ;
	}

	if( ( pic->pixmap = x_imagelib_get_transparent_background( pic->win , pic->mod)) == None)
	{
		return  0 ;
	}

	return  1 ;
}

x_icon_picture_t *
x_acquire_icon_picture(
	Display *  display ,
	char *  file_path	/* Don't specify NULL. */
	)
{
	u_int  count ;
	x_icon_picture_t **  p ;

	for( count = 0 ; count < num_of_icon_pics ; count++)
	{
		if( strcmp( file_path , icon_pics[count]->file_path) == 0 &&
			display == icon_pics[count]->display)
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Use cached icon(%s).\n" , file_path) ;
		#endif
			icon_pics[count]->ref_count ++ ;
			
			return  icon_pics[count] ;
		}
	}

	if( ( p = realloc( icon_pics , ( num_of_icon_pics + 1) * sizeof( *icon_pics))) == NULL)
	{
		return  NULL ;
	}

	if( ( p[num_of_icon_pics] = create_icon_picture( display , file_path)) == NULL)
	{
		if( num_of_icon_pics == 0)
		{
			free( p) ;
		}
		
		return  NULL ;
	}

	icon_pics = p ;

	return  icon_pics[num_of_icon_pics++] ;
}

int
x_release_icon_picture(
	x_icon_picture_t *  pic
	)
{
	u_int  count ;
	
	for( count = 0 ; count < num_of_icon_pics ; count++)
	{
		if( pic == icon_pics[count])
		{
			if( -- (pic->ref_count) == 0)
			{
				delete_icon_picture( pic) ;
				
				if( --num_of_icon_pics == 0)
				{
				#ifdef  DEBUG
					kik_debug_printf( KIK_DEBUG_TAG
						" All cached icons were free'ed\n") ;
				#endif

					free( icon_pics) ;
					icon_pics = NULL ;
				}
				else
				{
					icon_pics[count] = icon_pics[num_of_icon_pics] ;
				}
			}

			return  1 ;
		}
	}

	return  0 ;
}
