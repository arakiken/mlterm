/*
 *	$Id$
 */

#include  "x_im_status_screen.h"

#include  "ml_str.h"
#include  "x_draw_str.h"

#define  MARGIN		3
#define  LINE_SPACE	2


/* --- static variables --- */


/* --- static functions --- */

static void
draw_screen(
	x_im_status_screen_t *  stat_screen
	)
{
	x_font_t *  xfont ;
	u_int  width = 0 ;
	int  i ;

	/*
	 * resize window
	 */

	/* width of window */
	for( i = 0 ; i < stat_screen->filled_len ; i++)
	{
		x_font_t *  xfont ;

		xfont = x_get_font( stat_screen->font_man ,
				    ml_char_font( &stat_screen->chars[i])) ;

		width += x_calculate_char_width(
					xfont ,
					ml_char_bytes( &stat_screen->chars[i]) ,
					ml_char_size( &stat_screen->chars[i]) ,
					ml_char_cs( &stat_screen->chars[i])) ;
	}

	xfont = x_get_usascii_font( stat_screen->font_man) ;

	x_window_resize( &stat_screen->window , width ,
			 xfont->height + LINE_SPACE, 0) ;

	/*
	 * draw border
	 */
	x_window_draw_rect_frame( &stat_screen->window , 0 , 0 ,
				  width + MARGIN*2 - 1 ,
				  xfont->height + MARGIN*2 - 1);

	x_draw_str_to_eol( &stat_screen->window ,
			   stat_screen->font_man ,
			   stat_screen->color_man ,
			   stat_screen->chars ,
			   stat_screen->filled_len ,
			   0 , 0 ,
			   xfont->height + LINE_SPACE ,
			   xfont->height_to_baseline + LINE_SPACE / 2 ,
			   LINE_SPACE / 2 ,
			   LINE_SPACE / 2 + LINE_SPACE % 2) ;
}


/*
 * methods of x_im_status_screen_t
 */

static int
delete(
	x_im_status_screen_t *  stat_screen
	)
{
	x_window_manager_remove_root( stat_screen->window.win_man ,
				      &stat_screen->window) ;

	if( stat_screen->chars)
	{
		ml_str_delete( stat_screen->chars , stat_screen->num_of_chars) ;
	}

	free( stat_screen) ;

	return  1 ;
}

static int
show(
	x_im_status_screen_t *  stat_screen
	)
{
	x_window_map( &stat_screen->window) ;

	return  1 ;
}

static int
hide(
	x_im_status_screen_t *  stat_screen
	)
{
	if( stat_screen->is_focused)
	{
		return  1 ;
	}

	x_window_unmap( &stat_screen->window) ;

	return  1 ;
}

static int
set_spot(
	x_im_status_screen_t *  stat_screen ,
	int  x ,
	int  y
	)
{
	if( stat_screen->window.x != x || stat_screen->window.y != y)
	{
		x_window_move( &stat_screen->window , x , y) ;
	}

	return  1 ;
}

static int
set(
	x_im_status_screen_t *  stat_screen ,
	mkf_parser_t *  parser ,
	u_char *  str
	)
{
	int  count = 0 ;
	mkf_char_t  ch ;
	ml_char_t *  p ;

	/*
	 * count number of characters to allocate status[index].chars
	 */

	(*parser->init)( parser) ;
	(*parser->set_str)( parser , str , strlen( str)) ;

	while( (*parser->next_char)( parser , &ch))
	{
		count++ ;
	}

	if( stat_screen->chars)
	{
		ml_str_delete( stat_screen->chars , stat_screen->num_of_chars) ;
		stat_screen->num_of_chars = 0 ;
		stat_screen->filled_len = 0 ;
	}

	if( ! ( stat_screen->chars = ml_str_new( count)))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_str_new() failed.\n") ;
	#endif

		return  0 ;
	}

	stat_screen->num_of_chars = count ;

	/*
	 * u_char -> ml_char_t
	 */

	(*parser->init)( parser) ;
	(*parser->set_str)( parser , str , strlen(str)) ;

	p = stat_screen->chars;

	ml_str_init( p , stat_screen->num_of_chars) ;

	while( (*parser->next_char)( parser , &ch))
	{
		int  is_biwidth = 0 ;
		int  is_comb = 0 ;

		if( ch.cs == ISO10646_UCS4_1)
		{
			if( ch.property & MKF_BIWIDTH)
			{
				is_biwidth = 1 ;
			}
			else if( ch.property & MKF_AWIDTH)
			{
				/* TODO: check col_size_of_width_a */
				is_biwidth = 1 ;
			}
		}

		if( ch.property & MKF_COMBINING)
		{
			is_comb = 1 ;
		}

		if( is_comb)
		{
			if( ml_char_combine( p - 1 ,
					     ch.ch , ch.size , ch.cs ,
					     is_biwidth , is_comb ,
					     ML_FG_COLOR , ML_BG_COLOR ,
					     0 , 0))
			{
				continue;
			}

			/*
			 * if combining failed , char is normally appended.
			 */
		}

		if( ml_is_msb_set( ch.cs))
		{
			SET_MSB( ch.ch[0]) ;
		}

		ml_char_set( p , ch.ch , ch.size , ch.cs ,
			     is_biwidth , is_comb ,
			     ML_FG_COLOR , ML_BG_COLOR ,
			     0 , 0) ;

		p++ ;
		stat_screen->filled_len++ ;
	}

	draw_screen( stat_screen) ;

	return  1 ;
}


/*
 * callbacks of x_window events
 */

static void
window_realized(
	x_window_t *  win
	)
{
	x_im_status_screen_t *  stat_screen ;

	stat_screen = (x_im_status_screen_t*) win ;

	x_window_set_fg_color( win , x_get_color( stat_screen->color_man ,
						  ML_FG_COLOR)->pixel) ;
	x_window_set_bg_color( win , x_get_color( stat_screen->color_man ,
						  ML_BG_COLOR)->pixel) ;

	x_window_set_override_redirect( &stat_screen->window , 1) ;
}

static void
window_finalized(
	x_window_t *  win
	)
{
	/* do nothing */
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
	draw_screen( (x_im_status_screen_t *) win) ;
}

static void
window_focused(
	x_window_t *  win
	)
{
	((x_im_status_screen_t *) win)->is_focused = 1 ;
}

static void
window_unfocused(
	x_window_t *  win
	)
{
	((x_im_status_screen_t *) win)->is_focused = 0 ;
}


/* --- global functions --- */

x_im_status_screen_t *
x_im_status_screen_new(
	x_window_manager_t *  win_man ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	int  is_vertical ,
	int  x ,
	int  y
	)
{
	x_im_status_screen_t *  stat_screen ;

	if( ( stat_screen = malloc( sizeof( x_im_status_screen_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif

		return  NULL ;
	}

	stat_screen->font_man = font_man ;
	stat_screen->color_man = color_man ;

	stat_screen->chars = NULL ;
	stat_screen->num_of_chars = 0 ;
	stat_screen->filled_len = 0 ;

	stat_screen->is_focused = 0 ;

	stat_screen->is_vertical = is_vertical ;

	if( ! x_window_init( &stat_screen->window , MARGIN * 2 , MARGIN * 2 ,
			     MARGIN * 2 , MARGIN * 2 , MARGIN * 2 , MARGIN * 2 ,
			     1 , 1 , MARGIN))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_window_init failed.\n") ;
	#endif

		goto  error ;
	}

	x_window_init_event_mask( &stat_screen->window , 0) ;


	/*
	 * +------------+
	 * | x_window.c | --- window events ---> +----------------------+
	 * +------------+                        | x_im_status_screen.c |
	 * +------------+                        +----------------------+
	 * | im plugin  | <------ methods ------
	 * +------------+
	 */

	/* callbacks for window events */
	stat_screen->window.window_realized = window_realized ;
	stat_screen->window.window_finalized = window_finalized ;
	stat_screen->window.window_exposed = window_exposed ;
	stat_screen->window.window_focused = window_focused ;
	stat_screen->window.window_unfocused = window_unfocused ;

	/* methods of x_im_status_screen_t */
	stat_screen->delete = delete ;
	stat_screen->show = show ;
	stat_screen->hide = hide ;
	stat_screen->set_spot = set_spot ;
	stat_screen->set = set ;

	if( ! x_window_manager_show_root( win_man ,
					  &stat_screen->window ,
					  x , y , 0 ,
					  "mlterm-status-window"))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_window_manager_show_root() failed.\n") ;
	#endif

		goto  error ;
	}

	return  stat_screen ;

error:
	free( stat_screen) ;

	return  NULL ;
}

