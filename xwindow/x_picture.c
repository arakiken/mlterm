/*
 *	$Id$
 */

#include  "x_picture.h"


#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_util.h>	/* K_MIN */

#include  "x_imagelib.h"


#if  0
#define  __DEBUG
#endif


/* --- static varaibles --- */

static x_picture_t **  pics ;
static u_int  num_of_pics ;
static x_icon_picture_t **  icon_pics ;
static u_int  num_of_icon_pics ;


/* --- static functions --- */

static x_picture_t *
create_picture_intern(
	Display *  display ,
	x_picture_modifier_t *  mod ,
	char *  file_path ,
	u_int  width ,
	u_int  height
	)
{
	x_picture_t *  pic ;

	if( ( pic = malloc( sizeof( x_picture_t))) == NULL)
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

	pic->display = display ;
	pic->width = width ;
	pic->height = height ;

	return  pic ;

error2:
	free( pic->mod) ;
error1:
	free( pic) ;

	return  NULL ;
}

static int
delete_picture_intern(
	x_picture_t *  pic
	)
{
	free( pic->file_path) ;
	free( pic->mod) ;
	free( pic) ;

	return  1 ;
}

static x_picture_t *
create_bg_picture(
	x_window_t *  win ,
	x_picture_modifier_t *  mod ,
	char *  file_path
	)
{
	x_picture_t *  pic ;

	if( ! ( pic = create_picture_intern( win->disp->display , mod , file_path ,
			ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win))))
	{
		return  NULL ;
	}

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
		delete_picture_intern( pic) ;

		return  NULL ;
	}

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " New pixmap %ul is created.\n" , pic->pixmap) ;
#endif

	pic->ref_count = 1 ;

	return  pic ;
}

#ifdef  ENABLE_SIXEL
static x_picture_t *
create_picture(
	x_display_t *  disp ,
	x_picture_modifier_t *  mod ,
	char *  file_path ,
	u_int  width ,
	u_int  height
	)
{
	x_picture_t *  pic ;

	if( ! ( pic = create_picture_intern( disp->display , mod , file_path , width , height)))
	{
		return  NULL ;
	}

	if( ! x_imagelib_load_file( disp , file_path ,
		NULL , &(pic->pixmap) , NULL , &(pic->width) , &(pic->height)))
	{
		delete_picture_intern( pic) ;

		return  NULL ;
	}

	pic->ref_count = 1 ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " New pixmap %ul is created.\n" , pic->pixmap) ;
#endif

	return  pic ;
}
#endif	/* ENABLE_SIXEL */

static x_picture_t *
acquire_picture(
	x_window_t *  win ,	/* acquire bg picture => not-NULL */
	x_display_t *  disp ,
	x_picture_modifier_t *  mod ,
	char *  file_path ,
	u_int  width ,
	u_int  height
	)
{
	u_int  count ;
	x_picture_t **  p ;

	if( ! win || strcmp( file_path , "root") != 0) /* Transparent background is not cached. */
	{
		for( count = 0 ; count < num_of_pics ; count++)
		{
			if( strcmp( file_path , pics[count]->file_path) == 0 &&
			    disp->display == pics[count]->display &&
			    x_picture_modifiers_equal( mod , pics[count]->mod) &&
			    width == pics[count]->width &&
			    height == pics[count]->height)
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " Use cached picture(%s).\n" ,
					file_path) ;
			#endif
				pics[count]->ref_count ++ ;

				return  pics[count] ;
			}
		}
	}

	if( ( p = realloc( pics , ( num_of_pics + 1) * sizeof( *pics))) == NULL)
	{
		return  NULL ;
	}

	pics = p ;

	if( win)
	{
		pics[num_of_pics] = create_bg_picture( win , mod , file_path) ;
	}
#ifdef  ENABLE_SIXEL
	else
	{
		pics[num_of_pics] = create_picture( disp , NULL , file_path , width , height) ;
	}
#endif	/* ENABLE_SIXEL */

	if( ! pics[num_of_pics])
	{
		if( num_of_pics == 0 /* pics == NULL */)
		{
			free( pics) ;
			pics = NULL ;
		}

		return  NULL ;
	}

	return  pics[num_of_pics++] ;
}

static int
delete_picture(
	x_picture_t *  pic
	)
{
	/* XXX Pixmap of "pixmap:<ID>" is managed by others, so don't free here. */
	if( strncmp( pic->file_path , "pixmap:" , 7) != 0)
	{
		x_delete_image( pic->display , pic->pixmap) ;
	}

	delete_picture_intern( pic) ;

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

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Failed to load icon file(%s).\n" , file_path) ;
	#endif

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
			(a->alpha == 0))
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

x_picture_t *
x_acquire_bg_picture(
	x_window_t *  win ,
	x_picture_modifier_t *  mod ,
	char *  file_path		/* "root" means transparency. */
	)
{
	return  acquire_picture( win , win->disp , mod , file_path , 0 , 0) ;
}

#ifdef  ENABLE_SIXEL
x_picture_t *
x_acquire_picture(
	x_display_t *  disp ,
	char *  file_path ,
	u_int  width ,
	u_int  height
	)
{
	return  acquire_picture( NULL , disp , NULL , file_path , width , height) ;
}
#endif	/* ENABLE_SIXEL */

int
x_release_picture(
	x_picture_t *  pic
	)
{
	u_int  count ;
	
	for( count = 0 ; count < num_of_pics ; count++)
	{
		if( pic == pics[count])
		{
			if( -- (pic->ref_count) == 0)
			{
				delete_picture( pic) ;
				
				if( --num_of_pics == 0)
				{
				#ifdef  DEBUG
					kik_debug_printf( KIK_DEBUG_TAG
						" All cached bg pictures were free'ed\n") ;
				#endif

					free( pics) ;
					pics = NULL ;
				}
				else
				{
					pics[count] = pics[num_of_pics] ;
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

	icon_pics = p ;

	if( ( icon_pics[num_of_icon_pics] = create_icon_picture( disp , file_path)) == NULL)
	{
		if( num_of_icon_pics == 0 /* icon_pics == NULL */)
		{
			free( icon_pics) ;
			icon_pics = NULL ;
		}
		
		return  NULL ;
	}

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

#ifdef  ENABLE_SIXEL
x_picture_manager_t *
x_picture_manager_new(void)
{
	return  calloc( 1 , sizeof( x_picture_manager_t)) ;
}

int
x_picture_manager_delete(
	x_picture_manager_t *  pic_man
	)
{
	u_int  count ;

	for( count = 0 ; count < pic_man->num_of_pics ; count++)
	{
		x_release_picture( pic_man->pics[count].pic) ;
	}

	free( pic_man->pics) ;
	free( pic_man) ;

	return  1 ;
}

int
x_picture_manager_add(
	x_picture_manager_t *  pic_man ,
	x_picture_t *  pic ,
	int  x ,
	int  y
	)
{
	int  count ;
	void *  p ;

	for( count = pic_man->num_of_pics - 1 ; count >= 0 ; count--)
	{
		if( x <= pic_man->pics[count].x &&
		    pic_man->pics[count].x + pic_man->pics[count].pic->width <= x + pic->width &&
		    y <= pic_man->pics[count].y &&
		    pic_man->pics[count].y + pic_man->pics[count].pic->height <= y + pic->height)
		{
			/* Remove overlapped pictures. */

			x_release_picture( pic_man->pics[count].pic) ;

			if( -- pic_man->num_of_pics == 0)
			{
				break ;
			}

			pic_man->pics[count] = pic_man->pics[pic_man->num_of_pics] ;
		}
	}

	if( ! ( p = realloc( pic_man->pics ,
			sizeof(*pic_man->pics) * (pic_man->num_of_pics + 1))))
	{
		return  0 ;
	}

	pic_man->pics = p ;
	pic_man->pics[pic_man->num_of_pics].x = x ;
	pic_man->pics[pic_man->num_of_pics].y = y ;
	pic_man->pics[pic_man->num_of_pics].pic = pic ;

	pic_man->num_of_pics ++ ;

	return  1 ;
}

int
x_picture_manager_remove(
	x_picture_manager_t *  pic_man ,
	int  x ,
	int  y ,
	int  width ,
	int  height
	)
{
	int  count ;

	for( count = pic_man->num_of_pics - 1 ; count >= 0 ; count--)
	{
		if( x <= pic_man->pics[count].x + pic_man->pics[count].pic->width &&
		    pic_man->pics[count].x <= x + width &&
		    y <= pic_man->pics[count].y + pic_man->pics[count].pic->height &&
		    pic_man->pics[count].y <= y + height)
		{
			x_release_picture( pic_man->pics[count].pic) ;

			if( -- pic_man->num_of_pics == 0)
			{
				return  1 ;
			}

			pic_man->pics[count] = pic_man->pics[pic_man->num_of_pics] ;
		}
	}

	return  1 ;
}

int
x_picture_manager_redraw(
	x_picture_manager_t *  pic_man ,
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
	u_int  count ;

	for( count = 0 ; count < pic_man->num_of_pics ; count++)
	{
		int  dst_x ;
		int  dst_y ;
		int  src_x ;
		int  src_y ;
		u_int  src_width ;
		u_int  src_height ;

		if( x > pic_man->pics[count].x)
		{
			dst_x = x ;
			src_x = x - pic_man->pics[count].x ;
			if( src_x >= pic_man->pics[count].pic->width)
			{
				continue ;
			}
			src_width = K_MIN(width,pic_man->pics[count].pic->width - src_x) ;
		}
		else
		{
			dst_x = pic_man->pics[count].x ;
			src_x = 0 ;
			src_width = K_MIN(width + x - dst_x,pic_man->pics[count].pic->width) ;
		}

		if( y > pic_man->pics[count].y)
		{
			dst_y = y ;
			src_y = y - pic_man->pics[count].y ;
			if( src_y >= pic_man->pics[count].pic->height)
			{
				continue ;
			}
			src_height = K_MIN(height,pic_man->pics[count].pic->height - src_y) ;
		}
		else
		{
			dst_y = pic_man->pics[count].y ;
			src_y = 0 ;
			src_height = K_MIN(height + y - dst_y,pic_man->pics[count].pic->height) ;
		}

		x_window_copy_area( win , pic_man->pics[count].pic->pixmap ,
			src_x , src_y , src_width , src_height , dst_x , dst_y) ;
	}

	return  1 ;
}

int
x_picture_manager_scroll(
	x_picture_manager_t *  pic_man ,
	int  beg_y ,
	int  end_y ,
	int  height	/* < 0 => upward, > 0 => downward */
	)
{
	int  count ;

	if( pic_man->num_of_pics == 0)
	{
		return  1 ;
	}

	for( count = pic_man->num_of_pics - 1 ; count >= 0 ; count--)
	{
		if( beg_y < pic_man->pics[count].y + pic_man->pics[count].pic->height &&
		    pic_man->pics[count].y < end_y)
		{
			pic_man->pics[count].y += height ;
			if( pic_man->pics[count].y + (int)pic_man->pics[count].pic->height <= 0)
			{
				x_release_picture( pic_man->pics[count].pic) ;

				if( -- pic_man->num_of_pics == 0)
				{
					return  1 ;
				}

				pic_man->pics[count] = pic_man->pics[pic_man->num_of_pics] ;
			}
		}
	}

	return  1 ;
}
#endif  /* ENABLE_SIXEL */
