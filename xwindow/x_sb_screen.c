/*
 *	$Id$
 */

#include  "x_sb_screen.h"

#include  <kiklib/kik_types.h>		/* u_int */


#define  SEPARATOR_WIDTH  1
#define  SCREEN_TO_SBSCREEN(screen)  ((x_sb_screen_t*)(screen)->window.parent)


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
				x_col_width( term->next[0]->screen) +
				(term->next[0]->sb_mode != SBM_NONE ?
					ACTUAL_WIDTH(&term->next[0]->scrollbar.window) +
					SEPARATOR_WIDTH : 0)) ;
	}
	else
	{
		child_width = width ;
	}

	if( term->separator_y > 0)
	{
		term->separator_y = child_height =
			modify_separator( term->separator_y , height ,
				x_line_height( term->next[1]->screen)) ;
	}
	else
	{
		child_height = height ;
	}

	if( term->sb_mode != SBM_NONE)
	{
		if( term->sb_mode == SBM_RIGHT)
		{
			x_window_move( &term->scrollbar.window ,
				x + child_width - ACTUAL_WIDTH(&term->scrollbar.window) -
					SEPARATOR_WIDTH ,
				y) ;
			x_window_move( &term->screen->window , x , y) ;
		}
		else
		{
			x_window_move( &term->scrollbar.window , x , y) ;
			x_window_move( &term->screen->window ,
				x + ACTUAL_WIDTH(&term->scrollbar.window) + SEPARATOR_WIDTH ,
				y) ;
		}

		x_window_resize_with_margin( &term->scrollbar.window ,
			ACTUAL_WIDTH(&term->scrollbar.window) ,
			child_height , NOTIFY_TO_MYSELF) ;
		x_window_resize_with_margin( &term->screen->window ,
			child_width - ACTUAL_WIDTH(&term->scrollbar.window) - SEPARATOR_WIDTH ,
			child_height , NOTIFY_TO_MYSELF) ;
	}
	else
	{
		x_window_unmap( &term->scrollbar.window) ;
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
			x_window_fill( term->screen->window.parent , x , y + term->separator_y ,
				width , SEPARATOR_WIDTH) ;
		}

		if( term->next[0] && term->separator_x > 0)
		{
			reset_layout( term->next[0] , x + term->separator_x + SEPARATOR_WIDTH , y ,
				width - term->separator_x - SEPARATOR_WIDTH , child_height) ;
			x_window_fill( term->screen->window.parent , x + term->separator_x , y ,
				SEPARATOR_WIDTH , child_height) ;
		}
	}
	else
	{
		if( term->next[0] && term->separator_x > 0)
		{
			reset_layout( term->next[0] , x + term->separator_x + SEPARATOR_WIDTH , y ,
				width - term->separator_x - SEPARATOR_WIDTH , height) ;
			x_window_fill( term->screen->window.parent , x + term->separator_x , y ,
				SEPARATOR_WIDTH , height) ;
		}

		if( term->next[1] && term->separator_y > 0)
		{
			reset_layout( term->next[1] , x , y + term->separator_y + SEPARATOR_WIDTH ,
				child_width , height - term->separator_y - SEPARATOR_WIDTH) ;
			x_window_fill( term->screen->window.parent , x , y + term->separator_y ,
				child_width , SEPARATOR_WIDTH) ;
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

static struct terminal *
get_current_term(
	struct terminal *  term
	)
{
	struct terminal *  current ;

#if  defined(USE_FRAMEBUFFER) || defined(USE_WIN32GUI)
	if( term->screen->window.is_focused)
#else
	if( term->screen->window.has_input_focus)
#endif
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
get_focused_window(
	x_sb_screen_t *  sb_screen
	)
{
	struct terminal *  term ;

	if( ( term = get_current_term( &sb_screen->term)))
	{
		return  &term->screen->window ;
	}
	else
	{
		return  &sb_screen->term.screen->window ;
	}
}

static u_int
get_separator_x(
	x_screen_t *  screen ,
	x_scrollbar_t *  scrollbar
	)
{
	u_int  width ;
	u_int  fixed_width ;
	u_int  sep_x ;

	width = screen->window.width ;
	fixed_width = screen->window.hmargin * 2 ;
	if( scrollbar)
	{
		fixed_width += (ACTUAL_WIDTH(&scrollbar->window) + SEPARATOR_WIDTH) ;
	}

	sep_x = (width + fixed_width) / 2 ;
	if( sep_x < fixed_width + x_col_width( screen))
	{
		return  0 ;
	}

	return  sep_x - (sep_x - fixed_width) % x_col_width( screen) ;
}

static u_int
get_separator_y(
	x_screen_t *  screen
	)
{
	u_int  height ;
	u_int  fixed_height ;
	u_int  sep_y ;

	height = screen->window.height ;
	fixed_height = screen->window.vmargin * 2 ;

	sep_y = (height + fixed_height) / 2 ;
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
	x_sb_screen_t *  sb_screen ;

	sb_screen = (x_sb_screen_t*) win ;

	x_sb_screen_delete( sb_screen) ;
}

static void
window_resized(
	x_window_t *  win
	)
{
	x_sb_screen_t *  sb_screen ;

	sb_screen = (x_sb_screen_t*) win ;

	reset_layout( &sb_screen->term , 0 , 0 ,
		ACTUAL_WIDTH(&sb_screen->window) ,
		ACTUAL_HEIGHT(&sb_screen->window)) ;
}

static void
child_window_resized(
	x_window_t *  win ,
	x_window_t *  child
	)
{
	x_sb_screen_t *  sb_screen ;
	u_int  actual_width ;

	sb_screen = (x_sb_screen_t*) win ;

	if( sb_screen->term.next[0] || sb_screen->term.next[1])
	{
		reset_layout( &sb_screen->term , 0 , 0 ,
			ACTUAL_WIDTH(&sb_screen->window) ,
			ACTUAL_HEIGHT(&sb_screen->window)) ;

		return ;
	}

	if( &sb_screen->term.screen->window == child)
	{
		if( sb_screen->term.sb_mode == SBM_NONE)
		{
			actual_width = ACTUAL_WIDTH(child) ;
		}
		else
		{
			actual_width = ACTUAL_WIDTH(child) +
					ACTUAL_WIDTH( &sb_screen->term.scrollbar.window) +
					SEPARATOR_WIDTH ;
		}

		x_window_resize_with_margin( &sb_screen->window ,
			actual_width , ACTUAL_HEIGHT(child) , NOTIFY_TO_NONE) ;

		x_window_resize_with_margin( &sb_screen->term.scrollbar.window ,
			ACTUAL_WIDTH( &sb_screen->term.scrollbar.window) ,
			ACTUAL_HEIGHT(child) , NOTIFY_TO_MYSELF) ;

		if( sb_screen->term.sb_mode == SBM_RIGHT)
		{
			x_window_move( &sb_screen->term.scrollbar.window ,
				ACTUAL_WIDTH( &sb_screen->term.screen->window) + SEPARATOR_WIDTH ,
				0) ;
		}
	}
	else if( &sb_screen->term.scrollbar.window == child)
	{
		if( sb_screen->term.sb_mode == SBM_NONE)
		{
			return ;
		}

		x_window_resize_with_margin( &sb_screen->window ,
			ACTUAL_WIDTH(child) + ACTUAL_WIDTH( &sb_screen->term.screen->window) +
			SEPARATOR_WIDTH , ACTUAL_HEIGHT(child) , NOTIFY_TO_NONE) ;

		if( sb_screen->term.sb_mode == SBM_LEFT)
		{
			x_window_move( &sb_screen->term.screen->window ,
				ACTUAL_WIDTH( &sb_screen->term.scrollbar.window) +
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
	x_sb_screen_t *  sb_screen ;

	sb_screen = (x_sb_screen_t*) win ;

	/* dispatch to screen */
	(*sb_screen->term.screen->window.key_pressed)( get_focused_window(sb_screen) , event) ;
}

static void
utf_selection_notified(
	x_window_t *  win ,
	u_char *  buf ,
	size_t  len
	)
{
	x_sb_screen_t *  sb_screen ;

	sb_screen = (x_sb_screen_t*) win ;

	/* dispatch to screen */
	(*sb_screen->term.screen->window.utf_selection_notified)(
		get_focused_window(sb_screen) , buf , len) ;
}

static void
xct_selection_notified(
	x_window_t *  win ,
	u_char *  buf ,
	size_t  len
	)
{
	x_sb_screen_t *  sb_screen ;

	sb_screen = (x_sb_screen_t*) win ;

	/* dispatch to screen */
	(*sb_screen->term.screen->window.xct_selection_notified)(
		get_focused_window(sb_screen) , buf , len) ;
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
	x_sb_screen_t *  sb_screen ;

	sb_screen = (x_sb_screen_t*) win ;

	/* dispatch to screen */
	(*sb_screen->term.screen->window.set_xdnd_config)(
		get_focused_window(sb_screen) , dev, buf , value) ;
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
	x_sb_screen_t *  sb_screen ;

	sb_screen = (x_sb_screen_t*) win ;

	window_deleted_intern( &sb_screen->term) ;
}


/*
 * Overriding methods of ml_screen_listener_t of x_screen_t.
 */
 
static void
line_scrolled_out(
	void *  p		/* must be x_screen_t(, or child of x_sb_screen_t) */
	)
{
	x_sb_screen_t *  sb_screen ;
	struct terminal *  term ;

	sb_screen = SCREEN_TO_SBSCREEN( (x_screen_t*)p) ;

	(*sb_screen->line_scrolled_out)( p) ;

	if( ( term = search_term( &sb_screen->term , p)))
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
		x_window_resize_with_margin( term->screen->window.parent /* sb_screen */ ,
			total_width( term) , total_height( term) , NOTIFY_TO_MYSELF) ;
	#endif

		x_window_set_normal_hints( term->screen->window.parent /* sb_screen */ ,
			0 , 0 , 0 , 0) ;

	#ifdef  USE_FRAMEBUFFER
		/*
		 * The screen size is not changed.
		 * As a result the size of term->screen gets larger.
		 */
		window_resized( term->screen->window.parent /* sb_screen */) ;
	#endif
	}
	else
	{
		if( old_mode == SBM_NONE)
		{
			x_window_map( &term->scrollbar.window) ;

		#ifndef  USE_FRAMEBUFFER
			/* The screen size is changed to the one with scrollbar. */
			x_window_resize_with_margin( term->screen->window.parent /* sb_screen */ ,
				total_width( term) , total_height( term) , NOTIFY_TO_MYSELF) ;
		#else
			/*
			 * The screen size is not changed.
			 * As a result the size of term->screen gets smaller.
			 */
			window_resized( term->screen->window.parent /* sb_screen */) ;
		#endif

			x_window_set_normal_hints( term->screen->window.parent /* sb_screen */ ,
				SEPARATOR_WIDTH , term->screen->window.parent->min_height , 0 , 0) ;
		}
		else
		{
			reset_layout( &((x_sb_screen_t*)term->screen->window.parent)->term ,
				0 , 0 ,
				ACTUAL_WIDTH(term->screen->window.parent) ,
				ACTUAL_HEIGHT(term->screen->window.parent)) ;
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

x_sb_screen_t *
x_sb_screen_new(
	x_screen_t *  screen ,
	char *  view_name ,
	char *  fg_color ,
	char *  bg_color ,
	x_sb_mode_t  mode
	)
{
	x_sb_screen_t *  sb_screen ;
	u_int  actual_width ;
	u_int  min_width ;
	
	if( ( sb_screen = calloc( 1 , sizeof( x_sb_screen_t))) == NULL)
	{
		return  NULL ;
	}

	/*
	 * event callbacks.
	 */
	sb_screen->term.sb_listener.self = &sb_screen->term ;
	sb_screen->term.sb_listener.screen_scroll_to = screen_scroll_to ;
	sb_screen->term.sb_listener.screen_scroll_upward = screen_scroll_upward ;
	sb_screen->term.sb_listener.screen_scroll_downward = screen_scroll_downward ;
	sb_screen->term.sb_listener.screen_is_static = screen_is_static ;

	if( x_scrollbar_init( &sb_screen->term.scrollbar , &sb_screen->term.sb_listener ,
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

	sb_screen->term.screen = screen ;

	/*
	 * event callbacks.
	 */
	sb_screen->term.screen_scroll_listener.self = &sb_screen->term ;
	sb_screen->term.screen_scroll_listener.bs_mode_entered = NULL ;
	sb_screen->term.screen_scroll_listener.bs_mode_exited = bs_mode_exited ;
	sb_screen->term.screen_scroll_listener.scrolled_upward = scrolled_upward ;
	sb_screen->term.screen_scroll_listener.scrolled_downward = scrolled_downward ;
	sb_screen->term.screen_scroll_listener.scrolled_to = scrolled_to ;
	sb_screen->term.screen_scroll_listener.log_size_changed = log_size_changed ;
	sb_screen->term.screen_scroll_listener.line_height_changed = line_height_changed ;
	sb_screen->term.screen_scroll_listener.change_fg_color = change_fg_color ;
	sb_screen->term.screen_scroll_listener.fg_color = get_fg_color ;
	sb_screen->term.screen_scroll_listener.change_bg_color = change_bg_color ;
	sb_screen->term.screen_scroll_listener.bg_color = get_bg_color ;
	sb_screen->term.screen_scroll_listener.change_view = change_view ;
	sb_screen->term.screen_scroll_listener.view_name = get_view_name ;
	sb_screen->term.screen_scroll_listener.transparent_state_changed = transparent_state_changed ;
	sb_screen->term.screen_scroll_listener.sb_mode = sb_mode ;
	sb_screen->term.screen_scroll_listener.change_sb_mode = change_sb_mode ;
	sb_screen->term.screen_scroll_listener.term_changed = term_changed ;

	x_set_screen_scroll_listener( screen ,
		&sb_screen->term.screen_scroll_listener) ;

	sb_screen->line_scrolled_out = screen->screen_listener.line_scrolled_out ;
	screen->screen_listener.line_scrolled_out = line_scrolled_out ;

	sb_screen->term.sb_mode = mode ;

	if( sb_screen->term.sb_mode == SBM_NONE)
	{
		actual_width = ACTUAL_WIDTH( &screen->window) ;
		
		min_width = 0 ;
	}
	else
	{
		actual_width = (ACTUAL_WIDTH( &screen->window) +
				ACTUAL_WIDTH( &sb_screen->term.scrollbar.window) + SEPARATOR_WIDTH) ;

		min_width = SEPARATOR_WIDTH ;
	}

	if( x_window_init( &sb_screen->window ,
		actual_width , ACTUAL_HEIGHT( &screen->window) ,
		min_width , 0 , 0 , 0 , 0 , 0 , 0 , 0) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_window_init() failed.\n") ;
	#endif

		goto  error ;
	}

	if( sb_screen->term.sb_mode == SBM_RIGHT)
	{
		if( x_window_add_child( &sb_screen->window , &screen->window ,
			0 , 0 , 1) == 0)
		{
			goto  error ;
		}

		if( x_window_add_child( &sb_screen->window , &sb_screen->term.scrollbar.window ,
			ACTUAL_WIDTH( &screen->window) + SEPARATOR_WIDTH , 0 , 1) == 0)
		{
			goto  error ;
		}
	}
	else if( sb_screen->term.sb_mode == SBM_LEFT)
	{
		if( x_window_add_child( &sb_screen->window , &sb_screen->term.scrollbar.window ,
			0 , 0 , 1) == 0)
		{
			goto  error ;
		}
		
		if( x_window_add_child( &sb_screen->window , &screen->window ,
			ACTUAL_WIDTH( &sb_screen->term.scrollbar.window) + SEPARATOR_WIDTH ,
			0 , 1) == 0)
		{
			goto  error ;
		}
	}
	else /* if( sb_screen->term.sb_mode == SBM_NONE) */
	{
		if( x_window_add_child( &sb_screen->window , &sb_screen->term.scrollbar.window ,
			0 , 0 , 0) == 0)
		{
			goto  error ;
		}

		/* overlaying scrollbar window */
		if( x_window_add_child( &sb_screen->window , &screen->window , 0 , 0 , 1) == 0)
		{
			goto  error ;
		}
	}

	/*
	 * event call backs.
	 */
	x_window_init_event_mask( &sb_screen->window , KeyPressMask) ;
	sb_screen->window.window_finalized = window_finalized ;
	sb_screen->window.window_resized = window_resized ;
	sb_screen->window.child_window_resized = child_window_resized ;
	sb_screen->window.window_exposed = window_exposed ;
	sb_screen->window.key_pressed = key_pressed ;
	sb_screen->window.utf_selection_notified = utf_selection_notified ;
	sb_screen->window.xct_selection_notified = xct_selection_notified ;
	sb_screen->window.window_deleted = window_deleted ;
#ifndef  DISABLE_XDND
	sb_screen->window.set_xdnd_config = set_xdnd_config ;
#endif
	return  sb_screen ;

error:
	free( sb_screen) ;

	return  NULL ;
}

int
x_sb_screen_delete(
	x_sb_screen_t *  sb_screen
	)
{
	delete_term( &sb_screen->term) ;
	free( sb_screen) ;

	return  1 ;
}

int
x_sb_screen_add_child(
	x_sb_screen_t *  sb_screen ,
	x_screen_t *  screen ,
	int  vertical
	)
{
	struct terminal *  next ;
	struct terminal *  term ;

	if( ! ( next = calloc( 1 , sizeof(struct terminal))))
	{
		return  0 ;
	}

	if( ! ( term = get_current_term( &sb_screen->term)))
	{
		term = &sb_screen->term ;
	}

	if( vertical)
	{
		if( term->separator_x > 0 ||
		    ( term->separator_x = get_separator_x( term->screen ,
						(term->sb_mode != SBM_NONE ?
							&term->scrollbar : NULL))) == 0)
		{
			free( next) ;

			return  0 ;
		}

		next->next[0] = term->next[0] ;
		term->next[0] = next ;
	}
	else
	{
		if( term->separator_y > 0 ||
		    ( ( term->separator_y = get_separator_y( term->screen)) == 0))
		{
			free( next) ;

			return  0 ;
		}

		if( term->separator_x == 0)
		{
			term->yfirst = 1 ;
		}

		next->next[1] = term->next[1] ;
		term->next[1] = next ;
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
	x_window_add_child( &sb_screen->window , &next->scrollbar.window , 0 , 0 , 0) ;
	x_window_show( &next->scrollbar.window , 0) ;

	next->screen_scroll_listener = term->screen_scroll_listener ;
	next->screen_scroll_listener.self = next ;

	next->screen = screen ;
	x_set_screen_scroll_listener( screen , &next->screen_scroll_listener) ;
	screen->screen_listener.line_scrolled_out = line_scrolled_out ;
	x_window_add_child( &sb_screen->window , &screen->window , 0 , 0 , 0) ;
	x_window_show( &screen->window , 0) ;

	next->sb_mode = SBM_LEFT ;

	reset_layout( &sb_screen->term , 0 , 0 , ACTUAL_WIDTH(&sb_screen->window) ,
		ACTUAL_HEIGHT(&sb_screen->window)) ;

	x_window_map( &next->scrollbar.window) ;
	x_window_map( &screen->window) ;

	return  1 ;
}

int
x_sb_screen_remove_child(
	x_sb_screen_t *  sb_screen ,
	x_screen_t *  screen
	)
{
	struct terminal *  term ;
	int  idx2 ;

	if( sb_screen->term.next[0] == NULL &&
	    sb_screen->term.next[1] == NULL)
	{
		return  0 ;
	}

	term = search_term( &sb_screen->term , screen) ;

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

	if( term == &sb_screen->term)
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

		parent = search_parent_term( &sb_screen->term , term) ;

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

	x_window_remove_child( &sb_screen->window , &term->scrollbar.window) ;
	free( term) ;

	x_window_remove_child( &sb_screen->window , &screen->window) ;

	reset_layout( &sb_screen->term , 0 , 0 , ACTUAL_WIDTH(&sb_screen->window) ,
		ACTUAL_HEIGHT(&sb_screen->window)) ;

	return  1 ;
}
