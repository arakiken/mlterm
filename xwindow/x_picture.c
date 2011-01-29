/*
 *	$Id$
 */

#include  "x_picture.h"


#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_util.h>	/* K_MIN */

#include  "x_imagelib.h"


/* --- static varaibles --- */

static x_bg_picture_t **  bg_pics ;
static u_int  num_of_bg_pics ;
static x_icon_picture_t **  icon_pics ;
static u_int  num_of_icon_pics ;


/* --- static functions --- */

static x_bg_picture_t *
create_bg_picture(
	x_window_t *  win ,
	x_picture_modifier_t *  mod ,
	char *  file_path
	)
{
	x_bg_picture_t *  pic ;
	
	if( ( pic = malloc( sizeof( x_bg_picture_t))) == NULL)
	{
		return  NULL ;
	}

	if( mod)
	{
		if( ( pic->mod = malloc( sizeof( x_picture_modifier_t))) == NULL)
		{
			goto  error1 ;
		}
		
		*pic->mod = *mod ;
	}
	else
	{
		pic->mod = NULL ;
	}

	if( ( pic->file_path = strdup( file_path)) == NULL)
	{
		goto  error2 ;
	}

	pic->display = win->disp->display ;
	pic->width = ACTUAL_WIDTH(win) ;
	pic->height = ACTUAL_HEIGHT(win) ;

	if( strcmp( file_path , "root") == 0)
	{
		pic->pixmap = x_imagelib_get_transparent_background( win , mod) ;
	}
	else
	{
		pic->pixmap = x_imagelib_load_file_for_background( win , file_path , mod) ;
	}

	if( pic->pixmap == None)
	{
		goto  error3 ;
	}

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " New pixmap is created\n") ;
#endif

	pic->ref_count = 1 ;

	return  pic ;

error3:
	free( pic->file_path) ;
error2:
	free( pic->mod) ;
error1:
	free( pic) ;

	return  NULL ;
}

static int
delete_bg_picture(
	x_bg_picture_t *  pic
	)
{
	/* XXX Pixmap of "pixmap:<ID>" is managed by others, so don't free here. */
	if( strncmp( pic->file_path , "pixmap:" , K_MIN(strlen(pic->file_path),7)) != 0)
	{
		x_delete_image( pic->display , pic->pixmap) ;
	}

	free( pic->file_path) ;
	free( pic->mod) ;
	free( pic) ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " pixmap is deleted.\n") ;
#endif

	return  1 ;
}

static x_icon_picture_t *
create_icon_picture(
	x_display_t *  disp ,
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
	
	if( ! x_imagelib_load_file( disp , file_path ,
		&(pic->cardinal) , &(pic->pixmap) , &(pic->mask) , &icon_size , &icon_size))
	{
		free( pic->file_path) ;
		free( pic) ;

		kik_error_printf( " Failed to load icon file(%s).\n" , file_path) ;
		
		return  NULL ;
	}

	pic->disp = disp ;
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

	x_delete_image( pic->disp->display , pic->pixmap) ;

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
			if( icon_pics[count]->disp->display == display)
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

/*
 * Judge whether pic_mods are equal or not.
 * \param a,b picture modifier
 * \return  1 when they are same. 0 when not.
 */
int
x_picture_modifiers_equal(
	x_picture_modifier_t *  a ,	/* Can be NULL (which means normal pic_mod) */
	x_picture_modifier_t *  b	/* Can be NULL (which means normal pic_mod) */
	)
{
	if( a == b)
	{
		/* If a==NULL and b==NULL, return 1 */
		return  1 ;
	}

	if( a == NULL)
	{
		a = b ;
		b = NULL ;
	}

	if( b == NULL)
	{
		/* Check if 'a' is normal or not. */

		if( (a->brightness == 100) &&
			(a->contrast == 100) &&
			(a->gamma == 100) &&
			(a->alpha == 255))
		{
			return  1 ;
		}
	}
	else
	{
		if( (a->brightness == b->brightness) &&
			(a->contrast == b->contrast) &&
			(a->gamma == b->gamma) &&
			(a->alpha == b->alpha) &&
			(a->blend_red == b->blend_red) &&
			(a->blend_green == b->blend_green) &&
			(a->blend_blue == b->blend_blue))
		{
			return  1 ;
		}
	}
	
	return  0 ;
}

int
x_root_pixmap_available(
	Display *  display
	)
{
	return  x_imagelib_root_pixmap_available( display) ;
}

x_bg_picture_t *
x_acquire_bg_picture(
	x_window_t *  win ,
	x_picture_modifier_t *  mod ,
	char *  file_path		/* "root" means transparency. */
	)
{
	x_bg_picture_t **  p ;

	if( strcmp( file_path , "root") != 0)	/* Transparent background is not cached. */
	{
		u_int  count ;
		
		for( count = 0 ; count < num_of_bg_pics ; count++)
		{
			if( strcmp( file_path , bg_pics[count]->file_path) == 0 &&
				win->disp->display == bg_pics[count]->display &&
				x_picture_modifiers_equal( mod , bg_pics[count]->mod) &&
				ACTUAL_WIDTH(win) == bg_pics[count]->width &&
				ACTUAL_HEIGHT(win) == bg_pics[count]->height )
			{
			#ifdef  __DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " Use cached bg picture(%s).\n" ,
					file_path) ;
			#endif
				bg_pics[count]->ref_count ++ ;

				return  bg_pics[count] ;
			}
		}
	}

	if( ( p = realloc( bg_pics , ( num_of_bg_pics + 1) * sizeof( *bg_pics))) == NULL)
	{
		return  NULL ;
	}

	if( ( p[num_of_bg_pics] = create_bg_picture( win , mod , file_path)) == NULL)
	{
		if( num_of_bg_pics == 0 /* bg_pics == NULL */)
		{
			free( p) ;
		}
		
		return  NULL ;
	}

	bg_pics = p ;

	return  bg_pics[num_of_bg_pics++] ;
}

int
x_release_bg_picture(
	x_bg_picture_t *  pic
	)
{
	u_int  count ;
	
	for( count = 0 ; count < num_of_bg_pics ; count++)
	{
		if( pic == bg_pics[count])
		{
			if( -- (pic->ref_count) == 0)
			{
				delete_bg_picture( pic) ;
				
				if( --num_of_bg_pics == 0)
				{
				#ifdef  DEBUG
					kik_debug_printf( KIK_DEBUG_TAG
						" All cached bg pictures were free'ed\n") ;
				#endif

					free( bg_pics) ;
					bg_pics = NULL ;
				}
				else
				{
					bg_pics[count] = bg_pics[num_of_bg_pics] ;
				}
			}

			return  1 ;
		}
	}

	return  0 ;
}

x_icon_picture_t *
x_acquire_icon_picture(
	x_display_t *  disp ,
	char *  file_path	/* Don't specify NULL. */
	)
{
	u_int  count ;
	x_icon_picture_t **  p ;

	for( count = 0 ; count < num_of_icon_pics ; count++)
	{
		if( strcmp( file_path , icon_pics[count]->file_path) == 0 &&
			disp == icon_pics[count]->disp)
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

	if( ( p[num_of_icon_pics] = create_icon_picture( disp , file_path)) == NULL)
	{
		if( num_of_icon_pics == 0 /* icon_pics == NULL */)
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
