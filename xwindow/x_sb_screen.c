/*
 *	$Id$
 */

#include  "x_sb_screen.h"

#include  <kiklib/kik_types.h>		/* u_int */


#define  SEPARATOR_WIDTH  1


/* --- static functions --- */

static void
move_scrollbar(
	x_sb_screen_t *  sb_screen ,
	int  to_right
	)
{
	if( to_right)
	{
		x_window_move( &sb_screen->scrollbar.window ,
			ACTUAL_WIDTH( &sb_screen->screen->window) + SEPARATOR_WIDTH , 0) ;
	}
	else
	{
		x_window_move( &sb_screen->scrollbar.window , 0 , 0) ;
	}
}

static void
move_term_screen(
	x_sb_screen_t *  sb_screen ,
	int  to_right
	)
{
	if( to_right)
	{
		x_window_move( &sb_screen->screen->window ,
			ACTUAL_WIDTH( &sb_screen->scrollbar.window) + SEPARATOR_WIDTH , 0) ;
	}
	else
	{
		x_window_move( &sb_screen->screen->window , 0 , 0) ;
	}
}


/*
 * callbacks of x_window_t events.
 */

static void
window_realized(
	x_window_t *  win
	)
{
	/* seperator color of x_scrollbar_t and x_screen_t */
	x_window_set_fg_color( win , BlackPixel( win->display , win->screen)) ;
}

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
	u_int  actual_width ;

	sb_screen = (x_sb_screen_t*) win ;

	if( sb_screen->sb_mode == SB_NONE)
	{
		actual_width = ACTUAL_WIDTH(win) ;
	}
	else
	{
		actual_width = ACTUAL_WIDTH(win) - ACTUAL_WIDTH( &sb_screen->scrollbar.window)
				- SEPARATOR_WIDTH ;
	}
	
	x_window_resize_with_margin( &sb_screen->screen->window ,
		actual_width , ACTUAL_HEIGHT(win) , NOTIFY_TO_MYSELF) ;

	x_window_resize_with_margin( &sb_screen->scrollbar.window ,
		ACTUAL_WIDTH( &sb_screen->scrollbar.window) ,
		ACTUAL_HEIGHT(win) , NOTIFY_TO_MYSELF) ;
		
	if( sb_screen->sb_mode == SB_RIGHT)
	{
		move_scrollbar( sb_screen , 1) ;
	}
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

	if( &sb_screen->screen->window == child)
	{
		if( sb_screen->sb_mode == SB_NONE)
		{
			actual_width = ACTUAL_WIDTH(child) ;
		}
		else
		{
			actual_width = ACTUAL_WIDTH(child) + ACTUAL_WIDTH( &sb_screen->scrollbar.window) +
					SEPARATOR_WIDTH ;
		}

		x_window_resize_with_margin( &sb_screen->window ,
			actual_width , ACTUAL_HEIGHT(child) , NOTIFY_TO_NONE) ;

		x_window_resize_with_margin( &sb_screen->scrollbar.window ,
			ACTUAL_WIDTH( &sb_screen->scrollbar.window) ,
			ACTUAL_HEIGHT(child) , NOTIFY_TO_MYSELF) ;

		if( sb_screen->sb_mode == SB_RIGHT)
		{
			move_scrollbar( sb_screen , 1) ;
		}
	}
	else if( &sb_screen->scrollbar.window == child)
	{
		if( sb_screen->sb_mode == SB_NONE)
		{
			return ;
		}

		x_window_resize_with_margin( &sb_screen->window ,
			ACTUAL_WIDTH(child) + ACTUAL_WIDTH( &sb_screen->screen->window) + SEPARATOR_WIDTH ,
			ACTUAL_HEIGHT(child) , NOTIFY_TO_NONE) ;

		if( sb_screen->sb_mode == SB_LEFT)
		{
			move_term_screen( sb_screen , 1) ;
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
	(*sb_screen->screen->window.key_pressed)( &sb_screen->screen->window , event) ;
}

static void
utf8_selection_notified(
	x_window_t *  win ,
	u_char *  buf ,
	size_t  len
	)
{
	x_sb_screen_t *  sb_screen ;

	sb_screen = (x_sb_screen_t*) win ;

	/* dispatch to screen */
	(*sb_screen->screen->window.utf8_selection_notified)( &sb_screen->screen->window ,
		buf , len) ;
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
	(*sb_screen->screen->window.xct_selection_notified)( &sb_screen->screen->window ,
		buf , len) ;
}

static void
window_deleted(
	x_window_t *  win
	)
{
	x_sb_screen_t *  sb_screen ;

	sb_screen = (x_sb_screen_t*) win ;

	/* dispatch to screen */
	(*sb_screen->screen->window.window_deleted)( &sb_screen->screen->window) ;
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

	sb_screen = (x_sb_screen_t*)((x_window_t*)p)->parent ;

	(*sb_screen->line_scrolled_out)( p) ;

	x_scrollbar_line_is_added( &sb_screen->scrollbar) ;
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
	x_sb_screen_t *  sb_screen ;

	sb_screen = p ;

	x_screen_scroll_to( sb_screen->screen , row) ;
	
	return  1 ;
}

static int
screen_scroll_upward(
	void *  p ,
	u_int  size
	)
{
	x_sb_screen_t *  sb_screen ;

	sb_screen = p ;

	x_screen_scroll_upward( sb_screen->screen , size) ;
	
	return  1 ;
}

static int
screen_scroll_downward(
	void *  p ,
	u_int  size
	)
{
	x_sb_screen_t *  sb_screen ;

	sb_screen = p ;

	x_screen_scroll_downward( sb_screen->screen , size) ;

	return  1 ;
}

static int
screen_is_static(
	void *  p
	)
{
	x_sb_screen_t *  sb_screen  ;
	
	sb_screen = p ;

	if( ml_term_is_backscrolling( sb_screen->screen->term) == BSM_STATIC)
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
	x_sb_screen_t *  sb_screen  ;
	
	sb_screen = p ;
	
	x_scrollbar_reset( &sb_screen->scrollbar) ;
}

static void
scrolled_upward(
	void *  p ,
	u_int  size
	)
{
	x_sb_screen_t *  sb_screen  ;

	sb_screen = p ;

	x_scrollbar_move_downward( &sb_screen->scrollbar , size) ;
}

static void
scrolled_downward(
	void *  p ,
	u_int  size
	)
{
	x_sb_screen_t *  sb_screen  ;

	sb_screen = p ;

	x_scrollbar_move_upward( &sb_screen->scrollbar , size) ;
}

static void
log_size_changed(
	void *  p ,
	u_int  log_size
	)
{
	x_sb_screen_t *  sb_screen  ;

	sb_screen = p ;

	x_scrollbar_set_num_of_log_lines( &sb_screen->scrollbar , log_size) ;
}

static void
line_height_changed(
	void *  p ,
	u_int  line_height
	)
{
	x_sb_screen_t *  sb_screen  ;

	sb_screen = p ;

	x_scrollbar_set_line_height( &sb_screen->scrollbar , line_height) ;
}

static void
change_fg_color(
	void *  p ,
	char *  color
	)
{
	x_sb_screen_t *  sb_screen  ;

	sb_screen = p ;

	x_scrollbar_set_fg_color( &sb_screen->scrollbar , color) ;
}

static char *
get_fg_color(
	void *  p
	)
{
	x_sb_screen_t *  sb_screen  ;

	sb_screen = p ;

	return  sb_screen->scrollbar.fg_color ;
}

static void
change_bg_color(
	void *  p ,
	char *  color
	)
{
	x_sb_screen_t *  sb_screen  ;

	sb_screen = p ;

	x_scrollbar_set_bg_color( &sb_screen->scrollbar , color) ;
}

static char *
get_bg_color(
	void *  p
	)
{
	x_sb_screen_t *  sb_screen  ;

	sb_screen = p ;

	return  sb_screen->scrollbar.bg_color ;
}

static void
change_view(
	void *  p ,
	char *  name
	)
{
	x_sb_screen_t *  sb_screen  ;

	sb_screen = p ;

	x_scrollbar_change_view( &sb_screen->scrollbar , name) ;
}

static char *
get_view_name(
	void *  p
	)
{
	x_sb_screen_t *  sb_screen  ;

	sb_screen = p ;

	return  sb_screen->scrollbar.view_name ;
}

static void
transparent_state_changed(
	void *  p ,
	int  is_transparent ,
	x_picture_modifier_t *  pic_mod
	)
{
	x_sb_screen_t *  sb_screen  ;

	sb_screen = p ;

	if( is_transparent == 1)
	{
		x_scrollbar_set_transparent( &sb_screen->scrollbar , pic_mod , 1) ;
	}
	else
	{
		x_scrollbar_unset_transparent( &sb_screen->scrollbar) ;
	}
}

static x_sb_mode_t
sb_mode(
	void *  p
	)
{
	x_sb_screen_t *  sb_screen  ;
	
	sb_screen = p ;
	
	return  sb_screen->sb_mode ;
}

static void
change_sb_mode(
	void *  p ,
	x_sb_mode_t  mode
	)
{
	x_sb_screen_t *  sb_screen  ;

	sb_screen = p ;
	
	if( sb_screen->sb_mode == mode)
	{
		return ;
	}
	
	if( mode == SB_NONE)
	{
		x_window_unmap( &sb_screen->scrollbar.window) ;
	
		x_window_resize_with_margin( &sb_screen->window ,
			ACTUAL_WIDTH( &sb_screen->screen->window) ,
			ACTUAL_HEIGHT( &sb_screen->screen->window) , NOTIFY_TO_NONE) ;

		x_window_set_normal_hints( &sb_screen->window , 0 , 0 , 0 , 0) ;
			
		/* overlaying scrollbar window */
		move_term_screen( sb_screen , 0) ;
	}
	else
	{
		if( sb_screen->sb_mode == SB_NONE) ;
		{
			x_window_map( &sb_screen->scrollbar.window) ;
			
			x_window_resize_with_margin( &sb_screen->window ,
				ACTUAL_WIDTH( &sb_screen->screen->window)
					+ ACTUAL_WIDTH( &sb_screen->scrollbar.window)
					+ SEPARATOR_WIDTH ,
				ACTUAL_HEIGHT( &sb_screen->screen->window) ,
				NOTIFY_TO_NONE) ;
				
			x_window_set_normal_hints( &sb_screen->window ,
				SEPARATOR_WIDTH , sb_screen->window.min_height , 0 , 0) ;
		}
		
		if( mode == SB_LEFT)
		{
			move_term_screen( sb_screen , 1) ;
			move_scrollbar( sb_screen , 0) ;
		}
		else /* if( mode == SB_RIGHT) */
		{
			move_term_screen( sb_screen , 0) ;
			move_scrollbar( sb_screen , 1) ;
		}
	}

	sb_screen->sb_mode = mode ;
}

static void
term_changed(
	void *  p ,
	u_int  log_size ,
	u_int  logged_lines
	)
{
	x_sb_screen_t *  sb_screen  ;

	sb_screen = p ;

	x_scrollbar_set_num_of_log_lines( &sb_screen->scrollbar , log_size) ;
	x_scrollbar_set_num_of_filled_log_lines( &sb_screen->scrollbar , logged_lines) ;
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
	
	if( ( sb_screen = malloc( sizeof( x_sb_screen_t))) == NULL)
	{
		return  NULL ;
	}

	/*
	 * event callbacks.
	 */
	sb_screen->sb_listener.self = sb_screen ;
	sb_screen->sb_listener.screen_scroll_to = screen_scroll_to ;
	sb_screen->sb_listener.screen_scroll_upward = screen_scroll_upward ;
	sb_screen->sb_listener.screen_scroll_downward = screen_scroll_downward ;
	sb_screen->sb_listener.screen_is_static = screen_is_static ;

	if( x_scrollbar_init( &sb_screen->scrollbar , &sb_screen->sb_listener ,
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

	sb_screen->screen = screen ;

	/*
	 * event callbacks.
	 */	
	sb_screen->screen_scroll_listener.self = sb_screen ;
	sb_screen->screen_scroll_listener.bs_mode_entered = NULL ;
	sb_screen->screen_scroll_listener.bs_mode_exited = bs_mode_exited ;
	sb_screen->screen_scroll_listener.scrolled_upward = scrolled_upward ;
	sb_screen->screen_scroll_listener.scrolled_downward = scrolled_downward ;
	sb_screen->screen_scroll_listener.log_size_changed = log_size_changed ;
	sb_screen->screen_scroll_listener.line_height_changed = line_height_changed ;
	sb_screen->screen_scroll_listener.change_fg_color = change_fg_color ;
	sb_screen->screen_scroll_listener.fg_color = get_fg_color ;
	sb_screen->screen_scroll_listener.change_bg_color = change_bg_color ;
	sb_screen->screen_scroll_listener.bg_color = get_bg_color ;
	sb_screen->screen_scroll_listener.change_view = change_view ;
	sb_screen->screen_scroll_listener.view_name = get_view_name ;
	sb_screen->screen_scroll_listener.transparent_state_changed = transparent_state_changed ;
	sb_screen->screen_scroll_listener.sb_mode = sb_mode ;
	sb_screen->screen_scroll_listener.change_sb_mode = change_sb_mode ;
	sb_screen->screen_scroll_listener.term_changed = term_changed ;

	x_set_screen_scroll_listener( screen , &sb_screen->screen_scroll_listener) ;

	sb_screen->line_scrolled_out = screen->screen_listener.line_scrolled_out ;
	screen->screen_listener.line_scrolled_out = line_scrolled_out ;

	sb_screen->sb_mode = mode ;

	if( sb_screen->sb_mode == SB_NONE)
	{
		actual_width = ACTUAL_WIDTH( &screen->window) ;
		
		min_width = 0 ;
	}
	else
	{
		actual_width = (ACTUAL_WIDTH( &screen->window) +
				ACTUAL_WIDTH( &sb_screen->scrollbar.window) + SEPARATOR_WIDTH) ;

		min_width = SEPARATOR_WIDTH ;
	}

	if( x_window_init( &sb_screen->window ,
		actual_width , ACTUAL_HEIGHT( &screen->window) ,
		min_width , 0 , 0 , 0 , 0) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_window_init() failed.\n") ;
	#endif

		goto  error ;
	}

	if( sb_screen->sb_mode == SB_RIGHT)
	{
		if( x_window_add_child( &sb_screen->window , &screen->window ,
			0 , 0 , 1) == 0)
		{
			goto  error ;
		}

		if( x_window_add_child( &sb_screen->window , &sb_screen->scrollbar.window ,
			ACTUAL_WIDTH( &screen->window) + SEPARATOR_WIDTH , 0 , 1) == 0)
		{
			goto  error ;
		}
	}
	else if( sb_screen->sb_mode == SB_LEFT)
	{
		if( x_window_add_child( &sb_screen->window , &sb_screen->scrollbar.window , 0 , 0 , 1) == 0)
		{
			goto  error ;
		}
		
		if( x_window_add_child( &sb_screen->window , &screen->window ,
			ACTUAL_WIDTH( &sb_screen->scrollbar.window) + SEPARATOR_WIDTH , 0 , 1) == 0)
		{
			goto  error ;
		}
	}
	else /* if( sb_screen->sb_mode == SB_NONE) */
	{
		if( x_window_add_child( &sb_screen->window , &sb_screen->scrollbar.window , 0 , 0 , 0) == 0)
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
	sb_screen->window.window_realized = window_realized ;
	sb_screen->window.window_finalized = window_finalized ;
	sb_screen->window.window_resized = window_resized ;
	sb_screen->window.child_window_resized = child_window_resized ;
	sb_screen->window.window_exposed = window_exposed ;
	sb_screen->window.key_pressed = key_pressed ;
	sb_screen->window.utf8_selection_notified = utf8_selection_notified ;
	sb_screen->window.xct_selection_notified = xct_selection_notified ;
	sb_screen->window.window_deleted = window_deleted ;

	return  sb_screen ;

error:
	if( sb_screen)
	{
		free( sb_screen) ;
	}

	return  NULL ;
}

int
x_sb_screen_delete(
	x_sb_screen_t *  sb_screen
	)
{
	x_scrollbar_final( &sb_screen->scrollbar) ;

	free( sb_screen) ;

	return  1 ;
}
