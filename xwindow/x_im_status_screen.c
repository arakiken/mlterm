/*
 *	$Id$
 */

#include  "x_im_status_screen.h"

#ifdef  USE_IM_PLUGIN

#include  "ml_str.h"
#include  "ml_vt100_parser.h"
#include  "x_draw_str.h"

#define  MARGIN		3
#define  LINE_SPACE	2


/* --- static functions --- */

static void
adjust_window_position_by_size(
	x_im_status_screen_t *  stat_screen ,
	int *  x ,
	int *  y
	)
{
	if( *y + ACTUAL_HEIGHT(&stat_screen->window) > stat_screen->window.disp->height)
	{
		*y -= ACTUAL_HEIGHT(&stat_screen->window) ;
		if( ! stat_screen->is_vertical)
		{
			*y -= stat_screen->line_height ;
		}
	}

	if( *x + ACTUAL_WIDTH(&stat_screen->window) > stat_screen->window.disp->width)
	{
		if( stat_screen->is_vertical)
		{
			/* x_im_stat_screen doesn't know column width. */
			*x -= (ACTUAL_WIDTH(&stat_screen->window) + stat_screen->line_height) ;
		}
		else
		{
			*x = stat_screen->window.disp->width - ACTUAL_WIDTH(&stat_screen->window) ;
		}
	}
}

#ifdef  USE_FRAMEBUFFER
static void
reset_screen(
	x_window_t *  win
	)
{
	x_display_reset_input_method_window() ;
	x_window_draw_rect_frame( win , -MARGIN , -MARGIN ,
				  win->width + MARGIN - 1 ,
				  win->height + MARGIN - 1) ;
}
#endif

static int
is_nl(
	ml_char_t *  ch
	)
{
	return  ml_char_cs( ch) == US_ASCII && ml_char_code( ch) == '\n' ;
}

static void
draw_screen(
	x_im_status_screen_t *  stat_screen ,
	int  do_resize ,
	int  modified_beg	/* for canna */
	)
{
#define  MAX_ROWS ((sizeof(stat_screen->head_indexes) / sizeof(stat_screen->head_indexes[0])) - 1)
	x_font_t *  xfont ;
	u_int  line_height ;
	int *  heads ;
	u_int  i ;

	x_font_manager_set_attr( stat_screen->font_man , 0 , 0) ;
	xfont = x_get_usascii_font( stat_screen->font_man) ;
	line_height = xfont->height + LINE_SPACE ;
	heads = stat_screen->head_indexes ;

	/*
	 * resize window
	 */

	/* width of window */

	if( do_resize)
	{
		u_int  max_width ;
		u_int  tmp_max_width ;
		u_int  width ;
		u_int  rows ;

		max_width = stat_screen->window.disp->width / 2 ;
		tmp_max_width = 0 ;
		width = 0 ;
		heads[0] = 0 ;
		rows = 1 ;

		for( i = 0 ; i < stat_screen->filled_len ; i++)
		{
			if( is_nl( &stat_screen->chars[i]))
			{
				if( rows == 1 || tmp_max_width < width)
				{
					tmp_max_width = width ;
				}

				heads[rows++] = i + 1 ;

				if( rows == MAX_ROWS)
				{
					break ;
				}

				width = 0 ;
			}
			else
			{
				u_int  ch_width ;

				ch_width = x_calculate_char_width(
						x_get_font( stat_screen->font_man ,
							ml_char_font( &stat_screen->chars[i])) ,
						ml_char_code( &stat_screen->chars[i]) ,
						ml_char_cs( &stat_screen->chars[i]) , NULL) ;

				if( width + ch_width > max_width)
				{
					if( rows == 1)
					{
						tmp_max_width = max_width = width ;
					}

					heads[rows++] = i ;

					if( rows == MAX_ROWS)
					{
						break ;
					}

					width = ch_width ;
				}
				else
				{
					width += ch_width ;
				}
			}
		}

		if( tmp_max_width > 0)
		{
			max_width = tmp_max_width ;
		}

		if( rows > 1)
		{
			width = max_width ;
		}

		/* for following 'heads[i + 1] - heads[i]' */
		heads[rows] = stat_screen->filled_len ;

		if( x_window_resize( &stat_screen->window , width ,
					line_height * rows , 0))
		{
			int  x ;
			int  y ;

			x = stat_screen->x ;
			y = stat_screen->y ;
			adjust_window_position_by_size( stat_screen , &x , &y) ;

			if( stat_screen->window.x != x || stat_screen->window.y != y)
			{
				x_window_move( &stat_screen->window , x , y) ;
			}

		#ifdef  USE_FRAMEBUFFER
			reset_screen( &stat_screen->window) ;
		#endif

			modified_beg = 0 ;
		}
	}

	for( i = 0 ; heads[i] < stat_screen->filled_len ; i++)
	{
		if( heads[i + 1] > modified_beg)
		{
			u_int  len ;

			len = heads[i + 1] - heads[i] ;

			if( is_nl( &stat_screen->chars[heads[i + 1] - 1]))
			{
				len -- ;
			}

			x_draw_str_to_eol( &stat_screen->window ,
				   stat_screen->font_man ,
				   stat_screen->color_man ,
				   stat_screen->chars + heads[i] , len ,
				   0 , line_height * i ,
				   line_height ,
				   xfont->ascent + LINE_SPACE / 2 ,
				   LINE_SPACE / 2 ,
				   LINE_SPACE / 2 + LINE_SPACE % 2 ,
				   1 /* no need to draw underline */) ;
		}
	}
}


/*
 * methods of x_im_status_screen_t
 */

static int
delete(
	x_im_status_screen_t *  stat_screen
	)
{
	x_display_remove_root( stat_screen->window.disp , &stat_screen->window) ;

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
	return  x_window_map( &stat_screen->window) ;
}

static int
hide(
	x_im_status_screen_t *  stat_screen
	)
{
	return  x_window_unmap( &stat_screen->window) ;
}

static int
set_spot(
	x_im_status_screen_t *  stat_screen ,
	int  x ,
	int  y
	)
{
	stat_screen->x = x ;
	stat_screen->y = y ;

	adjust_window_position_by_size( stat_screen , &x , &y) ;

	if( stat_screen->window.x != x || stat_screen->window.y != y)
	{
		x_window_move( &stat_screen->window , x , y) ;
	#ifdef  USE_FRAMEBUFFER
		reset_screen( &stat_screen->window) ;
		draw_screen( stat_screen , 0 , 0) ;
	#endif

		return  1 ;
	}
	else
	{
		return  0 ;
	}
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
	ml_char_t *  old_chars ;
	u_int  old_num_of_chars ;
	u_int  old_filled_len ;
	int  modified_beg ;

	/*
	 * count number of characters to allocate status[index].chars
	 */

	(*parser->init)( parser) ;
	(*parser->set_str)( parser , str , strlen( str)) ;

	while( (*parser->next_char)( parser , &ch))
	{
		count++ ;
	}

	old_chars = stat_screen->chars ;
	old_num_of_chars = stat_screen->num_of_chars ;
	old_filled_len = stat_screen->filled_len ;

	if( ! ( stat_screen->chars = ml_str_new( count)))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_str_new() failed.\n") ;
	#endif

		return  0 ;
	}

	stat_screen->num_of_chars = count ;
	stat_screen->filled_len = 0 ;

	/*
	 * u_char -> ml_char_t
	 */

	(*parser->init)( parser) ;
	(*parser->set_str)( parser , str , strlen(str)) ;

	p = stat_screen->chars;

	ml_str_init( p , stat_screen->num_of_chars) ;

	while( (*parser->next_char)( parser , &ch))
	{
		int  is_fullwidth = 0 ;
		int  is_comb = 0 ;

		/* -1 (== control sequence) is permitted for \n. */
		if( ! ml_convert_to_internal_ch( &ch , 0 /* unicode policy */ , US_ASCII))
		{
			continue ;
		}

		if( ch.property & MKF_FULLWIDTH)
		{
			is_fullwidth = 1 ;
		}
		else if( ch.property & MKF_AWIDTH)
		{
			/* TODO: check col_size_of_width_a */
			is_fullwidth = 1 ;
		}

		if( is_comb)
		{
			if( ml_char_combine( p - 1 , mkf_char_to_int(&ch) ,
				ch.cs , is_fullwidth , is_comb , ML_FG_COLOR ,
				ML_BG_COLOR , 0 , 0 , 0 , 0 , 0))
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

		ml_char_set( p , mkf_char_to_int(&ch) , ch.cs , is_fullwidth ,
			is_comb , ML_FG_COLOR , ML_BG_COLOR , 0 , 0 , 0 , 0 , 0) ;

		p++ ;
		stat_screen->filled_len++ ;
	}

	for( modified_beg = 0 ;
	     modified_beg < stat_screen->filled_len &&
	     modified_beg < old_filled_len &&
	     ml_char_code_equal( old_chars + modified_beg , stat_screen->chars + modified_beg) ;
	     modified_beg ++) ;

	if( old_chars)
	{
		ml_str_delete( old_chars , old_num_of_chars) ;
	}

	if( modified_beg < old_filled_len || old_filled_len != stat_screen->filled_len)
	{
		draw_screen( stat_screen , 1 , modified_beg) ;
	}

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

	x_window_set_type_engine( &stat_screen->window ,
		x_get_type_engine( stat_screen->font_man)) ;

	x_window_set_fg_color( win , x_get_xcolor( stat_screen->color_man , ML_FG_COLOR)) ;
	x_window_set_bg_color( win , x_get_xcolor( stat_screen->color_man , ML_BG_COLOR)) ;

	x_window_set_override_redirect( &stat_screen->window , 1) ;
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
	draw_screen( (x_im_status_screen_t *) win , 0 , 0) ;

	/* draw border (margin area has been already cleared in x_window.c) */
	x_window_draw_rect_frame( win , -MARGIN , -MARGIN ,
				  win->width + MARGIN - 1 ,
				  win->height + MARGIN - 1);
}


/* --- global functions --- */

x_im_status_screen_t *
x_im_status_screen_new(
	x_display_t *  disp ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	int  is_vertical ,
	u_int  line_height ,
	int  x ,
	int  y
	)
{
	x_im_status_screen_t *  stat_screen ;

	if( ( stat_screen = calloc( 1 , sizeof( x_im_status_screen_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif

		return  NULL ;
	}

	stat_screen->font_man = font_man ;
	stat_screen->color_man = color_man ;

	stat_screen->x = x ;
	stat_screen->y = y ;
	stat_screen->line_height = line_height ;

	stat_screen->is_vertical = is_vertical ;

	if( ! x_window_init( &stat_screen->window , MARGIN * 2 , MARGIN * 2 ,
			     MARGIN * 2 , MARGIN * 2 , 0 , 0 ,
			     MARGIN , MARGIN , /* ceate_gc */ 1 , 0))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_window_init failed.\n") ;
	#endif

		goto  error ;
	}


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
#if  0
	stat_screen->window.window_finalized = window_finalized ;
#endif
	stat_screen->window.window_exposed = window_exposed ;

	/* methods of x_im_status_screen_t */
	stat_screen->delete = delete ;
	stat_screen->show = show ;
	stat_screen->hide = hide ;
	stat_screen->set_spot = set_spot ;
	stat_screen->set = set ;

	if( ! x_display_show_root( disp , &stat_screen->window , x , y ,
					XValue | YValue , "mlterm-status-window" , 0))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_display_show_root() failed.\n") ;
	#endif

		goto  error ;
	}

	return  stat_screen ;

error:
	free( stat_screen) ;

	return  NULL ;
}

#else /* ! USE_IM_PLUGIN */

x_im_status_screen_t *
x_im_status_screen_new(
	x_display_t *  disp ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	int  is_vertical ,
	u_int  line_height ,
	int  x ,
	int  y
	)
{
	return  NULL ;
}

#endif
