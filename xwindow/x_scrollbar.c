/*
 *	$Id$
 */

#include  "x_scrollbar.h"

#include  <stdlib.h>		/* abs */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* free */
#include  <kiklib/kik_str.h>	/* strdup */

#include  "x_sb_view_factory.h"


#if  0
#define  __DEBUG
#endif


#define  HEIGHT_MARGIN(sb)  ((sb)->top_margin + (sb)->bottom_margin)
#define  IS_TOO_SMALL(sb)  ((sb)->window.height <= HEIGHT_MARGIN(sb))

#ifdef  DEBUG
#define  MAX_BAR_HEIGHT(sb)  (IS_TOO_SMALL(sb) ? \
	0 & kik_debug_printf( KIK_DEBUG_TAG \
		" scroll bar is too small , but MAX_BAR_HEIGHT was refered.\n") : \
	(sb)->window.height - HEIGHT_MARGIN(sb))
#else
#define  MAX_BAR_HEIGHT(sb)  ((sb)->window.height - HEIGHT_MARGIN(sb))
#endif


/* --- static functions --- */

static void
draw_scrollbar(
	x_scrollbar_t *  sb
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

static void
draw_decoration(
	x_scrollbar_t *  sb
	)
{
	if( IS_TOO_SMALL(sb) || sb->view->draw_decoration == NULL)
	{
		x_window_fill_all( &sb->window) ;
	}
	else
	{
		(*sb->view->draw_decoration)( sb->view) ;
	}
}

/*
 * depends on sb->bar_height.
 */
static int
calculate_bar_top_y(
      x_scrollbar_t *  sb
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

/*
 * depends on sb->bar_height.
 */
static int
calculate_current_row(
	x_scrollbar_t *  sb
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

static u_int
calculate_bar_height(
	x_scrollbar_t *  sb
	)
{
	if( IS_TOO_SMALL(sb) || sb->num_of_filled_log_lines + sb->num_of_scr_lines == 0)
	{
		return  0 ;
	}
	else
	{
		u_int  bar_height ;
		
		bar_height = (sb->num_of_scr_lines * MAX_BAR_HEIGHT(sb)) /
				(sb->num_of_filled_log_lines + sb->num_of_scr_lines) ;
		
		if( bar_height < MAX_BAR_HEIGHT(sb) / 20)
		{
			bar_height = MAX_BAR_HEIGHT(sb) / 20 ;
		}

		return  bar_height ;
	}
}

static int
is_updown_button_event(
	x_scrollbar_t *  sb ,
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
 * callbacks of x_window_t events.
 */
 
static void
window_realized(
	x_window_t *  win
	)
{
	x_scrollbar_t *  sb ;

	sb = (x_scrollbar_t*) win ;

	x_load_named_xcolor( win->display , win->screen , &sb->fg_xcolor , sb->fg_color) ;
	x_load_named_xcolor( win->display , win->screen , &sb->bg_xcolor , sb->bg_color) ;
	x_window_set_fg_color( win , sb->fg_xcolor.pixel) ;
	x_window_set_bg_color( win , sb->bg_xcolor.pixel) ;

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
	x_window_t *  win
	)
{
	x_scrollbar_t *  sb ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " scrollbar resized.\n") ;
#endif

	sb = (x_scrollbar_t*) win ;

	if( IS_TOO_SMALL(sb))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG  " scrollbar is too small to be drawn.\n") ;
	#endif

		sb->num_of_scr_lines = 0 ;
		sb->bar_height = 0 ;
		sb->bar_top_y = 0 ;
	}
	else
	{
		sb->num_of_scr_lines = MAX_BAR_HEIGHT(sb) / sb->line_height ;
		sb->bar_height = calculate_bar_height( sb) ;
		sb->bar_top_y = MAX_BAR_HEIGHT(sb) - sb->bar_height ;
	}

	if( sb->view->resized)
	{
		(*sb->view->resized)( sb->view , sb->window.my_window , sb->window.height) ;
	}
	
	draw_decoration( sb) ;
	draw_scrollbar( sb) ;
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
	x_scrollbar_t *  sb ;

	sb = (x_scrollbar_t*) win ;

	draw_decoration( sb) ;
	draw_scrollbar( sb) ;
}

static void
up_button_pressed(
	x_scrollbar_t *  sb
	)
{
	if( ! x_scrollbar_move_upward( sb , 1))
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
	x_scrollbar_t *  sb
	)
{
	if( ! x_scrollbar_move_downward( sb , 1))
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
	x_window_t *  win ,
	XButtonEvent *  event ,
	int  click_num
	)
{
	x_scrollbar_t *  sb ;
	int  result ;
	int  y ;
	
	sb = (x_scrollbar_t*) win ;

	if( IS_TOO_SMALL(sb))
	{
		return ;
	}

	result = is_updown_button_event( sb , event->y) ;

	y = event->y - sb->top_margin ;

	if( result == 0)
	{
		if( y < sb->bar_top_y)
		{
			x_scrollbar_move_upward( sb , sb->num_of_scr_lines) ;

			if( sb->sb_listener->screen_scroll_downward)
			{
				/* down button scrolls *down* screen */
				(*sb->sb_listener->screen_scroll_downward)( sb->sb_listener->self ,
					sb->num_of_scr_lines) ;
			}
		}
		else if( y > sb->bar_top_y + sb->bar_height)
		{
			x_scrollbar_move_downward( sb , sb->num_of_scr_lines) ;

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
	x_window_t *  win ,
	XButtonEvent *  event
	)
{
	x_scrollbar_t *  sb ;
	int  result ;
	
	sb = (x_scrollbar_t*) win ;

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
	x_window_t *  win ,
	XMotionEvent *  event
	)
{
	x_scrollbar_t *  sb ;
	int  new_row ;
	int  up_to_top_now ;
	int  y ;
	
	sb = (x_scrollbar_t*) win ;

	if( sb->is_pressing_up_button || sb->is_pressing_down_button ||
		is_updown_button_event( sb , event->y) != 0 ||
		IS_TOO_SMALL(sb))
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
	x_window_t *  win ,
	XButtonEvent *  event
	)
{
	x_scrollbar_t *  sb ;
	
	sb = (x_scrollbar_t*) win ;

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
x_scrollbar_init(
	x_scrollbar_t *  sb ,
	x_scrollbar_event_listener_t *  sb_listener ,
	char *  view_name ,
	char *  fg_color ,
	char *  bg_color ,
	u_int  height ,
	u_int  line_height ,
	u_int  num_of_log_lines ,
	u_int  num_of_filled_log_lines ,
	int  use_transbg ,
	x_picture_modifier_t *  pic_mod
	)
{
	u_int  width ;

	/* dynamically allocated */
	sb->view_name = NULL ;
	sb->view = NULL ;
	sb->fg_color = NULL ;
	sb->bg_color = NULL ;

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
		if( ( sb->view = x_transparent_scrollbar_view_new( sb->view_name)))
		{
			goto  view_created ;
		}
	}

	if( ( sb->view = x_sb_view_new( sb->view_name)) == NULL)
	{
		free( sb->view_name) ;
		
		if( ( sb->view_name = strdup( "simple")) == NULL)
		{
			goto  error ;
		}

		if( use_transbg)
		{
			if( ( sb->view = x_transparent_scrollbar_view_new( sb->view_name)))
			{
				goto  view_created ;
			}
		}
		
		if( ( sb->view = x_sb_view_new( sb->view_name)) == NULL)
		{
			goto  error ;
		}
	}
	
	use_transbg = 0 ;

view_created:
	sb->sb_listener = sb_listener ;

	(*sb->view->get_geometry_hints)( sb->view , &width , &sb->top_margin , &sb->bottom_margin ,
		&sb->up_button_y , &sb->up_button_height , &sb->down_button_y , &sb->down_button_height) ;

	if( sb->view->get_default_color)
	{
		char *  _fg_color ;
		char *  _bg_color ;

		(*sb->view->get_default_color)( sb->view , &_fg_color , &_bg_color) ;

		if( fg_color == NULL)
		{
			fg_color = _fg_color ;
		}

		if( bg_color == NULL)
		{
			bg_color = _bg_color ;
		}
	}
	else
	{
		if( fg_color == NULL)
		{
			fg_color = "black" ;
		}

		if( bg_color == NULL)
		{
			bg_color = "white" ;
		}
	}
	
	sb->fg_color = strdup( fg_color) ;
	sb->bg_color = strdup( bg_color) ;
	
	sb->is_pressing_up_button = 0 ;
	sb->is_pressing_down_button = 0 ;

	if( x_window_init( &sb->window , width , height , width , 0 , width , 0 , 0 , 0 , 0) == 0)
	{
		goto  error ;
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
	sb->num_of_filled_log_lines = num_of_filled_log_lines ;
	sb->bar_top_y = 0 ;
	sb->y_on_bar = 0 ;
	sb->current_row = 0 ;
	sb->is_motion = 0 ;

	if( use_transbg)
	{
		x_window_set_transparent( &sb->window , pic_mod) ;
	}

	/* default scrollbar cursor */	
	x_window_set_cursor( &sb->window , XC_sb_v_double_arrow) ;

	/*
	 * event callbacks.
	 */
	x_window_init_event_mask( &sb->window , ButtonPressMask | ButtonReleaseMask | ButtonMotionMask) ;
	sb->window.window_realized = window_realized ;
	sb->window.button_pressed = button_pressed ;
	sb->window.button_released = button_released ;
	sb->window.button_press_continued = button_press_continued ;
	sb->window.button_motion = button_motion ;
	sb->window.window_resized = window_resized ;
	sb->window.window_exposed = window_exposed ;

	return  1 ;

error:
	if( sb->fg_color)
	{
		free( sb->fg_color) ;
	}
	
	if( sb->bg_color)
	{
		free( sb->bg_color) ;
	}
	
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
x_scrollbar_final(
	x_scrollbar_t *  sb
	)
{
	(*sb->view->delete)( sb->view) ;
	x_unload_scrollbar_view_lib( sb->view_name) ;

	x_unload_xcolor( sb->window.display , sb->window.screen , &sb->fg_xcolor) ;
	x_unload_xcolor( sb->window.display , sb->window.screen , &sb->bg_xcolor) ;
	free( sb->fg_color) ;
	free( sb->bg_color) ;
	free( sb->view_name) ;
	
	return  1 ;
}

int
x_scrollbar_set_num_of_log_lines(
	x_scrollbar_t *  sb ,
	u_int  num_of_log_lines
	)
{
	if( sb->num_of_log_lines == num_of_log_lines)
	{
		return  1 ;
	}
	
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
x_scrollbar_set_num_of_filled_log_lines(
	x_scrollbar_t *  sb ,
	u_int  lines
	)
{
	if( lines > sb->num_of_log_lines)
	{
		lines = sb->num_of_log_lines ;
	}

	if( sb->num_of_filled_log_lines == lines)
	{
		return  1 ;
	}
	
	sb->num_of_filled_log_lines = lines ;

	sb->bar_height = calculate_bar_height( sb) ;
	
	sb->bar_top_y = MAX_BAR_HEIGHT(sb) - sb->bar_height ;
			
	draw_scrollbar( sb) ;

	return  1 ;
}

int
x_scrollbar_line_is_added(
	x_scrollbar_t *  sb
	)
{
	int  old_y ;
	u_int  old_bar_height ;
	
	if( (*sb->sb_listener->screen_is_static)(sb->sb_listener->self))
	{
		if( sb->num_of_filled_log_lines < sb->num_of_log_lines)
		{
			sb->num_of_filled_log_lines ++ ;
		}

		sb->current_row -- ;
	}
	else if( sb->num_of_filled_log_lines == sb->num_of_log_lines)
	{
		return  0 ;
	}
	else
	{
		sb->num_of_filled_log_lines ++ ;
	}

	old_bar_height = sb->bar_height ;
	sb->bar_height = calculate_bar_height( sb) ;
	
	old_y = sb->bar_top_y ;
	sb->bar_top_y = calculate_bar_top_y( sb) ;
	
	if( old_y == sb->bar_top_y && old_bar_height == sb->bar_height)
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
x_scrollbar_reset(
	x_scrollbar_t *  sb
	)
{
	if( sb->is_motion || sb->bar_top_y + sb->bar_height < MAX_BAR_HEIGHT(sb))
	{
		sb->bar_top_y = MAX_BAR_HEIGHT(sb) - sb->bar_height ;
		sb->is_motion = 0 ;
		sb->current_row = 0 ;

		draw_scrollbar( sb) ;
	}
	
	return  1 ;
}

int
x_scrollbar_move_upward(
	x_scrollbar_t *  sb ,
	u_int  size
	)
{
#if  0
	if( sb->bar_top_y == 0)
#else
	/*
	 * XXX Adhoc solution
	 * Fix x_screen.c:bs_{half_}page_{up|down}ward() instead.
	 */
	if( sb->current_row + sb->num_of_filled_log_lines == 0)
#endif
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
x_scrollbar_move_downward(
	x_scrollbar_t *  sb ,
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
x_scrollbar_set_line_height(
	x_scrollbar_t *  sb ,
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
x_scrollbar_set_fg_color(
	x_scrollbar_t *  sb ,
	char *  fg_color
	)
{
	free( sb->fg_color) ;
	x_unload_xcolor( sb->window.display , sb->window.screen , &sb->fg_xcolor) ;
	
	sb->fg_color = strdup( fg_color) ;
	x_load_named_xcolor( sb->window.display , sb->window.screen , &sb->fg_xcolor , sb->fg_color) ;
	x_window_set_fg_color( &sb->window , sb->fg_xcolor.pixel) ;
	
	draw_decoration( sb) ;
	draw_scrollbar( sb) ;

	return  1 ;
}

int
x_scrollbar_set_bg_color(
	x_scrollbar_t *  sb ,
	char *  bg_color
	)
{
	free( sb->bg_color) ;
	x_unload_xcolor( sb->window.display , sb->window.screen , &sb->bg_xcolor) ;
	
	sb->bg_color = strdup( bg_color) ;
	x_load_named_xcolor( sb->window.display , sb->window.screen , &sb->bg_xcolor , sb->bg_color) ;
	x_window_set_bg_color( &sb->window , sb->bg_xcolor.pixel) ;

	draw_decoration( sb) ;
	draw_scrollbar( sb) ;

	return  1 ;
}

int
x_scrollbar_change_view(
	x_scrollbar_t *  sb ,
	char *  name
	)
{
	x_sb_view_t *  view ;
	u_int  width ;
	
	if( strcmp( name , sb->view_name) == 0 || ( name = strdup( name)) == NULL)
	{
		return  0 ;
	}

	if( sb->window.is_transparent)
	{
		if( ( view = x_transparent_scrollbar_view_new( name)) == NULL)
		{
			/* nothing is done */

			free( name) ;

			return  0 ;
		}
	}
	else
	{
		if( ( view = x_sb_view_new( name)) == NULL)
		{
			free( name) ;

			return  0 ;
		}
	}

	if( sb->view)
	{
		(*sb->view->delete)( sb->view) ;
		x_unload_scrollbar_view_lib( sb->view_name) ;
	}

	sb->view = view ;

	if( sb->view_name)
	{
		free( sb->view_name) ;
	}

	/* name is dynamically allocated above */
	sb->view_name = name ;
	
	(*sb->view->get_geometry_hints)( sb->view , &width , &sb->top_margin , &sb->bottom_margin ,
		&sb->up_button_y , &sb->up_button_height , &sb->down_button_y , &sb->down_button_height) ;

	sb->bar_height = calculate_bar_height( sb) ;
	sb->bar_top_y = calculate_bar_top_y(sb) ;

	if( sb->view->realized)
	{
		(*sb->view->realized)( sb->view , sb->window.display , sb->window.screen ,
			sb->window.my_window , sb->window.gc , sb->window.height) ;
	}

	if( sb->window.width != width)
	{
		x_window_set_normal_hints( &sb->window ,
			width , sb->window.min_height ,
			width , sb->window.min_height , 0 , 0) ;

		x_window_resize( &sb->window , width , sb->window.height , NOTIFY_TO_PARENT) ;
	}
	
	draw_decoration( sb) ;
	draw_scrollbar( sb) ;
	
	return  1 ;
}

int
x_scrollbar_set_transparent(
	x_scrollbar_t *  sb ,
	x_picture_modifier_t *  pic_mod ,
	int  force
	)
{
	x_sb_view_t *  view ;

	if( ! force && sb->window.is_transparent)
	{
		/* already set */
		
		return  1 ;
	}

	if( ( view = x_transparent_scrollbar_view_new( sb->view_name)) == NULL)
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
		 * this should be done before x_window_set_transparent() , which calls
		 * exposed event.
		 */
		(*sb->view->realized)( sb->view , sb->window.display , sb->window.screen ,
			sb->window.my_window , sb->window.gc , sb->window.height) ;
	}

	x_window_set_transparent( &sb->window , pic_mod) ;

	draw_decoration( sb) ;
	draw_scrollbar( sb) ;
	
	return  1 ;
}

int
x_scrollbar_unset_transparent(
	x_scrollbar_t *  sb
	)
{
	x_sb_view_t *  view ;

	if( ! sb->window.is_transparent)
	{
		/* already unset */
		
		return  1 ;
	}
	
	if( ( view = x_sb_view_new( sb->view_name)) == NULL)
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
		 * this should be done before x_window_set_transparent() , which calls
		 * exposed event.
		 */
		(*sb->view->realized)( sb->view , sb->window.display , sb->window.screen ,
			sb->window.my_window , sb->window.gc , sb->window.height) ;
	}

	x_window_unset_transparent( &sb->window) ;
	
	draw_decoration( sb) ;
	draw_scrollbar( sb) ;
	
	return  1 ;
}
