/*
 *	$Id$
 */

#include  "../x_window.h"

#include  <string.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>

#include  "x_display.h"


#define  MAX_CLICK  3			/* max is triple click */


/* --- static variables --- */

static int  click_interval = 250 ;	/* millisecond, same as xterm. */


/* --- static functions --- */

static void
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
	u_int  count ;

	if( src_y <= dst_y)
	{
		for( count = height ; count > 0 ; count--)
		{
			x_display_copy_line( win->disp->display ,
				src_x + win->margin ,
				src_y + win->margin + count - 1 ,
				dst_x + win->margin ,
				dst_y + win->margin + count - 1 ,
				width) ;
		}
	}
	else
	{
		for( count = 0 ; count < height ; count++)
		{
			x_display_copy_line( win->disp->display ,
				src_x + win->margin ,
				src_y + win->margin + count ,
				dst_x + win->margin ,
				dst_y + win->margin + count ,
				width) ;
		}
	}
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
	int  x_off ;
	int  y_off ;
	u_int  bpp ;
	u_int  count ;

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

	x += win->margin ;
	y += win->margin ;

	for( ; y_off < font_height ; y_off++)
	{
		for( count = 0 ; count < len ; count++)
		{
			int  force_fg ;

			force_fg = 0 ;

			for( x_off = 0 ; x_off < font->width ; x_off++)
			{
				u_long  pixel ;

				if( font->x_off <= x_off &&
				    x_get_bitmap_cell( font->xfont , bitmaps[count] ,
					x_off - font->x_off , y_off) )
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
						u_char *  fb ;

						fb = x_display_get_fb( win->disp->display , x ,
								y + y_off - font_ascent) +
							(p - src) ;

						switch( bpp)
						{
						case 1:
							pixel = *fb ;
							break ;
						case 2:
							pixel = *((u_int16_t*)fb) ;
							break ;
						/* case 4: */
						default:
							pixel = *((u_int32_t*)fb) ;
						}
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
		}

		x_display_put_image( win->disp->display ,
			x , y + y_off - font_ascent , src , size) ;
		p = src ;
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

	right_margin = (win->width - win->min_width) % win->width_inc ;
	bottom_margin = (win->height - win->min_height) % win->height_inc ;

	if( win->margin > 0)
	{
		x_window_clear( win , -(win->margin) , -(win->margin) ,
			win->margin , ACTUAL_HEIGHT(win)) ;
		x_window_clear( win , 0 , -(win->margin) , win->width , win->margin) ;
		x_window_clear( win , win->width - right_margin , -(win->margin) ,
			win->margin + right_margin , ACTUAL_HEIGHT(win)) ;
		x_window_clear( win , 0 , win->height - bottom_margin ,
			win->width , win->margin + bottom_margin) ;
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

	if( win->window_exposed)
	{
		clear_margin_area( win) ;
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

	if( win->window_exposed)
	{
		clear_margin_area( win) ;
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

	win->my_window = 1 ;	/* dummy */

	/*
	 * This should be called after Window Manager settings, because
	 * x_set_{window|icon}_name() can be called in win->window_realized().
	 */
	if( win->window_realized)
	{
		(*win->window_realized)( win) ;
	}

	x_window_resize_with_margin( win , win->disp->width ,
		win->disp->height , NOTIFY_TO_MYSELF) ;
	clear_margin_area( win) ;

	/*
	 * showing child windows.
	 */

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_show( win->children[count] , 0) ;
	}

	return  1 ;
}

int
x_window_map(
	x_window_t *  win
	)
{
	return  1 ;
}

int
x_window_unmap(
	x_window_t *  win
	)
{
	return  1 ;
}

int
x_window_resize(
	x_window_t *  win ,
	u_int  width ,			/* excluding margin */
	u_int  height ,			/* excluding margin */
	x_resize_flag_t  flag	/* NOTIFY_TO_PARENT , NOTIFY_TO_MYSELF */
	)
{
	if( win->width == width && win->height == height)
	{
		return  0 ;
	}

#if  0
	win->width = width ;
	win->height = height ;
#else
	/* Forcibly set the same size of the screen. */
	win->width = win->disp->width - win->margin * 2 ;
	win->height = win->disp->height - win->margin * 2 ;
#endif

	if( ( flag & NOTIFY_TO_MYSELF) && win->window_resized)
	{
		(*win->window_resized)( win) ;
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
	return  1 ;
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
		return  x_window_copy_area( win , win->wall_picture ,
				x + win->margin , y + win->margin ,
				width , height , x , y) ;
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

	bpp = win->disp->display->bytes_per_pixel ;

	if( ! ( p = src = alloca( ( size = width * bpp))))
	{
		return  0 ;
	}

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

		x_display_put_image( win->disp->display , win->margin + x ,
			win->margin + y + y_off , src , size) ;

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
	(*win->update_window)( win, flag) ;

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
	if( seq_len == 0)
	{
		return  0 ;
	}

	*parser = NULL ;

	if( ( *keysym = event->ksym) >= 0x100)
	{
		return  0 ;
	}

	seq[0] = event->ksym ;

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
x_window_scroll_upward_region(
	x_window_t *  win ,
	int  boundary_start ,
	int  boundary_end ,
	u_int  height
	)
{
	if( ! win->is_scrollable)
	{
		return  0 ;
	}

	if( boundary_start < 0 || boundary_end > win->height || boundary_end <= boundary_start + height)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" boundary start %d end %d height %d in window((h) %d (w) %d)\n" ,
			boundary_start , boundary_end , height , win->height , win->width) ;
	#endif

		return  0 ;
	}

	scroll_region( win ,
		0 , boundary_start + height ,	/* src */
		win->width , boundary_end - boundary_start - height ,	/* size */
		0 , boundary_start) ;		/* dst */

	return  1 ;
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
	if( ! win->is_scrollable)
	{
		return  0 ;
	}

	if( boundary_start < 0 || boundary_end > win->height ||
		boundary_end <= boundary_start + height)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " boundary start %d end %d height %d\n" ,
			boundary_start , boundary_end , height) ;
	#endif

		return  0 ;
	}

	scroll_region( win ,
		0 , boundary_start ,
		win->width , boundary_end - boundary_start - height ,
		0 , boundary_start + height) ;

	return  1 ;
}

int
x_window_scroll_leftward(
	x_window_t *  win ,
	u_int  width
	)
{
	return  x_window_scroll_leftward_region( win , 0 , win->width , width) ;
}

int
x_window_scroll_leftward_region(
	x_window_t *  win ,
	int  boundary_start ,
	int  boundary_end ,
	u_int  width
	)
{
	if( ! win->is_scrollable)
	{
		return  0 ;
	}

	if( boundary_start < 0 || boundary_end > win->width || boundary_end <= boundary_start + width)
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
	if( ! win->is_scrollable)
	{
		return  0 ;
	}

	if( boundary_start < 0 || boundary_end > win->width || boundary_end <= boundary_start + width)
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
	int  src_x ,
	int  src_y ,
	u_int  width ,
	u_int  height ,
	int  dst_x ,
	int  dst_y
	)
{
	size_t  size ;
	int  y_off ;
	u_int  bpp ;

	size = width * (bpp = win->disp->display->bytes_per_pixel) ;

	for( y_off = 0 ; y_off < height ; y_off++)
	{
		x_display_put_image( win->disp->display ,
			win->margin + dst_x ,
			win->margin + dst_y + y_off ,
			src->image + bpp * ((src_y + y_off) * src->width + src_x) ,
			size) ;
	}

	return  1 ;
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
	x_window_fill_with( win , &win->fg_color , x1 , y1 , abs(x2-x1) , 1) ;
	x_window_fill_with( win , &win->fg_color , x1 , y1 , 1 , abs(y2-y1)) ;
	x_window_fill_with( win , &win->fg_color , x1 , y2 , abs(x2-x1) , 1) ;
	x_window_fill_with( win , &win->fg_color , x2 , y1 , 1 , abs(y2-y1) + 1) ;

	return  1 ;
}

int
x_window_draw_line(
	x_window_t *  win,
	int  x1,
	int  y1,
	int  x2,
	int  y2
	)
{
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
	int  visual
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
	return  0 ;
}
