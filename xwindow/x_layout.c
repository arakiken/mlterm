/*
 *	$Id$
 */

#include  "x_layout.h"

#include  <kiklib/kik_types.h>		/* u_int */
#include  <kiklib/kik_str.h>


#define  SEPARATOR_WIDTH  1
#define  SCROLLBAR_WIDTH(scrollbar)  (ACTUAL_WIDTH(&(scrollbar).window) + SEPARATOR_WIDTH)


/* --- static functions --- */

static u_int
modify_separator(
	u_int  separator ,
	u_int  total ,
	u_int  min
	)
{
	if( total < separator + SEPARATOR_WIDTH + min)
	{
		return  total - min - SEPARATOR_WIDTH ;
	}
	else
	{
		return  separator ;
	}
}

static void
reset_layout(
	struct terminal *  term ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
	u_int  child_width ;
	u_int  child_height ;

	if( term->separator_x > 0)
	{
		term->separator_x = child_width =
			modify_separator( term->separator_x , width ,
				term->next[0]->screen->window.hmargin * 2 +
				x_col_width( term->next[0]->screen) +
				(term->next[0]->sb_mode != SBM_NONE ?
					SCROLLBAR_WIDTH(term->next[0]->scrollbar) : 0)) ;
	}
	else
	{
		child_width = width ;
	}

	if( term->separator_y > 0)
	{
		term->separator_y = child_height =
			modify_separator( term->separator_y , height ,
				term->next[1]->screen->window.vmargin * 2 +
				x_line_height( term->next[1]->screen)) ;
	}
	else
	{
		child_height = height ;
	}

	if( term->sb_mode != SBM_NONE)
	{
		int  sep_x ;
		int  do_redraw ;

		if( term->sb_mode == SBM_RIGHT)
		{
			x_window_move( &term->screen->window , x , y) ;
			do_redraw = x_window_move( &term->scrollbar.window ,
					x + child_width - ACTUAL_WIDTH(&term->scrollbar.window) ,
					y) ;
			sep_x = x + child_width - SCROLLBAR_WIDTH(term->scrollbar) ;
		}
		else
		{
			x_window_move( &term->screen->window ,
					x + SCROLLBAR_WIDTH(term->scrollbar) , y) ;
			do_redraw = x_window_move( &term->scrollbar.window , x , y) ;
			sep_x = x + ACTUAL_WIDTH( &term->scrollbar.window) ;
		}

		x_window_resize_with_margin( &term->screen->window ,
			child_width - SCROLLBAR_WIDTH(term->scrollbar) ,
			child_height , NOTIFY_TO_MYSELF) ;
		x_window_resize_with_margin( &term->scrollbar.window ,
			ACTUAL_WIDTH(&term->scrollbar.window) ,
			child_height , NOTIFY_TO_MYSELF) ;

	#if  defined(USE_FRAMEBUFFER)
		x_window_fill( &X_SCREEN_TO_LAYOUT(term->screen)->window ,
				sep_x , y , SEPARATOR_WIDTH , child_height) ;
	#elif  defined(USE_WIN32GUI)
		/*
		 * Scrollbar 3 isn't clearly redrawn without this.
		 *
		 *  1              2       3
		 * +-+------------+-+-----+-+----+
		 * | |$ hresize_sc| |$    | |$   |
		 * | |reen +1     | |     | |    |
		 * | |            | |     | |    |
		 */
		if( do_redraw)
		{
			x_window_update_all( &term->scrollbar.window) ;
		}
	#endif
	}
	else
	{
		x_window_move( &term->screen->window , x , y) ;
		x_window_resize_with_margin( &term->screen->window ,
			child_width , child_height , NOTIFY_TO_MYSELF) ;
	}

	if( term->yfirst)
	{
		if( term->next[1] && term->separator_y > 0)
		{
			reset_layout( term->next[1] , x , y + term->separator_y + SEPARATOR_WIDTH ,
				width , height - term->separator_y - SEPARATOR_WIDTH) ;
		#ifdef  USE_FRAMEBUFFER
			x_window_fill( &X_SCREEN_TO_LAYOUT(term->screen)->window ,
				x , y + term->separator_y ,
				width , SEPARATOR_WIDTH) ;
		#endif
		}

		if( term->next[0] && term->separator_x > 0)
		{
			reset_layout( term->next[0] , x + term->separator_x + SEPARATOR_WIDTH , y ,
				width - term->separator_x - SEPARATOR_WIDTH , child_height) ;
		#ifdef  USE_FRAMEBUFFER
			x_window_fill( &X_SCREEN_TO_LAYOUT(term->screen)->window ,
				x + term->separator_x , y ,
				SEPARATOR_WIDTH , child_height) ;
		#endif
		}
	}
	else
	{
		if( term->next[0] && term->separator_x > 0)
		{
			reset_layout( term->next[0] , x + term->separator_x + SEPARATOR_WIDTH , y ,
				width - term->separator_x - SEPARATOR_WIDTH , height) ;
		#ifdef  USE_FRAMEBUFFER
			x_window_fill( &X_SCREEN_TO_LAYOUT(term->screen)->window ,
				x + term->separator_x , y ,
				SEPARATOR_WIDTH , height) ;
		#endif
		}

		if( term->next[1] && term->separator_y > 0)
		{
			reset_layout( term->next[1] , x , y + term->separator_y + SEPARATOR_WIDTH ,
				child_width , height - term->separator_y - SEPARATOR_WIDTH) ;
		#ifdef  USE_FRAMEBUFFER
			x_window_fill( &X_SCREEN_TO_LAYOUT(term->screen)->window ,
				x , y + term->separator_y ,
				child_width , SEPARATOR_WIDTH) ;
		#endif
		}
	}
}

static struct terminal *
search_term(
	struct terminal *  term ,
	x_screen_t *  screen
	)
{
	struct terminal *  hit ;

	if( term->screen == screen)
	{
		return  term ;
	}

	if( ( term->next[0] && ( hit = search_term( term->next[0] , screen))) ||
	    ( term->next[1] && ( hit = search_term( term->next[1] , screen))))
	{
		return  hit ;
	}

	return  NULL ;
}

static struct terminal *
search_parent_term(
	struct terminal *  term ,
	struct terminal *  child
	)
{
	struct terminal *  parent ;

	if( term->next[0] == child || term->next[1] == child)
	{
		return  term ;
	}

	if( ( term->next[0] && ( parent = search_parent_term( term->next[0] , child))) ||
	    ( term->next[1] && ( parent = search_parent_term( term->next[1] , child))))
	{
		return  parent ;
	}

	return  NULL ;
}

static x_screen_t *
search_next_screen(
	struct terminal *  term ,
	x_screen_t *  screen ,
	int *  found
	)
{
	x_screen_t *  hit ;
	int  idx ;
	int  count ;

	if( term->screen == screen)
	{
		*found = 1 ;
	}

	if( term->yfirst)
	{
		idx = 0 ;
	}
	else
	{
		idx = 1 ;
	}

	for( count = 0 ; count < 2 ; count++)
	{
		if( term->next[idx])
		{
			if( *found)
			{
				return  term->next[idx]->screen ;
			}
			else if( ( hit = search_next_screen( term->next[idx] , screen , found)))
			{
				return  hit ;
			}
		}

		idx = ! idx ;
	}

	return  NULL ;
}

static x_screen_t *
search_prev_screen(
	struct terminal *  term ,
	x_screen_t *  screen ,
	x_screen_t **  prev
	)
{
	int  idx ;
	int  count ;

	if( *prev && term->screen == screen)
	{
		return  *prev ;
	}

	*prev = term->screen ;

	if( term->yfirst)
	{
		idx = 0 ;
	}
	else
	{
		idx = 1 ;
	}

	for( count = 0 ; count < 2 ; count++)
	{
		if( term->next[idx] && search_prev_screen( term->next[idx] , screen , prev))
		{
			return  *prev ;
		}

		idx = ! idx ;
	}

	return  NULL ;
}

static struct terminal *
get_current_term(
	struct terminal *  term
	)
{
	struct terminal *  current ;

	if( term->screen->window.inputtable > 0)
	{
		return  term ;
	}

	if( ( term->next[0] && ( current = get_current_term( term->next[0]))) ||
	    ( term->next[1] && ( current = get_current_term( term->next[1]))))
	{
		return  current ;
	}

	return  NULL ;
}

static x_window_t *
get_current_window(
	x_layout_t *  layout
	)
{
	struct terminal *  term ;

	if( ( term = get_current_term( &layout->term)))
	{
		return  &term->screen->window ;
	}
	else
	{
		return  &layout->term.screen->window ;
	}
}

static u_int
get_separator_x(
	x_screen_t *  screen ,
	u_int  sb_width ,
	u_int  percent		/* < 100 */
	)
{
	u_int  width ;
	u_int  fixed_width ;
	u_int  sep_x ;

	width = screen->window.width ;
	fixed_width = screen->window.hmargin * 2 + sb_width ;

	sep_x = (width + fixed_width) * percent / 100 ;
	if( sep_x < fixed_width + x_col_width( screen))
	{
		return  0 ;
	}

#if  defined(USE_FRAMEBUFFER) && ! defined(__ANDROID__)
	if( screen->window.disp->display->pixels_per_byte > 1)
	{
		return  sep_x - sep_x % screen->window.disp->display->pixels_per_byte -
			SEPARATOR_WIDTH ;
	}
	else
#endif
	{
		return  sep_x - (sep_x - fixed_width) % x_col_width( screen) ;
	}
}

static u_int
get_separator_y(
	x_screen_t *  screen ,
	u_int  percent		/* < 100 */
	)
{
	u_int  height ;
	u_int  fixed_height ;
	u_int  sep_y ;

	height = screen->window.height ;
	fixed_height = screen->window.vmargin * 2 ;

	sep_y = (height + fixed_height) * percent / 100 ;
	if( sep_y < fixed_height + x_line_height( screen))
	{
		return  0 ;
	}

	return  sep_y - (sep_y - fixed_height) % x_line_height( screen) ;
}


/*
 * callbacks of x_window_t events.
 */

static void
window_finalized(
	x_window_t *  win
	)
{
	x_layout_t *  layout ;

	layout = (x_layout_t*) win ;

	x_layout_delete( layout) ;
}

static void
window_resized(
	x_window_t *  win
	)
{
	x_layout_t *  layout ;

	layout = (x_layout_t*) win ;

	reset_layout( &layout->term , 0 , 0 , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;
}

static void
child_window_resized(
	x_window_t *  win ,
	x_window_t *  child
	)
{
	x_layout_t *  layout ;
	u_int  actual_width ;

	layout = (x_layout_t*) win ;

	if( layout->term.next[0] || layout->term.next[1])
	{
		reset_layout( &layout->term , 0 , 0 ,
			ACTUAL_WIDTH(&layout->window) ,
			ACTUAL_HEIGHT(&layout->window)) ;

		return ;
	}

	if( &layout->term.screen->window == child)
	{
		if( layout->term.sb_mode == SBM_NONE)
		{
			actual_width = ACTUAL_WIDTH(child) ;
		}
		else
		{
			actual_width = ACTUAL_WIDTH(child) +
					ACTUAL_WIDTH( &layout->term.scrollbar.window) +
					SEPARATOR_WIDTH ;
		}

		x_window_resize_with_margin( &layout->window ,
			actual_width , ACTUAL_HEIGHT(child) , NOTIFY_TO_NONE) ;

		x_window_resize_with_margin( &layout->term.scrollbar.window ,
			ACTUAL_WIDTH( &layout->term.scrollbar.window) ,
			ACTUAL_HEIGHT(child) , NOTIFY_TO_MYSELF) ;

		if( layout->term.sb_mode == SBM_RIGHT)
		{
			x_window_move( &layout->term.scrollbar.window ,
				ACTUAL_WIDTH( &layout->term.screen->window) + SEPARATOR_WIDTH ,
				0) ;
		}
	}
	else if( &layout->term.scrollbar.window == child)
	{
		if( layout->term.sb_mode == SBM_NONE)
		{
			return ;
		}

		x_window_resize_with_margin( &layout->window ,
			ACTUAL_WIDTH(child) + ACTUAL_WIDTH( &layout->term.screen->window) +
			SEPARATOR_WIDTH , ACTUAL_HEIGHT(child) , NOTIFY_TO_NONE) ;

		if( layout->term.sb_mode == SBM_LEFT)
		{
			x_window_move( &layout->term.screen->window ,
				ACTUAL_WIDTH( &layout->term.scrollbar.window) +
					SEPARATOR_WIDTH ,
				0) ;
		}
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" illegal child. this event should be invoken only by screen.\n") ;
	#endif
	}
}

static void
window_exposed(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
	x_window_fill( win , x , y , width , height) ;
}

static void
key_pressed(
	x_window_t *  win ,
	XKeyEvent *  event
	)
{
	x_layout_t *  layout ;
	x_window_t *  child ;

	layout = (x_layout_t*) win ;
	child = get_current_window( layout) ;

	/* dispatch to screen */
	(*child->key_pressed)( child , event) ;
}

static void
utf_selection_notified(
	x_window_t *  win ,
	u_char *  buf ,
	size_t  len
	)
{
	x_layout_t *  layout ;
	x_window_t *  child ;

	layout = (x_layout_t*) win ;
	child = get_current_window( layout) ;

	/* dispatch to screen */
	(*child->utf_selection_notified)( child , buf , len) ;
}

static void
xct_selection_notified(
	x_window_t *  win ,
	u_char *  buf ,
	size_t  len
	)
{
	x_layout_t *  layout ;
	x_window_t *  child ;

	layout = (x_layout_t*) win ;
	child = get_current_window( layout) ;

	/* dispatch to screen */
	(*child->xct_selection_notified)( child , buf , len) ;
}

#ifndef  DISABLE_XDND
static void
set_xdnd_config(
	x_window_t *  win ,
	char *  dev ,
	char *  buf ,
	char *  value
	)
{
	x_layout_t *  layout ;
	x_window_t *  child ;

	layout = (x_layout_t*) win ;
	child = get_current_window( layout) ;

	/* dispatch to screen */
	(*child->set_xdnd_config)( child , dev, buf , value) ;
}
#endif

static void
window_deleted_intern(
	struct terminal *  term
	)
{
	/* dispatch to screen */
	(*term->screen->window.window_deleted)( &term->screen->window) ;

	if( term->next[0])
	{
		window_deleted_intern( term->next[0]) ;
	}

	if( term->next[1])
	{
		window_deleted_intern( term->next[1]) ;
	}
}

static void
window_deleted(
	x_window_t *  win
	)
{
	x_layout_t *  layout ;

	layout = (x_layout_t*) win ;

	window_deleted_intern( &layout->term) ;
}


#if  0
/*
 * Overriding methods of x_scrollbar_t.
 */

static void
sb_key_pressed(
	x_window_t *  win ,
	XKeyEvent *  event
	)
{
	struct terminal  term ;
	x_screen_t **  screen ;

	screen = ((void*)win) - (((void*)&term.scrollbar) - ((void*)&term.screen)) ;

	/* dispatch to screen */
	(*(*screen)->window.key_pressed)( &(*screen)->window , event) ;
}
#endif


/*
 * Overriding methods of ml_screen_listener_t of x_screen_t.
 */
 
static void
line_scrolled_out(
	void *  p		/* must be x_screen_t(, or child of x_layout_t) */
	)
{
	x_layout_t *  layout ;
	struct terminal *  term ;

	layout = X_SCREEN_TO_LAYOUT( (x_screen_t*)p) ;

	(*layout->line_scrolled_out)( p) ;

	if( ( term = search_term( &layout->term , p)))
	{
		if( ml_term_log_size_is_unlimited( ((x_screen_t*)p)->term))
		{
			x_scrollbar_set_num_of_log_lines( &term->scrollbar ,
				ml_term_get_log_size( ((x_screen_t*)p)->term)) ;
		}

		x_scrollbar_line_is_added( &term->scrollbar) ;
	}
}


/*
 * callbacks of x_sb_event_listener_t events.
 */
 
static int
screen_scroll_to(
	void *  p ,
	int  row
	)
{
	struct terminal *  term ;

	term = p ;

	x_screen_scroll_to( term->screen , row) ;

	return  1 ;
}

static int
screen_scroll_upward(
	void *  p ,
	u_int  size
	)
{
	struct terminal *  term ;

	term = p ;

	x_screen_scroll_upward( term->screen , size) ;

	return  1 ;
}

static int
screen_scroll_downward(
	void *  p ,
	u_int  size
	)
{
	struct terminal *  term ;

	term = p ;

	x_screen_scroll_downward( term->screen , size) ;

	return  1 ;
}

static int
screen_is_static(
	void *  p
	)
{
	struct terminal *  term ;

	term = p ;

	if( ml_term_is_backscrolling( term->screen->term) == BSM_STATIC)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}


/*
 * callbacks of x_screen_scroll_event_listener_t events.
 */

static void
bs_mode_exited(
	void *  p
	)
{
	struct terminal *  term ;

	term = p ;

	x_scrollbar_reset( &term->scrollbar) ;
}

static void
scrolled_upward(
	void *  p ,
	u_int  size
	)
{
	struct terminal *  term ;

	term = p ;

	x_scrollbar_move_downward( &term->scrollbar , size) ;
}

static void
scrolled_downward(
	void *  p ,
	u_int  size
	)
{
	struct terminal *  term ;

	term = p ;

	x_scrollbar_move_upward( &term->scrollbar , size) ;
}

static void
scrolled_to(
	void *  p ,
	int  row
	)
{
	struct terminal *  term ;

	term = p ;

	x_scrollbar_move( &term->scrollbar , row) ;
}

static void
log_size_changed(
	void *  p ,
	u_int  log_size
	)
{
	struct terminal *  term ;

	term = p ;

	x_scrollbar_set_num_of_log_lines( &term->scrollbar , log_size) ;
}

static void
line_height_changed(
	void *  p ,
	u_int  line_height
	)
{
	struct terminal *  term ;

	term = p ;

	x_scrollbar_set_line_height( &term->scrollbar , line_height) ;
}

static void
change_fg_color(
	void *  p ,
	char *  color
	)
{
	struct terminal *  term ;

	term = p ;

	x_scrollbar_set_fg_color( &term->scrollbar , color) ;
}

static char *
get_fg_color(
	void *  p
	)
{
	struct terminal *  term ;

	term = p ;

	return  term->scrollbar.fg_color ;
}

static void
change_bg_color(
	void *  p ,
	char *  color
	)
{
	struct terminal *  term ;

	term = p ;

	x_scrollbar_set_bg_color( &term->scrollbar , color) ;
}

static char *
get_bg_color(
	void *  p
	)
{
	struct terminal *  term ;

	term = p ;

	return  term->scrollbar.bg_color ;
}

static void
change_view(
	void *  p ,
	char *  name
	)
{
	struct terminal *  term ;

	term = p ;

	x_scrollbar_change_view( &term->scrollbar , name) ;
}

static char *
get_view_name(
	void *  p
	)
{
	struct terminal *  term ;

	term = p ;

	return  term->scrollbar.view_name ;
}

static void
transparent_state_changed(
	void *  p ,
	int  is_transparent ,
	x_picture_modifier_t *  pic_mod
	)
{
	struct terminal *  term ;

	term = p ;

	if( is_transparent == 1)
	{
		x_scrollbar_set_transparent( &term->scrollbar , pic_mod , 1) ;
	}
	else
	{
		x_scrollbar_unset_transparent( &term->scrollbar) ;
	}
}

static x_sb_mode_t
sb_mode(
	void *  p
	)
{
	struct terminal *  term ;

	term = p ;
	
	return  term->sb_mode ;
}

static u_int
total_width(
	struct terminal *  term
	)
{
	u_int  width ;

	width = ACTUAL_WIDTH( &term->screen->window) ;

	if( term->sb_mode != SBM_NONE)
	{
		width += (ACTUAL_WIDTH( &term->scrollbar.window) + SEPARATOR_WIDTH) ;
	}

	if( term->separator_x > 0 && term->next[0])
	{
		width += total_width( term->next[0]) ;
	}

	return  width ;
}

static u_int
total_height(
	struct terminal *  term
	)
{
	u_int  height ;

	height = ACTUAL_HEIGHT( &term->screen->window) ;

	if( term->separator_y > 0 && term->next[1])
	{
		height += total_height( term->next[1]) ;
	}

	return  height ;
}

/* XXX */
static void
total_min_size(
	struct terminal *  term ,
	u_int *  width ,
	u_int *  height
	)
{
	if( term->sb_mode != SBM_NONE)
	{
		*width += SEPARATOR_WIDTH ;
	}

	if( term->next[0])
	{
		*width += SEPARATOR_WIDTH ;

		total_min_size( term->next[0] , width , height) ;
	}

	if( term->next[1])
	{
		*height += SEPARATOR_WIDTH ;

		total_min_size( term->next[1] , width , height) ;
	}
}

static void
update_normal_hints(
	x_layout_t *  layout
	)
{
	u_int  min_width = 0 ;
	u_int  min_height = 0 ;

	total_min_size( &layout->term , &min_width , &min_height) ;

	x_window_set_normal_hints( &layout->window , min_width , min_height , 0 , 0) ;
}

static void
change_sb_mode(
	void *  p ,
	x_sb_mode_t  new_mode
	)
{
	struct terminal *  term ;
	x_sb_mode_t  old_mode ;

	term = p ;

	if( (old_mode = term->sb_mode) == new_mode)
	{
		return ;
	}

	/*
	 * term->sb_mode should be changed before x_window_unmap/x_window_map
	 * for framebuffer. (see fb/x_window.c)
	 */
	term->sb_mode = new_mode ;

	if( new_mode == SBM_NONE)
	{
		x_window_unmap( &term->scrollbar.window) ;

	#ifndef  USE_FRAMEBUFFER
		/* The screen size is changed to the one without scrollbar. */
		x_window_resize_with_margin( &X_SCREEN_TO_LAYOUT(term->screen)->window ,
			total_width( &X_SCREEN_TO_LAYOUT(term->screen)->term) ,
			total_height( &X_SCREEN_TO_LAYOUT(term->screen)->term) ,
			NOTIFY_TO_MYSELF) ;
	#endif

		update_normal_hints( X_SCREEN_TO_LAYOUT( term->screen)) ;

	#ifdef  USE_FRAMEBUFFER
		/*
		 * The screen size is not changed.
		 * As a result the size of term->screen gets larger.
		 */
		window_resized( &X_SCREEN_TO_LAYOUT(term->screen)->window) ;
	#endif
	}
	else
	{
		if( old_mode == SBM_NONE)
		{
			x_window_map( &term->scrollbar.window) ;

		#ifndef  USE_FRAMEBUFFER
			/* The screen size is changed to the one with scrollbar. */
			x_window_resize_with_margin( &X_SCREEN_TO_LAYOUT(term->screen)->window ,
				total_width( &X_SCREEN_TO_LAYOUT(term->screen)->term) ,
				total_height( &X_SCREEN_TO_LAYOUT(term->screen)->term) ,
				NOTIFY_TO_MYSELF) ;
		#else
			/*
			 * The screen size is not changed.
			 * As a result the size of term->screen gets smaller.
			 */
			window_resized( &X_SCREEN_TO_LAYOUT(term->screen)->window) ;
		#endif

			update_normal_hints( X_SCREEN_TO_LAYOUT( term->screen)) ;
		}
		else
		{
			reset_layout( &X_SCREEN_TO_LAYOUT( term->screen)->term , 0 , 0 ,
				ACTUAL_WIDTH( &X_SCREEN_TO_LAYOUT( term->screen)->window) ,
				ACTUAL_HEIGHT( &X_SCREEN_TO_LAYOUT( term->screen)->window)) ;
		}
	}
}

static void
term_changed(
	void *  p ,
	u_int  log_size ,
	u_int  logged_lines
	)
{
	struct terminal *  term ;

	term = p ;

	x_scrollbar_set_num_of_log_lines( &term->scrollbar , log_size) ;
	x_scrollbar_set_num_of_filled_log_lines( &term->scrollbar , logged_lines) ;
}

static void
delete_term(
	struct terminal *  term
	)
{
	x_scrollbar_final( &term->scrollbar) ;

	if( term->next[0])
	{
		delete_term( term->next[0]) ;
		free( term->next[0]) ;
	}

	if( term->next[1])
	{
		delete_term( term->next[1]) ;
		free( term->next[1]) ;
	}
}


/* --- global functions --- */

x_layout_t *
x_layout_new(
	x_screen_t *  screen ,
	char *  view_name ,
	char *  fg_color ,
	char *  bg_color ,
	x_sb_mode_t  mode
	)
{
	x_layout_t *  layout ;
	u_int  actual_width ;
	u_int  min_width ;
	int  screen_x ;
	int  sb_x ;
	int  sb_map ;
	
	if( ( layout = calloc( 1 , sizeof( x_layout_t))) == NULL)
	{
		return  NULL ;
	}

	/*
	 * event callbacks.
	 */
	layout->term.sb_listener.self = &layout->term ;
	layout->term.sb_listener.screen_scroll_to = screen_scroll_to ;
	layout->term.sb_listener.screen_scroll_upward = screen_scroll_upward ;
	layout->term.sb_listener.screen_scroll_downward = screen_scroll_downward ;
	layout->term.sb_listener.screen_is_static = screen_is_static ;

	if( x_scrollbar_init( &layout->term.scrollbar , &layout->term.sb_listener ,
		view_name , fg_color , bg_color , ACTUAL_HEIGHT( &screen->window) ,
		x_line_height( screen) , ml_term_get_log_size( screen->term) ,
		ml_term_get_num_of_logged_lines( screen->term) ,
		screen->window.is_transparent ,
		x_screen_get_picture_modifier( screen)) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_scrollbar_init() failed.\n") ;
	#endif

		goto  error ;
	}
#if  0
	layout->term.scrollbar.window.key_pressed = sb_key_pressed ;
#endif
	layout->term.screen = screen ;

	/*
	 * event callbacks.
	 */
	layout->term.screen_scroll_listener.self = &layout->term ;
	layout->term.screen_scroll_listener.bs_mode_entered = NULL ;
	layout->term.screen_scroll_listener.bs_mode_exited = bs_mode_exited ;
	layout->term.screen_scroll_listener.scrolled_upward = scrolled_upward ;
	layout->term.screen_scroll_listener.scrolled_downward = scrolled_downward ;
	layout->term.screen_scroll_listener.scrolled_to = scrolled_to ;
	layout->term.screen_scroll_listener.log_size_changed = log_size_changed ;
	layout->term.screen_scroll_listener.line_height_changed = line_height_changed ;
	layout->term.screen_scroll_listener.change_fg_color = change_fg_color ;
	layout->term.screen_scroll_listener.fg_color = get_fg_color ;
	layout->term.screen_scroll_listener.change_bg_color = change_bg_color ;
	layout->term.screen_scroll_listener.bg_color = get_bg_color ;
	layout->term.screen_scroll_listener.change_view = change_view ;
	layout->term.screen_scroll_listener.view_name = get_view_name ;
	layout->term.screen_scroll_listener.transparent_state_changed = transparent_state_changed ;
	layout->term.screen_scroll_listener.sb_mode = sb_mode ;
	layout->term.screen_scroll_listener.change_sb_mode = change_sb_mode ;
	layout->term.screen_scroll_listener.term_changed = term_changed ;

	x_set_screen_scroll_listener( screen ,
		&layout->term.screen_scroll_listener) ;

	layout->line_scrolled_out = screen->screen_listener.line_scrolled_out ;
	screen->screen_listener.line_scrolled_out = line_scrolled_out ;

	layout->term.sb_mode = mode ;

	if( layout->term.sb_mode == SBM_NONE)
	{
		actual_width = ACTUAL_WIDTH( &screen->window) ;
		
		min_width = 0 ;
	}
	else
	{
		actual_width = ACTUAL_WIDTH( &screen->window) +
		               SCROLLBAR_WIDTH( layout->term.scrollbar) ;

		min_width = SEPARATOR_WIDTH ;
	}

	if( x_window_init( &layout->window ,
		actual_width , ACTUAL_HEIGHT( &screen->window) ,
		min_width , 0 , 0 , 0 , 0 , 0 , 0 , 0) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_window_init() failed.\n") ;
	#endif

		goto  error ;
	}

	if( layout->term.sb_mode == SBM_RIGHT)
	{
		screen_x = 0 ;
		sb_x = ACTUAL_WIDTH( &screen->window) + SEPARATOR_WIDTH ;
		sb_map = 1 ;
	}
	else if( layout->term.sb_mode == SBM_LEFT)
	{
		screen_x = ACTUAL_WIDTH( &layout->term.scrollbar.window) + SEPARATOR_WIDTH ;
		sb_x = 0 ;
		sb_map = 1 ;
	}
	else /* if( layout->term.sb_mode == SBM_NONE) */
	{
		screen_x = sb_x = 0 ;	/* overlaying scrollbar window */
		sb_map = 0 ;
	}

	if( ! x_window_add_child( &layout->window , &layout->term.scrollbar.window ,
		 sb_x , 0 , sb_map) ||
	    ! x_window_add_child( &layout->window , &screen->window , screen_x , 0 , 1))
	{
		goto  error ;
	}

	/*
	 * event call backs.
	 */
	x_window_init_event_mask( &layout->window , KeyPressMask) ;
	layout->window.window_finalized = window_finalized ;
	layout->window.window_resized = window_resized ;
	layout->window.child_window_resized = child_window_resized ;
	layout->window.window_exposed = window_exposed ;
	layout->window.key_pressed = key_pressed ;
	layout->window.utf_selection_notified = utf_selection_notified ;
	layout->window.xct_selection_notified = xct_selection_notified ;
	layout->window.window_deleted = window_deleted ;
#ifndef  DISABLE_XDND
	layout->window.set_xdnd_config = set_xdnd_config ;
#endif
	return  layout ;

error:
	free( layout) ;

	return  NULL ;
}

int
x_layout_delete(
	x_layout_t *  layout
	)
{
	delete_term( &layout->term) ;
	free( layout) ;

	return  1 ;
}

int
x_layout_add_child(
	x_layout_t *  layout ,
	x_screen_t *  screen ,
	int  horizontal ,
	const char *  sep_str	/* "XX%" or "XX" */
	)
{
	struct terminal *  next ;
	struct terminal *  term ;
	u_int  sep = 50 ;
	int  is_percent = 1 ;

	if( sep_str)
	{
		char *  p ;

		if( ( p = strchr( sep_str , '%')))
		{
			if( ! kik_str_n_to_uint( &sep , sep_str , p - sep_str) ||
			    sep >= 100 || sep == 0)
			{
				return  0 ;
			}
		}
		else
		{
			if( ! kik_str_to_uint( &sep , sep_str) || sep == 0)
			{
				return  0 ;
			}

			is_percent = 0 ;
		}
	}

	if( ! ( next = calloc( 1 , sizeof(struct terminal))))
	{
		return  0 ;
	}

	if( ! ( term = get_current_term( &layout->term)))
	{
		term = &layout->term ;
	}

	if( horizontal)
	{
		u_int  orig_separator_x ;
		u_int  sb_width ;

		orig_separator_x = term->separator_x ;

		if( term->sb_mode != SBM_NONE)
		{
			sb_width = SCROLLBAR_WIDTH( term->scrollbar) ;
		}
		else
		{
			sb_width = 0 ;
		}

		if( is_percent ?
			( ( term->separator_x = get_separator_x( term->screen , sb_width ,
							sep)) == 0) :
			( ( term->separator_x = x_col_width( term->screen) * sep + sb_width) >=
				ACTUAL_WIDTH(&term->screen->window) + sb_width))
		{
			term->separator_x = orig_separator_x ;
			free( next) ;

			return  0 ;
		}

		next->next[0] = term->next[0] ;
		term->next[0] = next ;

		if( orig_separator_x > 0)
		{
			next->separator_x = orig_separator_x - term->separator_x ;
		}
	}
	else
	{
		u_int  orig_separator_y ;

		orig_separator_y = term->separator_y ;

		if( is_percent ?
			( ( term->separator_y = get_separator_y( term->screen , sep)) == 0) :
			( ( term->separator_y = x_line_height( term->screen) * sep) >=
				ACTUAL_HEIGHT(&term->screen->window)))
		{
			term->separator_y = orig_separator_y ;
			free( next) ;

			return  0 ;
		}

		if( term->separator_x == 0)
		{
			term->yfirst = 1 ;
		}

		next->next[1] = term->next[1] ;
		term->next[1] = next ;

		if( orig_separator_y > 0)
		{
			next->separator_y = orig_separator_y - term->separator_y ;
		}
	}

	next->sb_listener = term->sb_listener ;
	next->sb_listener.self = next ;

	x_scrollbar_init( &next->scrollbar ,
		&next->sb_listener , term->scrollbar.view_name ,
		term->scrollbar.fg_color , term->scrollbar.bg_color ,
		ACTUAL_HEIGHT( &screen->window) ,
		x_line_height( screen) , ml_term_get_log_size( screen->term) ,
		ml_term_get_num_of_logged_lines( screen->term) ,
		screen->window.is_transparent ,
		x_screen_get_picture_modifier( screen)) ;
#if  0
	next->scrollbar.window.key_pressed = sb_key_pressed ;
#endif
	x_window_add_child( &layout->window , &next->scrollbar.window , 0 , 0 , 0) ;
	x_window_show( &next->scrollbar.window , 0) ;

	next->screen_scroll_listener = term->screen_scroll_listener ;
	next->screen_scroll_listener.self = next ;

	next->screen = screen ;
	x_set_screen_scroll_listener( screen , &next->screen_scroll_listener) ;
	screen->screen_listener.line_scrolled_out = line_scrolled_out ;
	x_window_add_child( &layout->window , &screen->window , 0 , 0 , 0) ;
	x_window_show( &screen->window , 0) ;

	next->sb_mode = term->sb_mode ;

	reset_layout( &layout->term , 0 , 0 , ACTUAL_WIDTH(&layout->window) ,
		ACTUAL_HEIGHT(&layout->window)) ;

	if( term->sb_mode != SBM_NONE)
	{
		x_window_map( &next->scrollbar.window) ;
	}
	x_window_map( &screen->window) ;

	update_normal_hints( layout) ;

	return  1 ;
}

int
x_layout_remove_child(
	x_layout_t *  layout ,
	x_screen_t *  screen
	)
{
	struct terminal *  term ;
	int  idx2 ;

	if( layout->term.next[0] == NULL &&
	    layout->term.next[1] == NULL)
	{
		return  0 ;
	}

	term = search_term( &layout->term , screen) ;

	x_scrollbar_final( &term->scrollbar) ;
	x_window_unmap( &term->scrollbar.window) ;
	x_window_final( &term->scrollbar.window) ;

	if( term->yfirst)
	{
		idx2 = 0 ;
	}
	else
	{
		idx2 = 1 ;
	}

	if( ! term->next[idx2])
	{
		idx2 = ! idx2 ;
	}

	if( term == &layout->term)
	{
		struct terminal *  next ;

		if( idx2 == 0)
		{
			if( term->separator_y > 0)
			{
				term->next[0]->separator_y = term->separator_y ;
			}
		}
		else
		{
			if( term->separator_x > 0)
			{
				term->next[1]->separator_x = term->separator_x ;
			}
		}
		term->next[idx2]->yfirst = term->yfirst ;

		next = term->next[idx2] ;

		if( term->next[!idx2])
		{
			struct terminal *  parent ;

			parent = search_parent_term( next , NULL) ;
			if( parent->next[!idx2] == NULL)
			{
				parent->next[!idx2] = term->next[!idx2] ;
			}
			else
			{
				parent->next[idx2] = term->next[!idx2] ;
			}
		}

		*term = *next ;
		term->screen_scroll_listener.self = term ;
		x_set_screen_scroll_listener( term->screen , &term->screen_scroll_listener) ;
		term->sb_listener.self = term ;
		term->scrollbar.sb_listener = &term->sb_listener ;
		term->scrollbar.view->win = &term->scrollbar.window ;

		term = next ;
	}
	else
	{
		struct terminal *  parent ;
		int  idx ;

		parent = search_parent_term( &layout->term , term) ;

		if( parent->next[0] == term)
		{
			idx = 0 ;
		}
		else
		{
			idx = 1 ;
		}

		if( term->next[idx2])
		{
			if( idx2 == 0)
			{
				if( term->separator_y > 0)
				{
					term->next[0]->separator_y = term->separator_y ;
				}
			}
			else
			{
				if( term->separator_x > 0)
				{
					term->next[1]->separator_x = term->separator_x ;
				}
			}
			term->next[idx2]->yfirst = term->yfirst ;

			parent->next[idx] = term->next[idx2] ;

			if( term->next[!idx2])
			{
				parent = search_parent_term( parent->next[idx] , NULL) ;
				if( parent->next[!idx2] == NULL)
				{
					parent->next[!idx2] = term->next[!idx2] ;
				}
				else
				{
					parent->next[idx2] = term->next[!idx2] ;
				}
			}
		}
		else
		{
			parent->next[idx] = NULL ;

			if( idx == 0)
			{
				parent->separator_x = 0 ;
			}
			else
			{
				parent->separator_y = 0 ;
			}
		}
	}

	x_window_remove_child( &layout->window , &term->scrollbar.window) ;
	x_window_remove_child( &layout->window , &screen->window) ;
	x_window_unmap( &screen->window) ;

	reset_layout( &layout->term , 0 , 0 , ACTUAL_WIDTH(&layout->window) ,
		ACTUAL_HEIGHT(&layout->window)) ;

	update_normal_hints( layout) ;

#ifndef  USE_FRAMEBUFFER
	if( x_screen_attached( screen))
	{
		/* Revert to the original size. */
		x_window_resize_with_margin( &screen->window ,
			ACTUAL_WIDTH(&layout->window) -
				(term->sb_mode != SBM_NONE ?
					SCROLLBAR_WIDTH(term->scrollbar) : 0) ,
			ACTUAL_HEIGHT(&layout->window) , NOTIFY_TO_MYSELF) ;
	}
#endif

	free( term) ;

	x_window_set_input_focus( &layout->term.screen->window) ;

	return  1 ;
}

int
x_layout_switch_screen(
	x_layout_t *  layout ,
	int  prev
	)
{
	x_screen_t *  screen ;

	if( ! layout->term.next[0] && ! layout->term.next[1])
	{
		return  0 ;
	}

	if( prev)
	{
		screen = NULL ;

		search_prev_screen( &layout->term , (x_screen_t*)get_current_window( layout) ,
				&screen) ;
	}
	else
	{
		int  found = 0 ;

		if( ! ( screen = search_next_screen( &layout->term ,
					(x_screen_t*)get_current_window( layout) , &found)))
		{
			screen = layout->term.screen ;
		}
	}

	x_window_set_input_focus( &screen->window) ;

	return  1 ;
}

int
x_layout_resize(
	x_layout_t *  layout ,
	x_screen_t *  screen ,
	int  horizontal ,
	int  step
	)
{
	struct terminal *  term ;

	if( step == 0)
	{
		return  0 ;
	}

	if( ! ( term = get_current_term( &layout->term)))
	{
		term = &layout->term ;
	}

	if( horizontal)
	{
		if( term->separator_x == 0)
		{
			if( ! ( term = search_parent_term( &layout->term , term)) ||
			    term->separator_x == 0)
			{
				return  0 ;
			}
			else
			{
				step = -step ;
			}
		}

		step *= x_col_width( term->screen) ;

		if( step < 0 && term->separator_x < abs(step))
		{
			return  0 ;
		}

		term->separator_x += step ;
	}
	else
	{
		if( term->separator_y == 0)
		{
			if( ! ( term = search_parent_term( &layout->term , term)) ||
			    term->separator_y == 0)
			{
				return  0 ;
			}
			else
			{
				step = -step ;
			}
		}

		step *= x_line_height( term->screen) ;

		if( step < 0 && term->separator_y < abs(step))
		{
			return  0 ;
		}

		term->separator_y += step ;
	}

	reset_layout( &layout->term , 0 , 0 , ACTUAL_WIDTH(&layout->window) ,
		ACTUAL_HEIGHT(&layout->window)) ;

	return  1 ;
}
