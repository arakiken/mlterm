/*
 *	$Id$
 */

#include  "../x_window.h"

#include  <string.h>
#include  <stdio.h>

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_unistd.h>	/* kik_usleep */
#include  <mkf/mkf_codepoint_parser.h>
#include  <mkf/mkf_utf16_parser.h>

#include  <ml_char.h>

#include  "x_display.h"
#include  "x_font.h"


#define  MAX_CLICK  3			/* max is triple click */

/* win->width is not multiples of (win)->width_inc in framebuffer. */
#define  RIGHT_MARGIN(win) \
	((win)->width_inc ? ((win)->width - (win)->min_width) % (win)->width_inc : 0)
#define  BOTTOM_MARGIN(win) \
	((win)->height_inc ? ((win)->height - (win)->min_height) % (win)->height_inc : 0)

#ifdef  USE_GRF
static x_color_t  black = { TP_COLOR , 0 , 0 , 0 , 0 } ;
#endif

#define  ParentRelative  (1L)
#define  DummyPixmap  (2L)

#define  COL_WIDTH  (win->disp->display->col_width)
#define  LINE_HEIGHT  (win->disp->display->line_height)

/* XXX Check if win is input method window or not. */
#define  IS_IM_WINDOW(win)  ((win)->disp->num_of_roots >= 2 && (win) == (win)->disp->roots[1])


/* --- static variables --- */

static int  click_interval = 250 ;	/* millisecond, same as xterm. */

static mkf_parser_t *  cp_parser ;
static mkf_parser_t *  utf16_parser ;


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
	int  top ;
	int  bottom ;
	int  left ;
	int  right ;

	if( ! win->is_mapped || ! x_window_is_scrollable( win))
	{
		return  0 ;
	}

	if( src_y < dst_y)
	{
		top = src_y ;
		bottom = dst_y + height ;
	}
	else
	{
		top = dst_y ;
		bottom = src_y + height ;
	}

	if( src_x < dst_x)
	{
		left = src_x ;
		right = dst_x + width ;
	}
	else
	{
		left = dst_x ;
		right = src_x + width ;
	}

	fprintf( win->disp->display->fp , "\x1b[%d;%dr" ,
		(top + win->y) / LINE_HEIGHT + 1 ,
		(bottom + win->y) / LINE_HEIGHT) ;
	fprintf( win->disp->display->fp , "\x1b[?69h\x1b[%d;%ds" ,
		(left + win->x) / COL_WIDTH + 1 ,
		(right + win->x) / COL_WIDTH) ;

	if( src_y < dst_y)
	{
		fprintf( win->disp->display->fp , "\x1b[%dT" ,
			(dst_y - src_y) / LINE_HEIGHT) ;
	}
	else
	{
		fprintf( win->disp->display->fp , "\x1b[%dS" ,
			(src_y - dst_y) / LINE_HEIGHT) ;
	}

	fprintf( win->disp->display->fp , "\x1b[r\x1b[?69l") ;

	return  1 ;
}

static int
draw_string(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	x_color_t *  bg_color ,	/* must be NULL if wall_picture_bg is 1 */
	int  x ,
	int  y ,
	u_char *  str ,	/* 'len * ch_len' bytes */
	u_int  len ,
	u_int  ch_len ,
	int  underline_style
	)
{
	u_char *  str2 ;

	if( ! win->is_mapped)
	{
		return  0 ;
	}

	if( ( ch_len != 1 || str[0] >= 0x80) &&
	    ( str2 = alloca( len * UTF_MAX_SIZE)))
	{
		mkf_parser_t *  parser ;
		mkf_charset_t  cs ;

		if( ( cs = FONT_CS(font->id)) == ISO10646_UCS4_1)
		{
			if( ! utf16_parser)
			{
				utf16_parser = mkf_utf16_parser_new() ;
			}

			parser = utf16_parser ;
			(*parser->init)( parser) ;
			(*parser->set_str)( parser , str , len * ch_len) ;
		}
		else
		{
			if( ! cp_parser)
			{
				cp_parser = mkf_codepoint_parser_new() ;
			}

			parser = cp_parser ;
			(*parser->init)( parser) ;
			/* 3rd argument of parser->set_str is len(16bit) + cs(16bit) */
			(*parser->set_str)( parser , str , (len * ch_len) | (cs << 16)) ;
		}

		(*win->disp->display->conv->init)( win->disp->display->conv) ;
		if( ( len = (*win->disp->display->conv->convert)( win->disp->display->conv ,
					str2 , len * UTF_MAX_SIZE , parser)) > 0)
		{
			str = str2 ;
		}
	}
	else
	{
		len *= ch_len ;
	}

	if( fg_color->pixel < 0x8)
	{
		fprintf( win->disp->display->fp , "\x1b[%dm" , fg_color->pixel + 30) ;
	}
	else if( fg_color->pixel < 0x10)
	{
		fprintf( win->disp->display->fp , "\x1b[%dm" ,
			(fg_color->pixel & ~ML_BOLD_COLOR_MASK) + 90) ;
	}
	else
	{
		fprintf( win->disp->display->fp , "\x1b[38;5;%dm" , fg_color->pixel) ;
	}

	if( ! bg_color)
	{
		bg_color = &win->bg_color ;
	}

	if( bg_color->pixel < 0x8)
	{
		fprintf( win->disp->display->fp , "\x1b[%dm" , bg_color->pixel + 40) ;
	}
	else if( bg_color->pixel < 0x10)
	{
		fprintf( win->disp->display->fp , "\x1b[%dm" ,
			(bg_color->pixel & ~ML_BOLD_COLOR_MASK) + 100) ;
	}
	else
	{
		fprintf( win->disp->display->fp , "\x1b[48;5;%dm" , bg_color->pixel) ;
	}

	switch( underline_style)
	{
	case  UNDERLINE_NORMAL:
		fprintf( win->disp->display->fp , "\x1b[4m") ;
		break ;
	case  UNDERLINE_DOUBLE:
		fprintf( win->disp->display->fp , "\x1b[21m") ;
		break ;
	}

	if( font->id & FONT_BOLD)
	{
		fprintf( win->disp->display->fp , "\x1b[1m") ;
	}

	if( font->id & FONT_ITALIC)
	{
		fprintf( win->disp->display->fp , "\x1b[3m") ;
	}

	fprintf( win->disp->display->fp , "\x1b[%d;%dH" ,
		(win->y + win->vmargin + y) / LINE_HEIGHT + 1 ,
		(win->x + win->hmargin + x) / COL_WIDTH + 1) ;
	fwrite( str , 1 , len , win->disp->display->fp) ;
	fwrite( "\x1b[m" , 1 , 3 , win->disp->display->fp) ;
	fflush( win->disp->display->fp) ;

	return  1 ;
}

#ifdef  USE_LIBSIXEL
static int
copy_area(
	x_window_t *  win ,
	Pixmap  src ,
	int  src_x ,	/* can be minus */
	int  src_y ,	/* can be minus */
	u_int  width ,
	u_int  height ,
	int  dst_x ,	/* can be minus */
	int  dst_y ,	/* can be minus */
	int  accept_margin	/* x/y can be minus and over width/height */
	)
{
	int  hmargin ;
	int  vmargin ;
	int  right_margin ;
	int  bottom_margin ;
	u_char *  picture ;

	if( ! win->is_mapped)
	{
		return  0 ;
	}

#if  0
	if( accept_margin)
#endif
	{
		hmargin = win->hmargin ;
		vmargin = win->vmargin ;
		right_margin = bottom_margin = 0 ;
	}
#if  0
	else
	{
		hmargin = vmargin = 0 ;
		right_margin = RIGHT_MARGIN(win) ;
		bottom_margin = BOTTOM_MARGIN(win) ;
	}
#endif

	if( dst_x >= (int)win->width + hmargin || dst_y >= (int)win->height + vmargin)
	{
		return  0 ;
	}

	if( dst_x + width > win->width + hmargin - right_margin)
	{
		width = win->width + hmargin - right_margin - dst_x ;
	}

	if( dst_y + height > win->height + vmargin - bottom_margin)
	{
		height = win->height + vmargin - bottom_margin - dst_y ;
	}

	picture = src->image + src->width * 4 * (vmargin + src_y) +
			4 * (hmargin + src_x) ;

	if( width < src->width)
	{
		u_char *  clip ;
		u_char *  p ;
		size_t  line_len ;

		line_len = width * 4 ;

		if( ( p = clip = calloc( line_len , height)))
		{
			u_int  count ;

			for( count = 0 ; count < height ; count++)
			{
				memcpy( p , picture , line_len) ;
				p += line_len ;
				picture += (src->width * 4) ;
			}

			picture = clip ;
		}
	}

	fprintf( win->disp->display->fp , "\x1b[%d;%dH" ,
		(win->y + win->vmargin + dst_y) / LINE_HEIGHT + 1 ,
		(win->x + win->hmargin + dst_x) / COL_WIDTH + 1) ;
	x_display_output_picture( win->disp , picture , width , height) ;
	fflush( win->disp->display->fp) ;

	if( width < src->width)
	{
		free( picture) ;
	}

	return  1 ;
}
#else
#define  copy_area( win , src , src_x , src_y , width , height , dst_x , dst_y , \
		accept_margin)  (0)
#endif

static int
clear_margin_area(
	x_window_t *  win
	)
{
	u_int  right_margin ;
	u_int  bottom_margin ;

	right_margin = RIGHT_MARGIN(win) ;
	bottom_margin = BOTTOM_MARGIN(win) ;

	if( win->hmargin | win->vmargin | right_margin | bottom_margin)
	{
		x_window_clear( win , -(win->hmargin) , -(win->vmargin) ,
			win->hmargin , ACTUAL_HEIGHT(win)) ;
		x_window_clear( win , 0 , -(win->vmargin) , win->width , win->vmargin) ;
		x_window_clear( win , win->width - right_margin , -(win->vmargin) ,
			win->hmargin + right_margin , ACTUAL_HEIGHT(win)) ;
		x_window_clear( win , 0 , win->height - bottom_margin ,
			win->width , win->vmargin + bottom_margin) ;
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

static void
reset_input_focus(
	x_window_t *  win
	)
{
	u_int  count ;

	if( win->inputtable)
	{
		win->inputtable = -1 ;
	}
	else
	{
		win->inputtable = 0 ;
	}

	if( win->is_focused)
	{
		win->is_focused = 0 ;

		if( win->window_unfocused)
		{
			(*win->window_unfocused)( win) ;
		}
	}

	for( count = 0 ; count < win->num_of_children ; count++)
	{
		reset_input_focus( win->children[count]) ;
	}
}

#if  0
static int
check_child_window_area(
	x_window_t *  win
	)
{
	if( win->num_of_children > 0)
	{
		u_int  count ;
		u_int  sum ;

		for( sum = 0 , count = 1 ; count < win->num_of_children ; count++)
		{
			sum += (ACTUAL_WIDTH(win->children[count]) *
			        ACTUAL_HEIGHT(win->children[count])) ;
		}

		if( sum < win->disp->width * win->disp->height * 0.9)
		{
			return  0 ;
		}
	}

	return  1 ;
}
#endif


/* --- global functions --- */

int
x_window_init(
	x_window_t *  win ,
	u_int  width ,
	u_int  height ,
	u_int  min_width ,
	u_int  min_height ,
	u_int  width_inc ,
	u_int  height_inc ,
	u_int  hmargin ,
	u_int  vmargin ,
	int  create_gc ,
	int  inputtable
	)
{
	memset( win , 0 , sizeof( x_window_t)) ;

	/* If wall picture is set, scrollable will be 0. */
	win->is_scrollable = 1 ;

	win->is_focused = 1 ;
	win->inputtable = inputtable ;
	win->is_mapped = 1 ;

	win->create_gc = create_gc ;

	win->width = width ;
	win->height = height ;
	win->min_width = min_width ;
	win->min_height = min_height ;
	win->width_inc = width_inc ;
	win->height_inc = height_inc ;
	win->hmargin = 0 /* hmargin */ ;
	win->vmargin = 0 /* vmargin */ ;

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
x_window_add_event_mask(
	x_window_t *  win ,
	long  event_mask
	)
{
	win->event_mask |= event_mask ;

	return  1 ;
}

int
x_window_remove_event_mask(
	x_window_t *  win ,
	long  event_mask
	)
{
	win->event_mask &= ~event_mask ;

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
	Pixmap  pic ,
	int  do_expose
	)
{
	u_int  count ;

	win->wall_picture = pic ;
	win->is_scrollable = 0 ;

	if( do_expose)
	{
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
	}

	for( count = 0 ; count < win->num_of_children ; count++)
	{
		x_window_set_wall_picture( win->children[count] , ParentRelative , do_expose) ;
	}

	return  1 ;
}

int
x_window_unset_wall_picture(
	x_window_t *  win ,
	int  do_expose
	)
{
	u_int  count ;

	win->wall_picture = None ;
	win->is_scrollable = 1 ;

	if( do_expose)
	{
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
	}

	for( count = 0 ; count < win->num_of_children ; count++)
	{
		x_window_unset_wall_picture( win->children[count] , do_expose) ;
	}

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
	child->x = x + win->hmargin ;
	child->y = y + win->vmargin ;
	child->is_focused = child->is_mapped = map ;

	win->children[ win->num_of_children ++] = child ;

	return  1 ;
}

int
x_window_remove_child(
	x_window_t *  win ,
	x_window_t *  child
	)
{
	u_int  count ;

	for( count = 0 ; count < win->num_of_children ; count++)
	{
		if( win->children[count] == child)
		{
			child->parent = NULL ;

			win->children[count] = win->children[--win->num_of_children] ;

			return  1 ;
		}
	}

	return  0 ;
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

	win->my_window = win ;	/* Note that x_connect_dialog.c uses this. */

	if( win->parent && ! win->parent->is_transparent &&
	    win->parent->wall_picture)
	{
		x_window_set_wall_picture( win , ParentRelative , 0) ;
	}

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
		win->is_mapped = 0 ;	/* XXX x_window_set_wall_picture() depends on this. */
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

		(*win->window_exposed)( win ,
			0 , 0 , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;
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
	if( ( flag & NOTIFY_TO_PARENT) && ! IS_IM_WINDOW(win))
	{
		if( win->parent)
		{
			win = win->parent ;
		}

		/*
		 * XXX
		 * If Font size, screen_{width|height}_ratio or vertical_mode is changed
		 * and x_window_resize( NOTIFY_TO_PARENT) is called, ignore this call and
		 * resize windows with display size.
		 */
		win->width = 0 ;
		return  x_window_resize_with_margin( win ,
				win->disp->width , win->disp->height ,
				NOTIFY_TO_MYSELF) ;
	}

	if( width + win->hmargin * 2 > win->disp->width)
	{
		width = win->disp->width - win->hmargin * 2 ;
	}

	if( height + win->vmargin * 2 > win->disp->height)
	{
		height = win->disp->height - win->vmargin * 2 ;
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
		 * Don't clear_margin_area() if flag == 0 because x_window_resize()
		 * is called before x_window_move() in x_im_*_screen.c and could
		 * cause segfault.
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
	return  x_window_resize( win , width - win->hmargin * 2 ,
			height - win->vmargin * 2 , flag) ;
}

int
x_window_set_normal_hints(
	x_window_t *  win ,
	u_int  min_width ,
	u_int  min_height ,
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
	if( win->parent)
	{
		x += win->parent->hmargin ;
		y += win->parent->vmargin ;
	}

	if( win->x == x && win->y == y)
	{
		return  0 ;
	}

	win->x = x ;
	win->y = y ;

	if( /* ! check_child_window_area( x_get_root_window( win)) || */
	    win->x + ACTUAL_WIDTH(win) > win->disp->width ||
	    win->y + ACTUAL_HEIGHT(win) > win->disp->height)
	{
		/*
		 * XXX Hack
		 * (Expect the caller to call x_window_resize() immediately after this.)
		 */
		return  1 ;
	}

	/*
	 * XXX
	 * Check if win is input method window or not, because x_window_move()
	 * can fall into the following infinite loop on framebuffer.
	 * 1) x_im_stat_screen::draw_screen() ->
	 *    x_window_move() ->
	 *    x_im_stat_screen::window_exposed() ->
	 *    x_im_stat_screen::draw_screen()
	 * 2) x_im_candidate_screen::draw_screen() ->
	 *    x_im_candidate_screen::resize() ->
	 *    x_window_move() ->
	 *    x_im_candidate_screen::window_exposed() ->
	 *    x_im_candidate_screen::draw_screen()
	 */
	if( ! IS_IM_WINDOW(win))
	{
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

		/* XXX */
		if( win->parent)
		{
			clear_margin_area( win->parent) ;
		}
	}

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
		Pixmap  pic ;
		int  src_x ;
		int  src_y ;

		if( win->wall_picture == ParentRelative)
		{
			src_x = x + win->x ;
			src_y = y + win->y ;
			pic = win->parent->wall_picture ;
		}
		else
		{
			pic = win->wall_picture ;
			src_x = x ;
			src_y = y ;
		}

		return  copy_area( win , pic , src_x , src_y , width , height , x , y , 1) ;
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
	u_int  width ,
	u_int  height
	)
{
	u_int  h ;
	int  fill_to_end ;

	if( ! win->is_mapped)
	{
		return  0 ;
	}

	if( height < LINE_HEIGHT || width < COL_WIDTH)
	{
		return  0 ;
	}

	if( ! IS_IM_WINDOW(win) &&
	    ( ! win->parent ||
	      win->x + ACTUAL_WIDTH(win) >= win->parent->width) &&
	    x + width >= win->width)
	{
		fill_to_end = 1 ;
	}
	else
	{
		fill_to_end = 0 ;
	}

	x += (win->x + win->hmargin) ;
	y += (win->y + win->vmargin) ;

	if( color->pixel < 0x8)
	{
		fprintf( win->disp->display->fp , "\x1b[%dm" , color->pixel + 40) ;
	}
	else if( color->pixel < 0x10)
	{
		fprintf( win->disp->display->fp , "\x1b[%dm" ,
			(color->pixel & ~ML_BOLD_COLOR_MASK) + 100) ;
	}
	else
	{
		fprintf( win->disp->display->fp , "\x1b[48;5;%dm" , color->pixel) ;
	}

#if  1
	for( h = 0 ; h < height ; h += LINE_HEIGHT)
	{
		fprintf( win->disp->display->fp , "\x1b[%d;%dH" ,
			(y + h) / LINE_HEIGHT + 1 ,
			x / COL_WIDTH + 1) ;

		if( fill_to_end)
		{
			fwrite( "\x1b[K" , 1 , 3 , win->disp->display->fp) ;
		}
		else
		{
			u_int  w ;

			for( w = 0 ; w < width ; w += COL_WIDTH)
			{
				fwrite( " " , 1 , 1 , win->disp->display->fp) ;
			}
		}
	}
#else
	fprintf( win->disp->display->fp , "\x1b[%d;%d;%d;%d$z" ,
		y / LINE_HEIGHT + 1 ,
		x / COL_WIDTH + 1 ,
		(y + height) / LINE_HEIGHT ,
		(x + width) / COL_WIDTH) ;
#endif

	fwrite( "\x1b[m" , 1 , 3 , win->disp->display->fp) ;
	fflush( win->disp->display->fp) ;

	return  1 ;
}

int
x_window_blank(
	x_window_t *  win
	)
{
	return  x_window_fill_with( win , &win->fg_color , 0 , 0 ,
			win->width - RIGHT_MARGIN(win) ,
			win->height - BOTTOM_MARGIN(win)) ;
}

int
x_window_update(
	x_window_t *  win ,
	int  flag
	)
{
	if( ! win->is_mapped)
	{
		return  0 ;
	}

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

	if( ! win->is_mapped)
	{
		return  0 ;
	}

	if( ! win->parent)
	{
		x_display_reset_cmap() ;
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
				event->xmotion.x -= win->hmargin ;
				event->xmotion.y -= win->vmargin ;

				(*win->button_motion)( win , &event->xmotion) ;
			}

			/* following button motion ... */

			win->prev_button_press_event.x = event->xmotion.x ;
			win->prev_button_press_event.y = event->xmotion.y ;
			win->prev_button_press_event.time = event->xmotion.time ;
		}
		else if( (win->event_mask & PointerMotionMask) && win->pointer_motion)
		{
			event->xmotion.x -= win->hmargin ;
			event->xmotion.y -= win->vmargin ;

			(*win->pointer_motion)( win , &event->xmotion) ;
		}
	}
	else if( event->type == ButtonRelease)
	{
		if( win->button_released)
		{
			event->xbutton.x -= win->hmargin ;
			event->xbutton.y -= win->vmargin ;

			(*win->button_released)( win , &event->xbutton) ;
		}

		win->button_is_pressing = 0 ;
	}
	else if( event->type == ButtonPress)
	{
		if( win->button_pressed)
		{
			event->xbutton.x -= win->hmargin ;
			event->xbutton.y -= win->vmargin ;

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

		if( ! win->is_focused && win->inputtable && event->xbutton.button == Button1 &&
		    ! event->xbutton.state)
		{
			x_window_set_input_focus( win) ;
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

#ifdef  __ANDROID__
	if( ch == 0)
	{
		return  x_display_get_str( seq , seq_len) ;
	}
#endif

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
				case  XK_KP_Begin:
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
	else if( *keysym == XK_Tab && (event->state & ShiftMask))
	{
		*keysym = XK_ISO_Left_Tab ;

		return  0 ;
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
	    ( win->disp->display->support_hmargin ||
	      win->disp->width == ACTUAL_WIDTH(win)) &&
	    ! IS_IM_WINDOW(win))
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
	return  copy_area( win , src , src_x , src_y , width , height , dst_x , dst_y , 0) ;
}

void
x_window_set_clip(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
}

void
x_window_unset_clip(
	x_window_t *  win
	)
{
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
	x_color_t *  bg_color ,	/* If NULL is specified, use wall_picture for bg */
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
	return  draw_string( win , font , fg_color , NULL , x , y , str , len , 1 , 0) ;
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
	return  draw_string( win , font , fg_color , NULL , x , y , str , len , 2 , 0) ;
}

int
x_window_draw_image_string(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	x_color_t *  bg_color ,	/* If NULL is specified, use wall_picture for bg */
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len
	)
{
	return  draw_string( win , font , fg_color , bg_color , x , y , str , len , 1 , 0) ;
}

int
x_window_draw_image_string16(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	x_color_t *  bg_color ,	/* If NULL is specified, use wall_picture for bg */
	int  x ,
	int  y ,
	XChar2b *  str ,
	u_int  len
	)
{
	return  draw_string( win , font , fg_color , bg_color , x , y , str , len , 2 , 0) ;
}

int
x_window_console_draw_string(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	x_color_t *  bg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len ,
	int  underline_style
	)
{
	return  draw_string( win , font , fg_color , bg_color , x , y , str , len , 1 ,
			underline_style) ;
}

int
x_window_console_draw_string16(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	x_color_t *  bg_color ,
	int  x ,
	int  y ,
	XChar2b *  str ,
	u_int  len ,
	int  underline_style
	)
{
	return  draw_string( win , font , fg_color , bg_color , x , y , str , len , 2 ,
			underline_style) ;
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
#ifdef  __ANDROID__
	if( win->utf_selection_requested)
	{
		(*win->utf_selection_requested)( win , NULL , 0) ;
	}
#else
	win->is_sel_owner = 1 ;
#endif

	return  1 ;
}

int
x_window_xct_selection_request(
	x_window_t *  win ,
	Time  time
	)
{
#ifdef  __ANDROID__
	x_display_request_text_selection() ;
#endif

	return  1 ;
}

int
x_window_utf_selection_request(
	x_window_t *  win ,
	Time  time
	)
{
#ifdef  __ANDROID__
	x_display_request_text_selection() ;
#endif

	return  1 ;
}

int
x_window_send_picture_selection(
	x_window_t *  win ,
	Pixmap  pixmap ,
	u_int  width ,
	u_int  height
	)
{
	return  0 ;
}

int
x_window_send_text_selection(
	x_window_t *  win ,
	XSelectionRequestEvent *  req_ev ,
	u_char *  sel_data ,
	size_t  sel_len ,
	Atom  sel_type
	)
{
#ifdef  __ANDROID__
	x_display_send_text_selection( sel_data , sel_len) ;
#endif

	return  1 ;
}

int
x_set_window_name(
	x_window_t *  win ,
	u_char *  name
	)
{
	fprintf( win->disp->display->fp , "\x1b]2;%s\x07" , name) ;

	return  1 ;
}

int
x_set_icon_name(
	x_window_t *  win ,
	u_char *  name
	)
{
	fprintf( win->disp->display->fp , "\x1b]1;%s\x07" , name) ;

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
	x_bel_mode_t  mode
	)
{
	if( mode & BEL_VISUAL)
	{
		x_window_blank( win) ;
		kik_usleep( 100000) ;	/* 100 mili sec */
		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
	}

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

void
x_window_set_input_focus(
	x_window_t *  win
	)
{
	reset_input_focus( x_get_root_window( win)) ;
	win->inputtable = win->is_focused = 1 ;
	if( win->window_focused)
	{
		(*win->window_focused)( win) ;
	}
}


/* for x_display.c */

int
x_window_clear_margin_area(
	x_window_t *  win
	)
{
	return  clear_margin_area( win) ;
}
