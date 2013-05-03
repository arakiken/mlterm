/*
 *	$Id$
 */

#include  "../x_window.h"

#include  <string.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>

#include  "x_display.h"
#include  "x_font.h"


#define  MAX_CLICK  3			/* max is triple click */

/* win->width is not multiples of (win)->width_inc in framebuffer. */
#define  RIGHT_MARGIN(win) \
	((win)->width_inc ? ((win)->width - (win)->min_width) % (win)->width_inc : 0)
#define  BOTTOM_MARGIN(win) \
	((win)->height_inc ? ((win)->height - (win)->min_height) % (win)->height_inc : 0)


/* --- static variables --- */

static int  click_interval = 250 ;	/* millisecond, same as xterm. */


/* --- static functions --- */

static int
scroll_region(
	x_window_t *  win ,
	int  src_x ,
	int  src_y ,
	u_int  width ,
	u_int  height ,
	int  dst_x ,
	int  dst_y
	)
{
	if( ! win->is_mapped || ! x_window_is_scrollable( win))
	{
		return  0 ;
	}

	x_display_copy_lines( win->disp->display ,
			src_x + win->x + win->margin ,
			src_y + win->y + win->margin ,
			dst_x + win->x + win->margin ,
			dst_y + win->y + win->margin ,
			width , height) ;

	return  1 ;
}

static int
draw_string(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	x_color_t *  bg_color ,
	int  x ,
	int  y ,
	u_char *  str ,	/* 'len * ch_len' bytes */
	u_int  len ,
	u_int  ch_len
	)
{
	u_char *  src ;
	u_char *  p ;
	u_char **  bitmaps ;
	size_t  size ;
	u_int  font_height ;
	u_int  font_ascent ;
	int  orig_x ;
	int  y_off ;
	u_int  bpp ;
	u_int  count ;

	if( ! win->is_mapped)
	{
		return  0 ;
	}

	bpp = win->disp->display->bytes_per_pixel ;

	if( ! ( p = src = alloca( ( size = len * font->width * bpp))))
	{
		return  0 ;
	}

	if( ! ( bitmaps = alloca( ( len * sizeof(*bitmaps)))))
	{
		return  0 ;
	}

	for( count = 0 ; count < len ; count++)
	{
		bitmaps[count] = x_get_bitmap( font->xfont , str + count * ch_len , ch_len) ;
	}

	/*
	 * Check if font->height or font->ascent excesses the display height,
	 * because font->height doesn't necessarily equals to the height of the US-ASCII font.
	 *
	 * XXX
	 * On the other hand, font->width is always the same (or exactly double) for now.
	 */

	font_ascent = font->ascent ;
	font_height = font->height ;

	if( y >= win->height)
	{
		font_ascent -= (y - win->height + 1) ;
		font_height -= (y - win->height + 1) ;
	}

	if( y + font_height - font_ascent > win->height)
	{
		font_height = win->height - y + font_ascent ;
	}

	if( y < font_ascent)
	{
		y_off = font_ascent - y ;
	}
	else
	{
		y_off = 0 ;
	}

	orig_x = (x += (win->margin + win->x)) ;
	y += (win->margin + win->y) ;

	for( ; y_off < font_height ; y_off++)
	{
		for( count = 0 ; count < len ; count++)
		{
			int  force_fg ;
			int  x_off ;
			u_char *  bitmap_line ;

			force_fg = 0 ;

			bitmap_line = x_get_bitmap_line( font->xfont , bitmaps[count] , y_off) ;

			for( x_off = 0 ; x_off < font->width ; x_off++)
			{
				u_long  pixel ;

				if( bitmap_line && font->x_off <= x_off &&
				    x_get_bitmap_cell( font->xfont , bitmap_line ,
					x_off - font->x_off) )
				{
					pixel = fg_color->pixel ;

					force_fg = font->is_double_drawing ;
				}
				else
				{
					if( force_fg)
					{
						pixel = fg_color->pixel ;
					}
					else if( bg_color)
					{
						pixel = bg_color->pixel ;
					}
					else
					{
						pixel = x_display_get_pixel(
								win->disp->display ,
								x + x_off ,
								y + y_off - font_ascent) ;
					}

					force_fg = 0 ;
				}

				switch( bpp)
				{
				case 1:
					*p = pixel ;
					break ;
				case 2:
					*((u_int16_t*)p) = pixel ;
					break ;
				/* case 4: */
				default:
					*((u_int32_t*)p) = pixel ;
				}

				p += bpp ;
			}

			x += x_off ;
		}

		x_display_put_image( win->disp->display ,
			(x = orig_x) , y + y_off - font_ascent , src , p - src) ;
		p = src ;
	}

	return  1 ;
}

static int
copy_area(
	x_window_t *  win ,
	Pixmap  src ,
	PixmapMask  mask ,
	int  src_x ,	/* can be minus */
	int  src_y ,	/* can be minus */
	u_int  width ,
	u_int  height ,
	int  dst_x ,	/* can be minus */
	int  dst_y ,	/* can be minus */
	int  accept_margin	/* x/y can be minus or not */
	)
{
	int  margin ;
	int  right_margin ;
	int  bottom_margin ;
	int  y_off ;
	u_int  bpp ;
	u_char *  image ;
	size_t  src_width_size ;

	if( ! win->is_mapped || dst_x >= (int)win->width || dst_y >= (int)win->height)
	{
		return  0 ;
	}

	if( accept_margin)
	{
		margin = win->margin ;
		right_margin = bottom_margin = 0 ;
	}
	else
	{
		margin = 0 ;
		right_margin = RIGHT_MARGIN(win) ;
		bottom_margin = BOTTOM_MARGIN(win) ;
	}

	if( dst_x + width > win->width + margin - right_margin)
	{
		width = win->width + margin - right_margin - dst_x ;
	}

	if( dst_y + height > win->height + margin - bottom_margin)
	{
		height = win->height + margin - bottom_margin - dst_y ;
	}

	bpp = win->disp->display->bytes_per_pixel ;
	src_width_size = src->width * bpp ;
	image = src->image + src_width_size * (margin + src_y) +
			bpp * ( margin + src_x) ;

	if( mask)
	{
		mask += ((margin + src_y) * src->width + margin + src_x) ;

		for( y_off = 0 ; y_off < height ; y_off++)
		{
			int  x_off ;
			u_int  w ;

			w = 0 ;
			for( x_off = 0 ; x_off < width ; x_off++)
			{
				if( mask[x_off])
				{
					w ++ ;

					if( x_off + 1 == width)
					{
						/* for x_off - w */
						x_off ++ ;
					}
					else
					{
						continue ;
					}
				}
				else if( w == 0)
				{
					continue ;
				}

				x_display_put_image( win->disp->display ,
					win->x + win->margin + dst_x + x_off - w ,
					win->y + win->margin + dst_y + y_off ,
					image + bpp * ( x_off - w) ,
					w * bpp) ;
				w = 0 ;
			}

			mask += src->width ;
			image += src_width_size ;
		}
	}
	else
	{
		size_t  size ;

		size = width * bpp ;

		for( y_off = 0 ; y_off < height ; y_off++)
		{
			x_display_put_image( win->disp->display ,
				win->x + win->margin + dst_x ,
				win->y + win->margin + dst_y + y_off ,
				image , size) ;
			image += src_width_size ;
		}
	}

	return  1 ;
}

static int
clear_margin_area(
	x_window_t *  win
	)
{
	u_int  right_margin ;
	u_int  bottom_margin ;

	right_margin = RIGHT_MARGIN(win) ;
	bottom_margin = BOTTOM_MARGIN(win) ;

	if( win->margin | right_margin | bottom_margin)
	{
		x_window_clear( win , -(win->margin) , -(win->margin) ,
			win->margin , ACTUAL_HEIGHT(win)) ;
		x_window_clear( win , 0 , -(win->margin) , win->width , win->margin) ;
		x_window_clear( win , win->width - right_margin , -(win->margin) ,
			win->margin + right_margin , ACTUAL_HEIGHT(win)) ;
		x_window_clear( win , 0 , win->height - bottom_margin ,
			win->width , win->margin + bottom_margin) ;
	}

	/* XXX */
	if( win->num_of_children == 2 &&
	    ACTUAL_HEIGHT(win->children[0]) == ACTUAL_HEIGHT(win->children[1]) )
	{
		if( win->children[0]->x + ACTUAL_WIDTH(win->children[0]) <= win->children[1]->x)
		{
			x_window_clear( win ,
				win->children[0]->x + ACTUAL_WIDTH(win->children[0]) , 0 ,
				win->children[1]->x - win->children[0]->x -
					ACTUAL_WIDTH(win->children[0]) ,
				win->height) ;
		}
		else if( win->children[0]->x >=
			win->children[1]->x + ACTUAL_WIDTH(win->children[1]))
		{
			x_window_clear( win ,
				win->children[1]->x + ACTUAL_WIDTH(win->children[1]) , 0 ,
				win->children[0]->x - win->children[1]->x -
					ACTUAL_WIDTH(win->children[1]) ,
				win->height) ;
		}
	}

	return  1 ;
}

static int
window_move(
	x_window_t *  win ,
	int  x ,
	int  y ,
	int  expose	/* 1=full, -1=normal, 0=no */
	)
{
	int  prev_x ;
	int  prev_y ;

	prev_x = win->x ;
	prev_y = win->y ;

	win->x = x ;
	win->y = y ;

	if( expose == 1)
	{
		/*
		 * x_expose_window() must be called after win->x and win->y is set
		 * because window_exposed event which is called in x_expose_window()
		 * can call x_window_move() recursively.
		 */
		x_display_expose( prev_x , prev_y , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;
	}

	clear_margin_area( win) ;

	if( expose && win->window_exposed)
	{
		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
	}
#if  0
	else
	{
		x_window_clear_all( win) ;
	}
#endif

	/* XXX */
	if( win->parent)
	{
		clear_margin_area( win->parent) ;
	}

	return  1 ;
}


/* --- global functions --- */

int
x_window_init(
	x_window_t *  win ,
	u_int  width ,
	u_int  height ,
	u_int  min_width ,
	u_int  min_height ,
	u_int  base_width ,
	u_int  base_height ,
	u_int  width_inc ,
	u_int  height_inc ,
	u_int  margin ,
	int  create_gc
	)
{
	memset( win , 0 , sizeof( x_window_t)) ;

	/* If wall picture is set, scrollable will be 0. */
	win->is_scrollable = 1 ;

	win->is_focused = 1 ;
	win->is_mapped = 1 ;

	win->create_gc = create_gc ;

	win->width = width ;
	win->height = height ;
	win->min_width = min_width ;
	win->min_height = min_height ;
	win->base_width = base_width ;
	win->base_height = base_height ;
	win->width_inc = width_inc ;
	win->height_inc = height_inc ;
	win->margin = margin ;

	win->prev_clicked_button = -1 ;

	win->app_name = "mlterm" ;	/* Can be changed in x_display_show_root(). */

	return	1 ;
}

int
x_window_final(
	x_window_t *  win
	)
{
	u_int  count ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_final( win->children[count]) ;
	}

	free( win->children) ;

	if( win->window_finalized)
	{
		(*win->window_finalized)( win) ;
	}

	return  1 ;
}

int
x_window_set_type_engine(
	x_window_t *  win ,
	x_type_engine_t  type_engine
	)
{
	return  1 ;
}

int
x_window_init_event_mask(
	x_window_t *  win ,
	long  event_mask
	)
{
	return  1 ;
}

int
x_window_add_event_mask(
	x_window_t *  win ,
	long  event_mask
	)
{
	return  1 ;
}

int
x_window_remove_event_mask(
	x_window_t *  win ,
	long  event_mask
	)
{
	return  1 ;
}

int
x_window_ungrab_pointer(
	x_window_t *  win
	)
{
	return  0 ;
}

int
x_window_set_wall_picture(
	x_window_t *  win ,
	Pixmap  pic
	)
{
	win->wall_picture = pic ;
	win->is_scrollable = 0 ;

	clear_margin_area( win) ;

	if( win->window_exposed)
	{
		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
	}
#if  0
	else
	{
		x_window_clear_all( win) ;
	}
#endif

	return  1 ;
}

int
x_window_unset_wall_picture(
	x_window_t *  win
	)
{
	win->wall_picture = None ;
	win->is_scrollable = 1 ;

	clear_margin_area( win) ;

	if( win->window_exposed)
	{
		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
	}
#if  0
	else
	{
		x_window_clear_all( win) ;
	}
#endif

	return  1 ;
}

int
x_window_set_transparent(
	x_window_t *  win ,		/* Transparency is applied to all children recursively */
	x_picture_modifier_ptr_t  pic_mod
	)
{
	return  0 ;
}

int
x_window_unset_transparent(
	x_window_t *  win
	)
{
	return  0 ;
}

int
x_window_set_cursor(
	x_window_t *  win ,
	u_int  cursor_shape
	)
{
	win->cursor_shape = cursor_shape ;

	return  1 ;
}

int
x_window_set_fg_color(
	x_window_t *  win ,
	x_color_t *  fg_color
	)
{
	win->fg_color = *fg_color ;

	return  1 ;
}

int
x_window_set_bg_color(
	x_window_t *  win ,
	x_color_t *  bg_color
	)
{
	win->bg_color = *bg_color ;

	clear_margin_area( win) ;

	return  1 ;
}

int
x_window_add_child(
	x_window_t *  win ,
	x_window_t *  child ,
	int  x ,
	int  y ,
	int  map
	)
{
	void *  p ;

	if( win->parent)
	{
		/* Can't add a grand child window. */
		return  0 ;
	}

	if( ( p = realloc( win->children , sizeof( *win->children) * (win->num_of_children + 1)))
		== NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " realloc failed.\n") ;
	#endif

		return  0 ;
	}

	win->children = p ;

	child->parent = win ;
	child->x = x ;
	child->y = y ;
	child->is_mapped = map ;

	win->children[ win->num_of_children ++] = child ;

	return  1 ;
}

x_window_t *
x_get_root_window(
	x_window_t *  win
	)
{
	while( win->parent != NULL)
	{
		win = win->parent ;
	}

	return  win ;
}

GC
x_window_get_fg_gc(
	x_window_t *  win
	)
{
	return  None ;
}

GC
x_window_get_bg_gc(
	x_window_t *  win
	)
{
	return  None ;
}

int
x_window_show(
	x_window_t *  win ,
	int  hint	/* If win->parent(_window) is None,
			   specify XValue|YValue to localte window at win->x/win->y. */
	)
{
	u_int  count ;

	if( win->my_window)
	{
		/* already shown */

		return  0 ;
	}

	if( win->parent)
	{
		win->disp = win->parent->disp ;
		win->parent_window = win->parent->my_window ;
		win->gc = win->parent->gc ;
	}

	win->my_window = win ;	/* dummy */

	/*
	 * This should be called after Window Manager settings, because
	 * x_set_{window|icon}_name() can be called in win->window_realized().
	 */
	if( win->window_realized)
	{
		int  is_mapped ;

		/*
		 * Don't show anything until x_window_resize_with_margin() is called
		 * at the end of this function.
		 */
		is_mapped = win->is_mapped ;
		win->is_mapped = 0 ;
		(*win->window_realized)( win) ;
		win->is_mapped = is_mapped ;
	}

	/*
	 * showing child windows.
	 */

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_show( win->children[count] , 0) ;
	}

	if( ! win->parent && win->x == 0 && win->y == 0)
	{
		x_window_resize_with_margin( win , win->disp->width ,
			win->disp->height , NOTIFY_TO_MYSELF) ;
	}

	return  1 ;
}

int
x_window_map(
	x_window_t *  win
	)
{
	if( ! win->is_mapped)
	{
		win->is_mapped = 1 ;
		clear_margin_area( win) ;
	}

	return  1 ;
}

int
x_window_unmap(
	x_window_t *  win
	)
{
	win->is_mapped = 0 ;

	return  1 ;
}

int
x_window_resize(
	x_window_t *  win ,
	u_int  width ,		/* excluding margin */
	u_int  height ,		/* excluding margin */
	x_resize_flag_t  flag	/* NOTIFY_TO_PARENT , NOTIFY_TO_MYSELF */
	)
{
	if( (flag & NOTIFY_TO_PARENT) && win->parent)
	{
		/*
		 * XXX
		 * If Font size, screen_{width|height}_ratio or vertical_mode is changed
		 * and x_window_resize( NOTIFY_TO_PARENT) is called, ignore this call and
		 * resize windows with display size.
		 */
		win->parent->width = 0 ;
		return  x_window_resize_with_margin( win->parent ,
				win->disp->width , win->disp->height ,
				NOTIFY_TO_MYSELF) ;
	}

	if( width + win->margin * 2 > win->disp->width)
	{
		width = win->disp->width - win->margin * 2 ;
	}

	if( height + win->margin * 2 > win->disp->height)
	{
		height = win->disp->height - win->margin * 2 ;
	}

	if( win->width == width && win->height == height)
	{
		return  0 ;
	}

	win->width = width ;
	win->height = height ;

	if( flag & NOTIFY_TO_MYSELF)
	{
		if( win->window_resized)
		{
			(*win->window_resized)( win) ;
		}

		/*
		 * clear_margin_area() must be called after win->window_resized
		 * because wall_picture can be resized to fit to the new window
		 * size in win->window_resized.
		 *
		 * Don't clear_margin_area() if flag == 0 because segfault happens
		 * in selecting candidates on the input method window.
		 */
		clear_margin_area( win) ;
	}

	return  1 ;
}

/*
 * !! Notice !!
 * This function is not recommended.
 * Use x_window_resize if at all possible.
 */
int
x_window_resize_with_margin(
	x_window_t *  win ,
	u_int  width ,
	u_int  height ,
	x_resize_flag_t  flag	/* NOTIFY_TO_PARENT , NOTIFY_TO_MYSELF */
	)
{
	return  x_window_resize( win , width - win->margin * 2 , height - win->margin * 2 , flag) ;
}

int
x_window_set_normal_hints(
	x_window_t *  win ,
	u_int  min_width ,
	u_int  min_height ,
	u_int  base_width ,
	u_int  base_height ,
	u_int  width_inc ,
	u_int  height_inc
	)
{
	return  1 ;
}

int
x_window_set_override_redirect(
	x_window_t *  win ,
	int  flag
	)
{
	return  0 ;
}

int
x_window_set_borderless_flag(
	x_window_t *  win ,
	int  flag
	)
{
	return  0 ;
}

int
x_window_move(
	x_window_t *  win ,
	int  x ,
	int  y
	)
{
	return  window_move( win , x , y , -1) ;
}

/* XXX for x_im_status_screen */
int
x_window_move_full_expose(
	x_window_t *  win ,
	int  x ,
	int  y
	)
{
	return  window_move( win , x , y , 1) ;
}

/* XXX for x_im_candidate_screen */
int
x_window_move_no_expose(
	x_window_t *  win ,
	int  x ,
	int  y
	)
{
	return  window_move( win , x , y , 0) ;
}

int
x_window_clear(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
	if( ! win->wall_picture)
	{
		return  x_window_fill_with( win , &win->bg_color , x , y , width , height) ;
	}
	else
	{
		return  copy_area( win , win->wall_picture , None ,
				x , y , width , height , x , y , 1) ;
	}

	return  1 ;
}

int
x_window_clear_all(
	x_window_t *  win
	)
{
	return  x_window_clear( win , 0 , 0 , win->width , win->height) ;
}

int
x_window_fill(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int	width ,
	u_int	height
	)
{
	return  x_window_fill_with( win , &win->fg_color , x , y , width , height) ;
}

int
x_window_fill_with(
	x_window_t *  win ,
	x_color_t *  color ,
	int  x ,
	int  y ,
	u_int	width ,
	u_int	height
	)
{
	u_char *  src ;
	u_char *  p ;
	size_t  size ;
	int  x_off ;
	int  y_off ;
	u_int  bpp ;

	if( ! win->is_mapped)
	{
		return  0 ;
	}

	bpp = win->disp->display->bytes_per_pixel ;

	if( ! ( p = src = alloca( ( size = width * bpp))))
	{
		return  0 ;
	}

	x += (win->x + win->margin) ;
	y += (win->y + win->margin) ;

	for( y_off = 0 ; y_off < height ; y_off++)
	{
		for( x_off = 0 ; x_off < width ; x_off++)
		{
			switch( bpp)
			{
			case 1:
				*p = color->pixel ;
				break ;
			case 2:
				*((u_int16_t*)p) = color->pixel ;
				break ;
			/* case 4: */
			default:
				*((u_int32_t*)p) = color->pixel ;
			}

			p += bpp ;
		}

		x_display_put_image( win->disp->display , x , y + y_off , src , p - src) ;

		p = src ;
	}

	return  1 ;
}

int
x_window_blank(
	x_window_t *  win
	)
{
	return  x_window_fill_with( win , &win->fg_color , 0 , 0 , win->width , win->height) ;
}

int
x_window_update(
	x_window_t *  win ,
	int  flag
	)
{
	if( win->update_window)
	{
		(*win->update_window)( win, flag) ;
	}

	return  1 ;
}

int
x_window_update_all(
	x_window_t *  win
	)
{
	u_int  count ;

	if( ! win->parent)
	{
		x_display_reset_cmap( win->disp->display) ;
	}

	clear_margin_area( win) ;

	if( win->window_exposed)
	{
		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
	}

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_update_all( win->children[count]) ;
	}

	return  1 ;
}

void
x_window_idling(
	x_window_t *  win
	)
{
	u_int  count ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_idling( win->children[count]) ;
	}

#ifdef  __DEBUG
	if( win->button_is_pressing)
	{
		kik_debug_printf( KIK_DEBUG_TAG " button is pressing...\n") ;
	}
#endif

	if( win->button_is_pressing && win->button_press_continued)
	{
		(*win->button_press_continued)( win , &win->prev_button_press_event) ;
	}
	else if( win->idling)
	{
		(*win->idling)( win) ;
	}
}

/*
 * Return value: 0 => different window.
 *               1 => finished processing.
 */
int
x_window_receive_event(
	x_window_t *   win ,
	XEvent *  event
	)
{
#if  0
	u_int  count ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		if( x_window_receive_event( win->children[count] , event))
		{
			return  1 ;
		}
	}
#endif

	if( event->type == KeyPress)
	{
		if( win->key_pressed)
		{
			(*win->key_pressed)( win , &event->xkey) ;
		}
	}
	else if( event->type == MotionNotify)
	{
		if( win->button_is_pressing)
		{
			if( win->button_motion)
			{
				event->xmotion.x -= win->margin ;
				event->xmotion.y -= win->margin ;

				(*win->button_motion)( win , &event->xmotion) ;
			}

			/* following button motion ... */

			win->prev_button_press_event.x = event->xmotion.x ;
			win->prev_button_press_event.y = event->xmotion.y ;
			win->prev_button_press_event.time = event->xmotion.time ;
		}
		else if( win->pointer_motion)
		{
			event->xmotion.x -= win->margin ;
			event->xmotion.y -= win->margin ;

			(*win->pointer_motion)( win , &event->xmotion) ;
		}
	}
	else if( event->type == ButtonRelease)
	{
		if( win->button_released)
		{
			event->xbutton.x -= win->margin ;
			event->xbutton.y -= win->margin ;

			(*win->button_released)( win , &event->xbutton) ;
		}

		win->button_is_pressing = 0 ;
	}
	else if( event->type == ButtonPress)
	{
		if( win->button_pressed)
		{
			event->xbutton.x -= win->margin ;
			event->xbutton.y -= win->margin ;

			if( win->click_num == MAX_CLICK)
			{
				win->click_num = 0 ;
			}

			if( win->prev_clicked_time + click_interval >= event->xbutton.time &&
				event->xbutton.button == win->prev_clicked_button)
			{
				win->click_num ++ ;
				win->prev_clicked_time = event->xbutton.time ;
			}
			else
			{
				win->click_num = 1 ;
				win->prev_clicked_time = event->xbutton.time ;
				win->prev_clicked_button = event->xbutton.button ;
			}

			(*win->button_pressed)( win , &event->xbutton , win->click_num) ;
		}

		if( event->xbutton.button <= Button3)
		{
			/* button_is_pressing flag is on except wheel mouse (Button4/Button5). */
			win->button_is_pressing = 1 ;
			win->prev_button_press_event = event->xbutton ;
		}
	}

	return  1 ;
}

size_t
x_window_get_str(
	x_window_t *  win ,
	u_char *  seq ,
	size_t  seq_len ,
	mkf_parser_t **  parser ,
	KeySym *  keysym ,
	XKeyEvent *  event
	)
{
	u_char  ch ;

	if( seq_len == 0)
	{
		return  0 ;
	}

	*parser = NULL ;

	ch = event->ksym ;

	if( ( *keysym = event->ksym) >= 0x100)
	{
		switch( *keysym)
		{
		case  XK_KP_Multiply:
			ch = '*' ;
			break ;
		case  XK_KP_Add:
			ch = '+' ;
			break ;
		case  XK_KP_Separator:
			ch = ',' ;
			break ;
		case  XK_KP_Subtract:
			ch = '-' ;
			break ;
		case  XK_KP_Divide:
			ch = '/' ;
			break ;

		default:
			if( win->disp->display->lock_state & NLKED)
			{
				switch( *keysym)
				{
				case  XK_KP_Insert:
					ch = '0' ;
					break ;
				case  XK_KP_End:
					ch = '1' ;
					break ;
				case  XK_KP_Down:
					ch = '2' ;
					break ;
				case  XK_KP_Next:
					ch = '3' ;
					break ;
				case  XK_KP_Left:
					ch = '4' ;
					break ;
				case  XK_KP_5:
					ch = '5' ;
					break ;
				case  XK_KP_Right:
					ch = '6' ;
					break ;
				case  XK_KP_Home:
					ch = '7' ;
					break ;
				case  XK_KP_Up:
					ch = '8' ;
					break ;
				case  XK_KP_Prior:
					ch = '9' ;
					break ;
				case  XK_KP_Delete:
					ch = '.' ;
					break ;
				default:
					return  0 ;
				}

				*keysym = ch ;
			}
			else
			{
				return  0 ;
			}
		}
	}

	/*
	 * Control + '@'(0x40) ... '_'(0x5f) -> 0x00 ... 0x1f
	 *
	 * Not "<= '_'" but "<= 'z'" because Control + 'a' is not
	 * distinguished from Control + 'A'.
	 */
	if( (event->state & ControlMask) &&
	    (ch == ' ' || ('@' <= ch && ch <= 'z')) )
	{
		seq[0] = (ch & 0x1f) ;
	}
	else
	{
		seq[0] = ch ;
	}

	return  1 ;
}

/*
 * Scroll functions.
 * The caller side should clear the scrolled area.
 */

int
x_window_scroll_upward(
	x_window_t *  win ,
	u_int  height
	)
{
	return  x_window_scroll_upward_region( win , 0 , win->height , height) ;
}

int
x_window_is_scrollable(
	x_window_t *  win
	)
{
	/* XXX If input method module is activated, don't scroll window. */
	if( win->is_scrollable &&
	    (win->disp->num_of_roots == 1 || ! win->disp->roots[1]->is_mapped) )
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
x_window_scroll_upward_region(
	x_window_t *  win ,
	int  boundary_start ,
	int  boundary_end ,
	u_int  height
	)
{
	if( boundary_start < 0 || boundary_end > win->height ||
	    boundary_end <= boundary_start + height)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" boundary start %d end %d height %d in window((h) %d (w) %d)\n" ,
			boundary_start , boundary_end , height , win->height , win->width) ;
	#endif

		return  0 ;
	}

	return  scroll_region( win ,
			0 , boundary_start + height ,	/* src */
			win->width , boundary_end - boundary_start - height ,	/* size */
			0 , boundary_start) ;		/* dst */
}

int
x_window_scroll_downward(
	x_window_t *  win ,
	u_int  height
	)
{
	return  x_window_scroll_downward_region( win , 0 , win->height , height) ;
}

int
x_window_scroll_downward_region(
	x_window_t *  win ,
	int  boundary_start ,
	int  boundary_end ,
	u_int  height
	)
{
	if( boundary_start < 0 || boundary_end > win->height ||
	    boundary_end <= boundary_start + height)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " boundary start %d end %d height %d\n" ,
			boundary_start , boundary_end , height) ;
	#endif

		return  0 ;
	}

	return  scroll_region( win ,
			0 , boundary_start ,
			win->width , boundary_end - boundary_start - height ,
			0 , boundary_start + height) ;
}

int
x_window_scroll_leftward(
	x_window_t *  win ,
	u_int  width
	)
{
	return  x_window_scroll_leftward_region( win , 0 , win->width , width) ;
}

static int
fix_rl_boundary(
	x_window_t *  win ,
	int  boundary_start ,
	int *  boundary_end
	)
{
	int  margin ;

	margin = RIGHT_MARGIN(win) ;

	if( boundary_start > win->width - margin)
	{
		return  0 ;
	}

	if( *boundary_end > win->width - margin)
	{
		*boundary_end = win->width - margin ;
	}

	return  1 ;
}

int
x_window_scroll_leftward_region(
	x_window_t *  win ,
	int  boundary_start ,
	int  boundary_end ,
	u_int  width
	)
{
	if( boundary_start < 0 || boundary_end > win->width ||
	    boundary_end <= boundary_start + width ||
	    ! fix_rl_boundary( win , boundary_start , &boundary_end))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" boundary start %d end %d width %d in window((h) %d (w) %d)\n" ,
			boundary_start , boundary_end , width , win->height , win->width) ;
	#endif

		return  0 ;
	}

	scroll_region( win ,
		boundary_start + width , 0 ,	/* src */
		boundary_end - boundary_start - width , win->height ,	/* size */
		boundary_start , 0) ;		/* dst */

	return  1 ;
}

int
x_window_scroll_rightward(
	x_window_t *  win ,
	u_int  width
	)
{
	return  x_window_scroll_rightward_region( win , 0 , win->width , width) ;
}

int
x_window_scroll_rightward_region(
	x_window_t *  win ,
	int  boundary_start ,
	int  boundary_end ,
	u_int  width
	)
{
	if( boundary_start < 0 || boundary_end > win->width ||
	    boundary_end <= boundary_start + width ||
	    ! fix_rl_boundary( win , boundary_start , &boundary_end))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " boundary start %d end %d width %d\n" ,
			boundary_start , boundary_end , width) ;
	#endif

		return  0 ;
	}

	scroll_region( win ,
		boundary_start , 0 ,
		boundary_end - boundary_start - width , win->height ,
		boundary_start + width , 0) ;

	return  1 ;
}

int
x_window_copy_area(
	x_window_t *  win ,
	Pixmap  src ,
	PixmapMask  mask ,
	int  src_x ,	/* >= 0 */
	int  src_y ,	/* >= 0 */
	u_int  width ,
	u_int  height ,
	int  dst_x ,	/* >= 0 */
	int  dst_y	/* >= 0 */
	)
{
	return  copy_area( win , src , mask , src_x , src_y , width , height , dst_x , dst_y , 0) ;
}

int
x_window_draw_decsp_string(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len
	)
{
	return  x_window_draw_string( win , font , fg_color , x , y , str , len) ;
}

int
x_window_draw_decsp_image_string(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	x_color_t *  bg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len
	)
{
	return  x_window_draw_image_string( win , font , fg_color , bg_color ,
			x , y , str , len) ;
}

int
x_window_draw_string(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len
	)
{
	return  draw_string( win , font , fg_color , NULL , x , y , str , len , 1) ;
}

int
x_window_draw_string16(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	XChar2b *  str ,
	u_int  len
	)
{
	return  draw_string( win , font , fg_color , NULL , x , y , str , len , 2) ;
}

int
x_window_draw_image_string(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	x_color_t *  bg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len
	)
{
	return  draw_string( win , font , fg_color , bg_color , x , y , str , len , 1) ;
}

int
x_window_draw_image_string16(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	x_color_t *  bg_color ,
	int  x ,
	int  y ,
	XChar2b *  str ,
	u_int  len
	)
{
	return  draw_string( win , font , fg_color , bg_color , x , y , str , len , 2) ;
}

int
x_window_draw_rect_frame(
	x_window_t *  win ,
	int  x1 ,
	int  y1 ,
	int  x2 ,
	int  y2
	)
{
	x_window_fill_with( win , &win->fg_color , x1 , y1 , x2 - x1 + 1 , 1) ;
	x_window_fill_with( win , &win->fg_color , x1 , y1 , 1 , y2 - y1 + 1) ;
	x_window_fill_with( win , &win->fg_color , x1 , y2 , x2 - x1 + 1 , 1) ;
	x_window_fill_with( win , &win->fg_color , x2 , y1 , 1 , y2 - y1 + 1) ;

	return  1 ;
}

int
x_set_use_clipboard_selection(
	int  use_it
	)
{
	return  0 ;
}

int
x_is_using_clipboard_selection(void)
{
	return  0 ;
}

int
x_window_set_selection_owner(
	x_window_t *  win ,
	Time  time
	)
{
	win->is_sel_owner = 1 ;

	return  1 ;
}

int
x_window_xct_selection_request(
	x_window_t *  win ,
	Time  time
	)
{
	return  1 ;
}

int
x_window_utf_selection_request(
	x_window_t *  win ,
	Time  time
	)
{
	return  1 ;
}

int
x_window_send_selection(
	x_window_t *  win ,
	XSelectionRequestEvent *  req_ev ,
	u_char *  sel_data ,
	size_t  sel_len ,
	Atom  sel_type ,
	int sel_format
	)
{
	return  1 ;
}

int
x_set_window_name(
	x_window_t *  win ,
	u_char *  name
	)
{
	return  1 ;
}

int
x_set_icon_name(
	x_window_t *  win ,
	u_char *  name
	)
{
	return  1 ;
}

int
x_window_set_icon(
	x_window_t *  win ,
	x_icon_picture_ptr_t  icon
	)
{
	return 1 ;
}

int
x_window_remove_icon(
	x_window_t *  win
	)
{
	return  1 ;
}

int
x_window_reset_group(
	x_window_t *  win
	)
{
	return  1 ;
}

int
x_window_get_visible_geometry(
	x_window_t *  win ,
	int *  x ,		/* x relative to root window */
	int *  y ,		/* y relative to root window */
	int *  my_x ,		/* x relative to my window */
	int *  my_y ,		/* y relative to my window */
	u_int *  width ,
	u_int *  height
	)
{
	return  1 ;
}

int
x_set_click_interval(
	int  interval
	)
{
	click_interval = interval ;

	return  1 ;
}

u_int
x_window_get_mod_ignore_mask(
	x_window_t *  win ,
	KeySym *  keysyms
	)
{
	return  ~0 ;
}

u_int
x_window_get_mod_meta_mask(
	x_window_t *  win ,
	char *  mod_key
	)
{
	return  ModMask ;
}

int
x_window_bell(
	x_window_t *  win ,
	x_bel_mode_t  bel_mode
	)
{
	return  1 ;
}

int
x_window_translate_coordinates(
	x_window_t *  win ,
	int  x ,
	int  y ,
	int *  global_x ,
	int *  global_y ,
	Window *  child
	)
{
	*global_x = x + win->x ;
	*global_y = y + win->y ;

	return  0 ;
}


/* for x_display.c */

int
x_window_clear_margin_area(
	x_window_t *  win
	)
{
	return  clear_margin_area( win) ;
}
