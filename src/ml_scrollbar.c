/*
 *	$Id$
 */

#include  "ml_scrollbar.h"

#include  <string.h>		/* memset */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_str.h>	/* strdup */

#include  "ml_sb_view_factory.h"


#if  0
#define  __DEBUG
#endif


#define  HEIGHT_MARGIN(sb)  ((sb)->top_margin + (sb)->bottom_margin)
#define  IS_TOO_SMALL(sb)  ((sb)->window.height - HEIGHT_MARGIN(sb) <= (sb)->line_height)

#ifdef  DEBUG

#define  MAX_BAR_HEIGHT(sb)  (IS_TOO_SMALL(sb) ? \
	0 & kik_debug_printf( KIK_DEBUG_TAG \
		" scroll bar is too small , but MAX_BAR_HEIGHT was refered.\n") : \
	(sb)->window.height - HEIGHT_MARGIN(sb))
#else

#define  MAX_BAR_HEIGHT(sb)  ((sb)->window.height - HEIGHT_MARGIN(sb))

#endif


/* --- static functions --- */

inline static void
draw_scrollbar(
	ml_scrollbar_t *  sb
	)
{
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " updating scrollbar from %d height %d\n" ,
		sb->bar_top_y , sb->bar_height) ;
#endif

	if( ! IS_TOO_SMALL(sb) && sb->view->draw_scrollbar)
	{
		(*sb->view->draw_scrollbar)( sb->view , sb->top_margin + sb->bar_top_y , sb->bar_height) ;
	}
}

inline static void
draw_decoration(
	ml_scrollbar_t *  sb
	)
{
	if( IS_TOO_SMALL(sb) || sb->view->draw_decoration == NULL)
	{
		ml_window_fill_all( &sb->window) ;
	}
	else
	{
		(*sb->view->draw_decoration)( sb->view) ;
	}
}

inline static int
calculate_bar_top_y(
      ml_scrollbar_t *  sb
      )
{
	if( IS_TOO_SMALL(sb) || MAX_BAR_HEIGHT(sb) == sb->bar_height ||
		abs( sb->current_row) == sb->num_of_filled_log_lines)
	{
		return  0 ;
	}
	else
	{
		return  (sb->current_row + sb->num_of_filled_log_lines) *
			(MAX_BAR_HEIGHT(sb) - sb->bar_height) / sb->num_of_filled_log_lines ;
	}
}

inline static u_int
calculate_bar_height(
	ml_scrollbar_t *  sb
	)
{
	if( IS_TOO_SMALL(sb))
	{
		return  0 ;
	}
	else
	{
		return  (sb->num_of_scr_lines * MAX_BAR_HEIGHT(sb)) /
			(sb->num_of_filled_log_lines + sb->num_of_scr_lines) ;
	}
}

inline static int
calculate_current_row(
	ml_scrollbar_t *  sb
	)
{
	if( IS_TOO_SMALL(sb) || MAX_BAR_HEIGHT(sb) == sb->bar_height)
	{
		return  0 ;
	}
	else
	{
		/*
		 * sb->bar_top_y / (sb->num_of_filled_log_lines / (MAX_BAR_HEIGHT(sb) - sb->bar_height))
		 * => (sb->num_of_filled_log_lines / (MAX_BAR_HEIGHT(sb) - sb->bar_height)) = pixel per line
		 */
		return  sb->bar_top_y * sb->num_of_filled_log_lines / (MAX_BAR_HEIGHT(sb) - sb->bar_height) -
			sb->num_of_filled_log_lines ;
	}
}

static int
is_updown_button_event(
	ml_scrollbar_t *  sb ,
	int  y		/* this value must include margin or be y on actual window */
	)
{
	int  up_button_y ;
	int  down_button_y ;

	/*
	 * minus value means y from the bottom.
	 */
	 
	if( sb->up_button_y < 0)
	{
		up_button_y = sb->window.height + sb->up_button_y ;
	}
	else
	{
		up_button_y = sb->up_button_y ;
	}
	
	if( sb->down_button_y < 0)
	{
		down_button_y = sb->window.height + sb->down_button_y ;
	}
	else
	{
		down_button_y = sb->down_button_y ;
	}
	
	if( up_button_y <= y && y <= up_button_y + sb->up_button_height)
	{
	#ifdef  __DEBUG
		kik_debug_printf( "up button pressed\n") ;
	#endif

		return  1 ;
	}
	else if( down_button_y <= y && y <= down_button_y + sb->down_button_height)
	{
	#ifdef  __DEBUG
		kik_debug_printf( "down button pressed\n") ;
	#endif
	
		return  -1 ;
	}
	else
	{
		return  0 ;
	}
}

/*
 * callbacks of ml_window_t events.
 */
 
static void
window_realized(
	ml_window_t *  win
	)
{
	ml_scrollbar_t *  sb ;

	sb = (ml_scrollbar_t*) win ;

	if( sb->view->realized)
	{
		(*sb->view->realized)( sb->view , sb->window.display , sb->window.screen ,
			sb->window.my_window , sb->window.gc , sb->window.height) ;
	}
	
	draw_decoration( sb) ;
	draw_scrollbar( sb) ;
}

static void
window_resized(
	ml_window_t *  win
	)
{
	ml_scrollbar_t *  sb ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " scrollbar resized.\n") ;
#endif

	sb = (ml_scrollbar_t*) win ;

	if( IS_TOO_SMALL(sb))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG  " scrollbar is too small to be drawn.\n") ;
	#endif

		sb->num_of_scr_lines = 0 ;
	}
	else
	{
		sb->num_of_scr_lines = MAX_BAR_HEIGHT(sb) / sb->line_height ;
	}

	sb->bar_height = calculate_bar_height( sb) ;

	sb->bar_top_y = MAX_BAR_HEIGHT(sb) - sb->bar_height ;

	if( sb->view->resized)
	{
		(*sb->view->resized)( sb->view , sb->window.my_window , sb->window.height) ;
	}
	
	draw_decoration( sb) ;
	draw_scrollbar( sb) ;
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
	ml_scrollbar_t *  sb ;

	sb = (ml_scrollbar_t*) win ;

	draw_decoration( sb) ;
	draw_scrollbar( sb) ;
}

static void
up_button_pressed(
	ml_scrollbar_t *  sb
	)
{
	if( ! ml_scrollbar_move_upward( sb , 1))
	{
		return ;
	}

	if( sb->sb_listener->screen_scroll_downward)
	{
		/* down button scrolls *down* screen */
		(*sb->sb_listener->screen_scroll_downward)( sb->sb_listener->self , 1) ;
	}
}

static void
down_button_pressed(
	ml_scrollbar_t *  sb
	)
{
	if( ! ml_scrollbar_move_downward( sb , 1))
	{
		return ;
	}

	if( sb->sb_listener->screen_scroll_upward)
	{
		/* down button scrolls *up* screen */
		(*sb->sb_listener->screen_scroll_upward)( sb->sb_listener->self , 1) ;
	}
}

static void
button_pressed(
	ml_window_t *  win ,
	XButtonEvent *  event ,
	int  click_num
	)
{
	ml_scrollbar_t *  sb ;
	int  result ;
	int  y ;
	
	sb = (ml_scrollbar_t*) win ;

	result = is_updown_button_event( sb , event->y) ;

	y = event->y - sb->top_margin ;

	if( result == 0)
	{
		if( y < sb->bar_top_y)
		{
			ml_scrollbar_move_upward( sb , sb->num_of_scr_lines) ;

			if( sb->sb_listener->screen_scroll_downward)
			{
				/* down button scrolls *down* screen */
				(*sb->sb_listener->screen_scroll_downward)( sb->sb_listener->self ,
					sb->num_of_scr_lines) ;
			}
		}
		else if( y > sb->bar_top_y + sb->bar_height)
		{
			ml_scrollbar_move_downward( sb , sb->num_of_scr_lines) ;

			if( sb->sb_listener->screen_scroll_upward)
			{
				/* down button scrolls *up* screen */
				(*sb->sb_listener->screen_scroll_upward)( sb->sb_listener->self ,
					sb->num_of_scr_lines) ;
			}
		}
	}
	else if( result == 1)
	{
		sb->is_pressing_up_button = 1 ;

		if( sb->view->up_button_pressed)
		{
			(*sb->view->up_button_pressed)( sb->view) ;
		}

		up_button_pressed( sb) ;
	}
	else if( result == -1)
	{
		sb->is_pressing_down_button = 1 ;

		if( sb->view->down_button_pressed)
		{
			(*sb->view->down_button_pressed)( sb->view) ;
		}

		down_button_pressed( sb) ;
	}
}

static void
button_press_continued(
	ml_window_t *  win ,
	XButtonEvent *  event
	)
{
	ml_scrollbar_t *  sb ;
	int  result ;
	
	sb = (ml_scrollbar_t*) win ;

	result = is_updown_button_event( sb , event->y) ;
	
	if( sb->is_pressing_up_button && result == 1)
	{
		up_button_pressed( sb) ;
	}
	else if( sb->is_pressing_down_button && result == -1)
	{
		down_button_pressed( sb) ;
	}
}

static void
button_motion(
	ml_window_t *  win ,
	XMotionEvent *  event
	)
{
	ml_scrollbar_t *  sb ;
	int  new_row ;
	int  up_to_top_now ;
	int  y ;
	
	sb = (ml_scrollbar_t*) win ;

	if( sb->is_pressing_up_button || sb->is_pressing_down_button ||
		is_updown_button_event( sb , event->y) != 0)
	{
		return ;
	}

	y = event->y - sb->top_margin ;
	
	if( sb->bar_top_y == 0)
	{
		up_to_top_now = 1 ;
	}
	else
	{
		up_to_top_now = 0 ;
	}

	if( sb->is_motion == 0)
	{
		if( sb->bar_top_y <= y && y <= sb->bar_top_y + sb->bar_height)
		{
			/* on the bar */

			sb->y_on_bar = y - sb->bar_top_y ;
		}
		else
		{
			/* out of the bar */

			sb->y_on_bar = sb->bar_height / 2 ;

			if( y < sb->y_on_bar)
			{
				sb->bar_top_y = 0 ;
			}
			else
			{
				sb->bar_top_y = y - sb->y_on_bar ;
			}
		}
		
		sb->is_motion = 1 ;
	}
	else
	{
		if( y < sb->y_on_bar)
		{
			if( sb->bar_top_y != 0)
			{
				sb->bar_top_y = 0 ;
			}
			else
			{
				return  ;
			}
		}
		else if( y - sb->y_on_bar + sb->bar_height > MAX_BAR_HEIGHT(sb))
		{
			sb->bar_top_y = MAX_BAR_HEIGHT(sb) - sb->bar_height ;
		}
		else
		{
			sb->bar_top_y = y - sb->y_on_bar ;
		}
	}

	if( ! up_to_top_now && sb->bar_top_y == 0)
	{
		/* up to the top this time */
		
		up_to_top_now = 1 ;
	}
	else
	{
		/* if bar is on the top , it is not *this* time(maybe previous...) */
		
		up_to_top_now = 0 ;
	}
	
	new_row = calculate_current_row( sb) ;

	/*
	 * if bar reaches the top this time , it doesn't return but draw_scrollbar().
	 */
	if( ! up_to_top_now && sb->current_row == new_row)
	{
		return ;
	}
	
	sb->current_row = new_row ;

	if( sb->sb_listener->screen_scroll_to)
	{
		(*sb->sb_listener->screen_scroll_to)( sb->sb_listener->self , sb->current_row) ;
	}

	draw_scrollbar( sb) ;
}

static void
button_released(
	ml_window_t *  win ,
	XButtonEvent *  event
	)
{
	ml_scrollbar_t *  sb ;
	
	sb = (ml_scrollbar_t*) win ;

	if( sb->is_pressing_up_button)
	{
		if( sb->view->up_button_released)
		{
			(*sb->view->up_button_released)( sb->view) ;
		}

		sb->is_pressing_up_button = 0 ;
	}
	
	if( sb->is_pressing_down_button)
	{
		if( sb->view->down_button_released)
		{
			(*sb->view->down_button_released)( sb->view) ;
		}
		
		sb->is_pressing_down_button = 0 ;
	}
}


/* --- global functions --- */

int
ml_scrollbar_init(
	ml_scrollbar_t *  sb ,
	ml_scrollbar_event_listener_t *  sb_listener ,
	char *  view_name ,
	ml_color_table_t  color_table ,
	u_int  height ,
	u_int  line_height ,
	u_int  num_of_log_lines ,
	int  use_transbg
	)
{
	u_int  width ;
	char *  fg_color_name ;
	char *  bg_color_name ;

	/* dynamically allocated */
	sb->view_name = NULL ;
	sb->view = NULL ;

	if( view_name)
	{
		sb->view_name = strdup( view_name) ;
	}
	else
	{
		sb->view_name = strdup( "simple") ;
	}

	if( sb->view_name == NULL)
	{
		goto  error ;
	}

	if( use_transbg)
	{
		if( ( sb->view = ml_transparent_scrollbar_view_new( sb->view_name)))
		{
			goto  view_created ;
		}
		else
		{
			/* transparency is not used */
			
			use_transbg = 0 ;
		}
	}
	
	if( ( sb->view = ml_sb_view_new( sb->view_name)) == NULL)
	{
		free( sb->view_name) ;
		
		if( ( sb->view_name = strdup( "simple")) == NULL)
		{
			goto  error ;
		}

		if( ( sb->view = ml_sb_view_new( sb->view_name)) == NULL)
		{
			goto  error ;
		}
	}

view_created:
	sb->sb_listener = sb_listener ;

	(*sb->view->get_geometry_hints)( sb->view , &width , &sb->top_margin , &sb->bottom_margin ,
		&sb->up_button_y , &sb->up_button_height , &sb->down_button_y , &sb->down_button_height) ;

	if( sb->view->get_default_color)
	{
		(*sb->view->get_default_color)( sb->view , &fg_color_name , &bg_color_name) ;
	}
	else
	{
		fg_color_name = NULL ;
		bg_color_name = NULL ;
	}

	sb->is_pressing_up_button = 0 ;
	sb->is_pressing_down_button = 0 ;

	if( ml_window_init( &sb->window , color_table , width , height , 0 , 0 , 0 , 0 , 0) == 0)
	{
		goto  error ;
	}

	if( ml_window_get_fg_color( &sb->window) == MLC_UNKNOWN_COLOR)
	{
		ml_color_t  fg_color ;

		if( fg_color_name == NULL || 
			( fg_color = ml_get_color( fg_color_name)) == MLC_UNKNOWN_COLOR)
		{
			fg_color = MLC_GRAY ;
		}

		ml_window_set_fg_color( &sb->window , fg_color) ;
	}

	if( ml_window_get_bg_color( &sb->window) == MLC_UNKNOWN_COLOR)
	{
		ml_color_t  bg_color ;

		if( bg_color_name == NULL ||
			( bg_color = ml_get_color( bg_color_name)) == MLC_UNKNOWN_COLOR)
		{
			bg_color = MLC_LIGHTGRAY ;
		}
		
		ml_window_set_bg_color( &sb->window , bg_color) ;
	}

	sb->line_height = line_height ;

	if( IS_TOO_SMALL(sb))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG  " scrollbar is too small to be drawn.\n") ;
	#endif

		sb->bar_height = 0 ;
		sb->num_of_scr_lines = 0 ;
	}
	else
	{
		sb->bar_height = height - HEIGHT_MARGIN(sb) ;
		sb->num_of_scr_lines = sb->bar_height / sb->line_height ;
	}
	
	sb->num_of_log_lines = num_of_log_lines ;
	sb->num_of_filled_log_lines = 0 ;
	sb->bar_top_y = 0 ;
	sb->y_on_bar = 0 ;
	sb->current_row = 0 ;
	sb->is_motion = 0 ;

	if( use_transbg)
	{
		ml_window_set_transparent( &sb->window) ;
	}

	/* default scrollbar cursor */	
	ml_window_set_cursor( &sb->window , XC_sb_v_double_arrow) ;

	/*
	 * event callbacks.
	 */
	ml_window_init_event_mask( &sb->window , ButtonPressMask | ButtonReleaseMask | ButtonMotionMask) ;
	sb->window.window_realized = window_realized ;
	sb->window.button_pressed = button_pressed ;
	sb->window.button_released = button_released ;
	sb->window.button_press_continued = button_press_continued ;
	sb->window.button_motion = button_motion ;
	sb->window.window_resized = window_resized ;
	sb->window.window_exposed = window_exposed ;

	return  1 ;

error:
	if( sb->view_name)
	{
		free( sb->view_name) ;
	}
	
	if( sb->view)
	{
		(*sb->view->delete)(sb->view) ;
	}

	return  0 ;
}

int
ml_scrollbar_final(
	ml_scrollbar_t *  sb
	)
{
	free( sb->view_name) ;
	(*sb->view->delete)( sb->view) ;
	
	return  1 ;
}

int
ml_scrollbar_line_is_added(
	ml_scrollbar_t *  sb
	)
{
	int  old_y ;
	
	if( sb->num_of_filled_log_lines == sb->num_of_log_lines)
	{
		return  0 ;
	}

	sb->num_of_filled_log_lines ++ ;

	sb->bar_height = calculate_bar_height( sb) ;

	old_y = sb->bar_top_y ;
	sb->bar_top_y = MAX_BAR_HEIGHT(sb) - sb->bar_height ;
	
	if( old_y == sb->bar_top_y)
	{
		return  1 ;
	}
	else
	{
		draw_scrollbar( sb) ;

		return  1 ;
	}
}

int
ml_scrollbar_reset(
	ml_scrollbar_t *  sb
	)
{
	if( sb->is_motion || sb->bar_top_y < MAX_BAR_HEIGHT(sb) - sb->bar_height)
	{
		sb->bar_top_y = MAX_BAR_HEIGHT(sb) - sb->bar_height ;
		sb->is_motion = 0 ;
		sb->current_row = 0 ;

		draw_scrollbar( sb) ;
	}
	
	return  1 ;
}

int
ml_scrollbar_move_upward(
	ml_scrollbar_t *  sb ,
	u_int  size
	)
{
	if( sb->bar_top_y == 0)
	{
		return  0 ;
	}

	if( sb->num_of_filled_log_lines < abs( sb->current_row) + size)
	{
		sb->current_row = -(sb->num_of_filled_log_lines) ;
	}
	else
	{
		sb->current_row -= size ;
	}
	
	sb->bar_top_y = calculate_bar_top_y(sb) ;

	draw_scrollbar( sb) ;

	return  1 ;
}

int
ml_scrollbar_move_downward(
	ml_scrollbar_t *  sb ,
	u_int  size
	)
{
	if( sb->current_row >= 0)
	{
		return  0 ;
	}

	if( abs( sb->current_row) < size)
	{
		sb->current_row = 0 ;
	}
	else
	{
		sb->current_row += size ;
	}
	
	sb->bar_top_y = calculate_bar_top_y(sb) ;
	
	draw_scrollbar( sb) ;

	return  1 ;
}

int
ml_scrollbar_set_num_of_log_lines(
	ml_scrollbar_t *  sb ,
	u_int  num_of_log_lines
	)
{
	sb->num_of_log_lines = num_of_log_lines ;

	if( sb->num_of_filled_log_lines > sb->num_of_log_lines)
	{
		sb->num_of_filled_log_lines = sb->num_of_log_lines ;
	}

	sb->bar_height = calculate_bar_height( sb) ;

	sb->bar_top_y = MAX_BAR_HEIGHT(sb) - sb->bar_height ;
	
	draw_scrollbar( sb) ;

	return  1 ;
}

int
ml_scrollbar_set_line_height(
	ml_scrollbar_t *  sb ,
	u_int  line_height
	)
{
	sb->line_height = line_height ;

	sb->bar_height = calculate_bar_height( sb) ;

	draw_decoration( sb) ;
	draw_scrollbar( sb) ;
	
	return  1 ;
}

int
ml_scrollbar_set_transparent(
	ml_scrollbar_t *  sb
	)
{
	ml_sb_view_t *  view ;

	if( sb->window.is_transparent)
	{
		/* already set */
		
		return  1 ;
	}

	if( ( view = ml_transparent_scrollbar_view_new( sb->view_name)) == NULL)
	{
		/* nothing is done */
		
		return  0 ;
	}

	if( sb->view)
	{
		(*sb->view->delete)( sb->view) ;
		ml_unload_prev_scrollbar_view() ;
	}

	sb->view = view ;

	if( sb->view->realized)
	{
		/*
		 * this should be done before ml_window_set_transparent() , which calls
		 * exposed event.
		 */
		(*sb->view->realized)( sb->view , sb->window.display , sb->window.screen ,
			sb->window.my_window , sb->window.gc , sb->window.height) ;
	}
	
	ml_window_set_transparent( &sb->window) ;

	draw_decoration( sb) ;
	draw_scrollbar( sb) ;
	
	return  1 ;
}

int
ml_scrollbar_unset_transparent(
	ml_scrollbar_t *  sb
	)
{
	ml_sb_view_t *  view ;

	if( ! sb->window.is_transparent)
	{
		/* already unset */
		
		return  1 ;
	}
	
	if( ( view = ml_sb_view_new( sb->view_name)) == NULL)
	{
		/* nothing is done */
		
		return  0 ;
	}

	if( sb->view)
	{
		(*sb->view->delete)( sb->view) ;
	}

	sb->view = view ;

	if( sb->view->realized)
	{
		/*
		 * this should be done before ml_window_set_transparent() , which calls
		 * exposed event.
		 */
		(*sb->view->realized)( sb->view , sb->window.display , sb->window.screen ,
			sb->window.my_window , sb->window.gc , sb->window.height) ;
	}
	
	ml_window_unset_transparent( &sb->window) ;

	draw_decoration( sb) ;
	draw_scrollbar( sb) ;
	
	return  1 ;
}
