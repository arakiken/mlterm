/*
 *	$Id$
 */

#include  "ml_sb_term_screen.h"


/* --- static functions --- */

/*
 * callbacks of ml_window_t events.
 */
 
static void
window_finalized(
	ml_window_t *  win
	)
{
	ml_sb_term_screen_t *  sb_termscr ;

	sb_termscr = (ml_sb_term_screen_t*) win ;

	ml_sb_term_screen_delete( sb_termscr) ;
}

static void
window_resized(
	ml_window_t *  win
	)
{
	ml_sb_term_screen_t *  sb_termscr ;

	sb_termscr = (ml_sb_term_screen_t*) win ;

	ml_window_resize( &sb_termscr->termscr->window ,
		sb_termscr->window.width - sb_termscr->scrollbar.window.width ,
		win->height , NOTIFY_TO_MYSELF) ;

	/* srollbar window's margin is 0 , so ACTUAL_HEIGHT() is used. */
	ml_window_resize( &sb_termscr->scrollbar.window , sb_termscr->scrollbar.window.width ,
		ACTUAL_HEIGHT(win) , NOTIFY_TO_MYSELF) ;
}

static void
child_window_resized(
	ml_window_t *  win ,
	ml_window_t *  child
	)
{
	ml_sb_term_screen_t *  sb_termscr ;
	ml_term_screen_t *  termscr ;

	sb_termscr = (ml_sb_term_screen_t*) win ;
	termscr = (ml_term_screen_t*) child ;

	if( &sb_termscr->termscr->window != child)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" illegal child. this event should be invoken only by termscr.\n") ;
	#endif
	
		return ;
	}

	ml_window_resize( &sb_termscr->window ,
		termscr->window.width + sb_termscr->scrollbar.window.width ,
		termscr->window.height , 0) ;
	
	ml_window_resize( &sb_termscr->scrollbar.window ,
		sb_termscr->scrollbar.window.width ,
		ACTUAL_HEIGHT(&sb_termscr->window) , NOTIFY_TO_MYSELF) ;
}

static void
key_pressed(
	ml_window_t *  win ,
	XKeyEvent *  event
	)
{
	ml_sb_term_screen_t *  sb_termscr ;

	sb_termscr = (ml_sb_term_screen_t*) win ;

	/* dispatch to termscr */
	(*sb_termscr->termscr->window.key_pressed)( &sb_termscr->termscr->window , event) ;
}

static void
window_deleted(
	ml_window_t *  win
	)
{
	ml_sb_term_screen_t *  sb_termscr ;

	sb_termscr = (ml_sb_term_screen_t*) win ;

	/* dispatch to termscr */
	(*sb_termscr->termscr->window.window_deleted)( &sb_termscr->termscr->window) ;
}


/*
 * overriding methods of ml_term_screen_t
 */
 
static void
receive_scrolled_out_line(
	void *  p ,		/* must be ml_term_screen_t(, or child of ml_sb_term_screen_t) */
	ml_image_line_t *  line
	)
{
	ml_sb_term_screen_t *  sb_termscr ;

	sb_termscr = (ml_sb_term_screen_t*)((ml_window_t*)p)->parent ;

	(*sb_termscr->receive_upward_scrolled_out_line)( p , line) ;

	ml_scrollbar_line_is_added( &sb_termscr->scrollbar) ;

	return ;
}


/*
 * callbacks of ml_sb_event_listener_t events.
 */
 
static int
screen_scroll_to(
	void *  p ,
	int  row
	)
{
	ml_sb_term_screen_t *  sb_termscr ;

	sb_termscr = p ;

	ml_term_screen_scroll_to( sb_termscr->termscr , row) ;
	
	return  1 ;
}

static int
screen_scroll_upward(
	void *  p ,
	u_int  size
	)
{
	ml_sb_term_screen_t *  sb_termscr ;

	sb_termscr = p ;

	ml_term_screen_scroll_upward( sb_termscr->termscr , size) ;
	
	return  1 ;
}

static int
screen_scroll_downward(
	void *  p ,
	u_int  size
	)
{
	ml_sb_term_screen_t *  sb_termscr ;

	sb_termscr = p ;

	ml_term_screen_scroll_downward( sb_termscr->termscr , size) ;

	return  1 ;
}


/*
 * callbacks of ml_screen_scroll_event_listener_t events.
 */
 
static void
bs_mode_exited(
	void *  p
	)
{
	ml_sb_term_screen_t *  sb_termscr  ;
	
	sb_termscr = p ;
	
	ml_scrollbar_reset( &sb_termscr->scrollbar) ;
}

static void
scrolled_upward(
	void *  p ,
	u_int  size
	)
{
	ml_sb_term_screen_t *  sb_termscr  ;

	sb_termscr = p ;

	ml_scrollbar_move_downward( &sb_termscr->scrollbar , size) ;
}

static void
scrolled_downward(
	void *  p ,
	u_int  size
	)
{
	ml_sb_term_screen_t *  sb_termscr  ;

	sb_termscr = p ;

	ml_scrollbar_move_upward( &sb_termscr->scrollbar , size) ;
}

static void
log_size_changed(
	void *  p ,
	u_int  log_size
	)
{
	ml_sb_term_screen_t *  sb_termscr  ;

	sb_termscr = p ;

	ml_scrollbar_set_num_of_log_lines( &sb_termscr->scrollbar , log_size) ;
}

static void
line_height_changed(
	void *  p ,
	u_int  line_height
	)
{
	ml_sb_term_screen_t *  sb_termscr  ;

	sb_termscr = p ;

	ml_scrollbar_set_line_height( &sb_termscr->scrollbar , line_height) ;
}

static void
transparent_state_changed(
	void *  p ,
	int  is_transparent
	)
{
	ml_sb_term_screen_t *  sb_termscr  ;

	sb_termscr = p ;

	if( is_transparent)
	{
		ml_scrollbar_set_transparent( &sb_termscr->scrollbar) ;
	}
	else
	{
		ml_scrollbar_unset_transparent( &sb_termscr->scrollbar) ;
	}
}


/* --- global functions --- */

ml_sb_term_screen_t *
ml_sb_term_screen_new(
	ml_term_screen_t *  termscr ,
	char *  view_name ,
	ml_color_table_t  color_table ,
	int  use_transbg
	)
{
	ml_sb_term_screen_t *  sb_termscr ;
	
	if( ( sb_termscr = malloc( sizeof( ml_sb_term_screen_t))) == NULL)
	{
		return  NULL ;
	}

	/*
	 * event callbacks.
	 */
	sb_termscr->sb_listener.self = sb_termscr ;
	sb_termscr->sb_listener.screen_scroll_to = screen_scroll_to ;
	sb_termscr->sb_listener.screen_scroll_upward = screen_scroll_upward ;
	sb_termscr->sb_listener.screen_scroll_downward = screen_scroll_downward ;

	if( ml_scrollbar_init( &sb_termscr->scrollbar , &sb_termscr->sb_listener ,
		view_name , color_table , ACTUAL_HEIGHT( &termscr->window) ,
		ml_line_height( termscr->font_man) , termscr->logs.num_of_rows ,
		use_transbg) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_scrollbar_init() failed.\n") ;
	#endif

		goto  error ;
	}

	sb_termscr->termscr = termscr ;

	/*
	 * event callbacks.
	 */	
	sb_termscr->screen_scroll_listener.self = sb_termscr ;
	sb_termscr->screen_scroll_listener.bs_mode_exited = bs_mode_exited ;
	sb_termscr->screen_scroll_listener.bs_mode_entered = NULL ;
	sb_termscr->screen_scroll_listener.scrolled_upward = scrolled_upward ;
	sb_termscr->screen_scroll_listener.scrolled_downward = scrolled_downward ;
	sb_termscr->screen_scroll_listener.log_size_changed = log_size_changed ;
	sb_termscr->screen_scroll_listener.line_height_changed = line_height_changed ;
	sb_termscr->screen_scroll_listener.transparent_state_changed = transparent_state_changed ;

	ml_set_screen_scroll_listener( termscr , &sb_termscr->screen_scroll_listener) ;
	
	/* override */
	sb_termscr->receive_upward_scrolled_out_line =
		termscr->image_scroll_listener.receive_upward_scrolled_out_line ;
	termscr->image_scroll_listener.receive_upward_scrolled_out_line =
		receive_scrolled_out_line ;


	if( ml_window_init( &sb_termscr->window , ml_color_table_dup( color_table) ,
		termscr->window.width + sb_termscr->scrollbar.window.width ,
		termscr->window.height ,
		termscr->window.min_width , termscr->window.min_height ,
		termscr->window.width_inc , termscr->window.height_inc ,
		termscr->window.margin , 100) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_window_init() failed.\n") ;
	#endif

		goto  error ;
	}

	/*
	 * event call backs.
	 */
	ml_window_init_event_mask( &sb_termscr->window , KeyPressMask) ;
	sb_termscr->window.window_finalized = window_finalized ;
	sb_termscr->window.window_resized = window_resized ;
	sb_termscr->window.child_window_resized = child_window_resized ;
	sb_termscr->window.key_pressed = key_pressed ;
	sb_termscr->window.window_deleted = window_deleted ;

	if( ml_window_add_child( &sb_termscr->window , &sb_termscr->termscr->window ,
		sb_termscr->scrollbar.window.width , 0) == 0)
	{
		goto  error ;
	}
	
	if( ml_window_add_child( &sb_termscr->window , &sb_termscr->scrollbar.window , 0 , 0) == 0)
	{
		goto  error ;
	}

	return  sb_termscr ;

error:
	if( sb_termscr)
	{
		free( sb_termscr) ;
	}

	return  NULL ;
}

int
ml_sb_term_screen_delete(
	ml_sb_term_screen_t *  sb_termscr
	)
{
	ml_scrollbar_final( &sb_termscr->scrollbar) ;

	free( sb_termscr) ;

	return  1 ;
}
