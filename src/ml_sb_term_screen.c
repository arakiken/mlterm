/*
 *	$Id$
 */

#include  "ml_sb_term_screen.h"

#include  <kiklib/kik_types.h>		/* u_int */


#define  SEPARATOR_WIDTH  1


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
		(ACTUAL_WIDTH(win) - sb_termscr->scrollbar.window.width - SEPARATOR_WIDTH)
			- sb_termscr->termscr->window.margin * 2 ,
		ACTUAL_HEIGHT(win) - sb_termscr->termscr->window.margin * 2 ,
		NOTIFY_TO_MYSELF) ;

	ml_window_resize( &sb_termscr->scrollbar.window ,
		sb_termscr->scrollbar.window.width ,
		ACTUAL_HEIGHT(win) - sb_termscr->scrollbar.window.margin * 2 ,
		NOTIFY_TO_MYSELF) ;

	if( sb_termscr->is_right_sb)
	{
		ml_window_move( &sb_termscr->scrollbar.window ,
			ACTUAL_WIDTH( &sb_termscr->termscr->window) + SEPARATOR_WIDTH , 0) ;
	}
}

static void
child_window_resized(
	ml_window_t *  win ,
	ml_window_t *  child
	)
{
	ml_sb_term_screen_t *  sb_termscr ;

	sb_termscr = (ml_sb_term_screen_t*) win ;

	if( &sb_termscr->termscr->window != child)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" illegal child. this event should be invoken only by termscr.\n") ;
	#endif
	
		return ;
	}
	
	ml_window_resize( &sb_termscr->window ,
		(ACTUAL_WIDTH(child) + ACTUAL_WIDTH( &sb_termscr->scrollbar.window) + SEPARATOR_WIDTH)
			- sb_termscr->window.margin * 2 ,
		ACTUAL_HEIGHT(child) - sb_termscr->window.margin * 2 ,
		NOTIFY_TO_NONE) ;
	
	ml_window_resize( &sb_termscr->scrollbar.window ,
		sb_termscr->scrollbar.window.width ,
		ACTUAL_HEIGHT(child) - sb_termscr->scrollbar.window.margin * 2 ,
		NOTIFY_TO_MYSELF) ;
		
	if( sb_termscr->is_right_sb)
	{
		ml_window_move( &sb_termscr->scrollbar.window ,
			ACTUAL_WIDTH( &sb_termscr->termscr->window) + SEPARATOR_WIDTH , 0) ;
	}
}

static void
window_exposed(
	ml_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
	ml_window_fill( win , x , y - win->margin , width , height + win->margin * 2) ;
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
	int  is_transparent ,
	ml_picture_modifier_t *  pic_mod
	)
{
	ml_sb_term_screen_t *  sb_termscr  ;

	sb_termscr = p ;

	if( is_transparent == 1)
	{
		ml_scrollbar_set_transparent( &sb_termscr->scrollbar , pic_mod , 1) ;
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
	int  is_right_sb
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
		termscr->window.is_transparent ,
		ml_term_screen_get_picture_modifier( termscr)) == NULL)
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
		(ACTUAL_WIDTH( &termscr->window) + ACTUAL_WIDTH( &sb_termscr->scrollbar.window)
			+ SEPARATOR_WIDTH) - termscr->window.margin * 2 ,
		ACTUAL_HEIGHT( &termscr->window) - termscr->window.margin * 2 ,
		termscr->window.min_width , termscr->window.min_height ,
		termscr->window.width_inc , termscr->window.height_inc ,
		termscr->window.margin) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_window_init() failed.\n") ;
	#endif

		goto  error ;
	}

	/* seperator color of ml_scrollbar_t and ml_term_screen_t */
	ml_window_set_fg_color( &sb_termscr->window , MLC_BLACK) ;

	/*
	 * event call backs.
	 */
	ml_window_init_event_mask( &sb_termscr->window , KeyPressMask) ;
	sb_termscr->window.window_finalized = window_finalized ;
	sb_termscr->window.window_resized = window_resized ;
	sb_termscr->window.child_window_resized = child_window_resized ;
	sb_termscr->window.window_exposed = window_exposed ;
	sb_termscr->window.key_pressed = key_pressed ;
	sb_termscr->window.window_deleted = window_deleted ;

	if( ( sb_termscr->is_right_sb = is_right_sb))
	{
		if( ml_window_add_child( &sb_termscr->window , &termscr->window ,
			0 , 0) == 0)
		{
			goto  error ;
		}

		if( ml_window_add_child( &sb_termscr->window , &sb_termscr->scrollbar.window ,
			ACTUAL_WIDTH( &termscr->window) + SEPARATOR_WIDTH , 0) == 0)
		{
			goto  error ;
		}
	}
	else
	{
		if( ml_window_add_child( &sb_termscr->window , &sb_termscr->scrollbar.window , 0 , 0) == 0)
		{
			goto  error ;
		}
		
		if( ml_window_add_child( &sb_termscr->window , &termscr->window ,
			ACTUAL_WIDTH( &sb_termscr->scrollbar.window) + SEPARATOR_WIDTH , 0) == 0)
		{
			goto  error ;
		}
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
