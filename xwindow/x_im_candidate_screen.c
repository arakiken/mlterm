/*
 *	$Id$
 */

#include  <kiklib/kik_str.h>
#include  "x_im_candidate_screen.h"

#ifdef  USE_IM_PLUGIN

#include  "ml_str.h"
#include  "x_draw_str.h"

#define  MARGIN		3
#define  LINE_SPACE	2

#define  VISIBLE_INDEX( n , p , i , t , l)	\
do {						\
	(t) = ((i) / p) * p ;			\
	(l) = (t) + p - 1 ;			\
	if( (l) > (n) - 1)			\
	{					\
		(l) = (n) - 1 ;			\
	}					\
} while(0)

/*
 * calculate max number of digits
 */

#define  NUM_OF_DIGITS( n , d)			\
do {						\
	int  v10 ;				\
	(n) = 1 ;				\
	v10 = 10 ;				\
	while( 1)				\
	{					\
		if( (d) / v10)			\
		{				\
			(n)++ ;			\
			v10 *= 10 ;		\
		}				\
		else				\
		{				\
			break ;			\
		}				\
	}					\
} while( 0)

/* --- static variables --- */


/* --- static functions --- */

static int
free_candidates(
	x_im_candidate_t *  candidates ,
	u_int  num_of_candidates
	)
{
	x_im_candidate_t *  c ;
	int  i ;

	if( candidates == NULL || num_of_candidates == 0)
	{
		return  1 ;
	}

	for( i = 0 , c = candidates ; i < num_of_candidates ; i++ , c++)
	{
		ml_str_delete( c->chars , c->num_of_chars);

		c->filled_len = 0 ;
		c->num_of_chars = 0 ;
	}

	free( candidates) ;

	return  1 ;
}

static u_int
candidate_width(
	x_font_manager_t *  font_man ,
	x_im_candidate_t *  candidate
	)
{
	u_int  width ;
	int  i ;

	if( candidate->chars == NULL || candidate->filled_len == 0)
	{
		return  0 ;
	}

	width = 0 ;

	for( i = 0 ; i < candidate->filled_len ; i++)
	{
		x_font_t *  xfont ;

		xfont = x_get_font( font_man ,
				    ml_char_font( &candidate->chars[i])) ;

		width += x_calculate_char_width(
					xfont ,
					ml_char_bytes( &candidate->chars[i]) ,
					ml_char_size( &candidate->chars[i]) ,
					ml_char_cs( &candidate->chars[i])) ;
	}

	return  width ;
}

static u_int
max_candidate_width(
	x_font_manager_t *  font_man ,
	x_im_candidate_t *  candidates ,
	u_int  num_of_candidates
	)
{
	u_int  max_width ;
	int  i ;

	max_width = 0 ;

	for( i = 0 ; i < num_of_candidates  ; i++)
	{
		u_int  width ;

		width = candidate_width( font_man , &candidates[i]) ;

		if( width > max_width)
		{
			max_width = width ;
		}
	}

	return  max_width ;
}

static u_int
total_candidate_width(
	x_font_manager_t *  font_man ,
	x_im_candidate_t *  candidates ,
	u_int  to ,
	u_int  from
	)
{
	u_int  total_width ;
	int  i ;

	total_width = 0 ;

	for( i = to ; i <= from  ; i++)
	{
		total_width += candidate_width( font_man , &candidates[i]) ;
	}

	return  total_width ;
}

#define  MAX_NUM_OF_DIGITS 4 /* max is 9999. enough? */

static void
draw_screen_vertical(
	x_im_candidate_screen_t *  cand_screen ,
	u_int  top ,
	u_int  last
	)
{
	x_font_t *  xfont ;
	u_int  i ;
	u_int  num_of_digits ;
	u_int  win_width ;
	u_int  win_height ;

	if( cand_screen->num_of_candidates > cand_screen->num_per_window)
	{
		NUM_OF_DIGITS( num_of_digits , cand_screen->num_per_window) ;
	}
	else
	{
		NUM_OF_DIGITS( num_of_digits , last) ;
	}

	/*
	 * resize window
	 */

	/*
	 * width : [digit] + [space] + [max_candidate_width]
	 *         or width of "index/total"
	 * height: ([ascii height] + [line space]) x (num_per_window + 1)
	 *         or ([ascii height] + [line space)] x num_of_candidates
	 *   +-------------------+
	 *   |1  cand0           |\
	 *   |2  cand1           | \
	 *   |3  cand2           |  |
	 *   |4  cand3           |  |
	 *   |5  cand4           |  |-- num_per_window
	 *   |6  cand5           |  |
	 *   |7  cand6           |  |
	 *   |8  cand7           |  |
	 *   |9  widest candidate| /
	 *   |10 cand9           |/
	 *   |    index/total    |--> show if total > num_per_window
	 *   +-------------------+
	 */

	xfont = x_get_usascii_font( cand_screen->font_man) ;

	/* width of window */
	win_width = xfont->width * (num_of_digits + 1) ;
	win_width += max_candidate_width( cand_screen->font_man ,
					  &cand_screen->candidates[top] ,
					  last - top + 1) ;
	if( win_width < ( MAX_NUM_OF_DIGITS * 2 + 1) * xfont->width)
	{
		win_width = ( MAX_NUM_OF_DIGITS * 2 + 1) * xfont->width ;
	}

	/* height of window */
	if( cand_screen->num_of_candidates > cand_screen->num_per_window)
	{
		win_height = (xfont->height + LINE_SPACE) *
			     (cand_screen->num_per_window + 1) ;
	}
	else
	{
		win_height = (xfont->height + LINE_SPACE) *
			     cand_screen->num_of_candidates ;
	}

	x_window_resize( &cand_screen->window , win_width , win_height , 0) ;

	/*
	 * draw border
	 */
	x_window_draw_rect_frame( &cand_screen->window , 0 , 0 ,
				  win_width + MARGIN*2 - 1 ,
				  win_height + MARGIN*2 - 1);

	/*
	 * digits and candidates
	 */

#ifdef  DEBUG
	if( num_of_digits > MAX_NUM_OF_DIGITS)
	{
		kik_warn_printf( KIK_DEBUG_TAG " num_of_digits %d is too large.\n", num_of_digits) ;
	}
#endif

	for( i = top ; i <= last; i++)
	{
		u_char  digit[MAX_NUM_OF_DIGITS + 1] ;
		int  j ;

		/*
		 * digits
		 * +----------+
		 * |1 cand0   |
		 *  ^
		 */
		kik_snprintf( digit , MAX_NUM_OF_DIGITS + 1 , "%i    " , i - top + 1) ;

		for( j = 0 ; j < num_of_digits + 1 ; j++)
		{
			ml_char_t  ch ;

			ml_char_init( &ch) ;

			ml_char_set( &ch , &digit[j] , 1 ,
				     US_ASCII , 0 , 0 ,
				     ML_FG_COLOR , ML_BG_COLOR ,
				     0 , 0) ;

			x_draw_str( &cand_screen->window ,
				    cand_screen->font_man ,
				    cand_screen->color_man ,
				    &ch , 1 ,
				    j * xfont->width ,
				    (xfont->height + LINE_SPACE) * (i - top) ,
				    xfont->height + LINE_SPACE ,
				    xfont->height_to_baseline + LINE_SPACE / 2 ,
				    LINE_SPACE / 2 ,
				    LINE_SPACE / 2 + LINE_SPACE % 2) ;
		}

		/*
		 * candidate
		 * +----------+
		 * |1 cand0   |
		 *    ^^^^^
		 */
		x_draw_str_to_eol( &cand_screen->window ,
				   cand_screen->font_man ,
				   cand_screen->color_man ,
				   cand_screen->candidates[i].chars ,
				   cand_screen->candidates[i].filled_len ,
				   xfont->width * (num_of_digits + 1) ,
				   (xfont->height + LINE_SPACE) * (i - top) ,
				   xfont->height + LINE_SPACE ,
				   xfont->height_to_baseline + LINE_SPACE / 2 ,
				   LINE_SPACE / 2 ,
				   LINE_SPACE / 2 + LINE_SPACE % 2) ;
	}

	/*
	 * |7 cand6         |
	 * |8 last candidate|
	 * |                |\
	 * |                | }-- clear this area
	 * |                |/
	 * +----------------+
	 */
	if( cand_screen->num_of_candidates > 10 && last - top < 10)
	{
		u_int  y ;

		y = (xfont->height + LINE_SPACE) * (last - top + 1);

		x_window_clear( &cand_screen->window , 0 , y ,
				win_width , win_height - y - 1) ;
	}

	/*
	 * |8  cand7     |
	 * |9  cand8     |
	 * |10 cand9     |
	 * | index/total | <-- draw this
	 * +-------------+
	 */
	if( cand_screen->num_of_candidates > cand_screen->num_per_window)
	{
		u_char  navi[MAX_NUM_OF_DIGITS * 2 + 4];
		u_int  width ;
		size_t  len ;
		int  x ;

		len = kik_snprintf( navi , MAX_NUM_OF_DIGITS * 2 + 2 , "%d/%d",
				cand_screen->index + 1 ,
				cand_screen->num_of_candidates) ;

		width = len * xfont->width ;

		x = (win_width - width) / 2 ;	/* centering */

		for( i = 0 ; i < len ; i++)
		{
			ml_char_t  ch ;

			ml_char_init( &ch) ;

			ml_char_set( &ch , &navi[i] , 1 ,
				     US_ASCII , 0 , 0 ,
				     ML_FG_COLOR , ML_BG_COLOR ,
				     0 , 0) ;

			x_draw_str( &cand_screen->window ,
				    cand_screen->font_man ,
				    cand_screen->color_man ,
				    &ch , 1 ,
				    x + i * xfont->width ,
				    (xfont->height + LINE_SPACE) * cand_screen->num_per_window + LINE_SPACE,
				    xfont->height ,
				    xfont->height_to_baseline ,
				    0 , 0) ;
		}
	}
}

static void
draw_screen_horizontal(
	x_im_candidate_screen_t *  cand_screen ,
	u_int  top ,
	u_int  last
	)
{
	x_font_t *  xfont ;
	u_int  win_width ;
	u_int  win_height ;
	int  i ;
	int  x = 0 ;

	/*
	 * resize window
	 */

	/*
	 * +-------------------------------------+
	 * |1:cand0 2:cand1 3:cand4 ... 10:cand9 |
	 * |                          index/total|
	 * +-------------------------------------+
	 */

	xfont = x_get_usascii_font( cand_screen->font_man) ;

	/* width of window */
	win_width = 0 ;
	for( i = 1 ; i <= last - top + 1 ; i++)
	{
		u_int  num_of_digits ;

		NUM_OF_DIGITS( num_of_digits , i) ;
		win_width += xfont->width * (num_of_digits + 2) ;
	}
	win_width += total_candidate_width( cand_screen->font_man ,
					    cand_screen->candidates ,
					    top , last) ;
	/* height of window */
	win_height = xfont->height + LINE_SPACE ;

	x_window_resize( &cand_screen->window , win_width , win_height , 0) ;

	/*
	 * draw border
	 */
	x_window_draw_rect_frame( &cand_screen->window , 0 , 0 ,
				  win_width + MARGIN*2 - 1 ,
				  win_height + MARGIN*2 - 1);

	for( i = top ; i <= last; i++)
	{
		u_int  num_of_digits ;
		u_char  digit[MAX_NUM_OF_DIGITS + 1] ;
		int  j ;

		/*
		 * digits
		 * +---------------
		 * |1:cand0 2:cand1
		 *  ^^
		 */
		NUM_OF_DIGITS( num_of_digits , (i - top + 1)) ;
		kik_snprintf( digit , MAX_NUM_OF_DIGITS + 1 , "%i." , i - top + 1) ;

		for( j = 0 ; j < num_of_digits + 1 ; j++)
		{
			ml_char_t  ch ;

			ml_char_init( &ch) ;

			ml_char_set( &ch , &digit[j] , 1 , US_ASCII , 0 , 0 ,
				     ML_FG_COLOR , ML_BG_COLOR , 0 , 0) ;

			x_draw_str( &cand_screen->window ,
				    cand_screen->font_man ,
				    cand_screen->color_man ,
				    &ch , 1 ,
				    x , 0 ,
				    xfont->height + LINE_SPACE ,
				    xfont->height_to_baseline + LINE_SPACE / 2 ,
				    LINE_SPACE / 2 ,
				    LINE_SPACE / 2 + LINE_SPACE % 2) ;
			x += xfont->width ;
		}

		/*
		 * candidate
		 * +---------------
		 * |1:cand0 2:cand2
		 *    ^^^^^
		 */
		x_draw_str( &cand_screen->window ,
			    cand_screen->font_man ,
			    cand_screen->color_man ,
			    cand_screen->candidates[i].chars ,
			    cand_screen->candidates[i].filled_len ,
			    x , 0 ,
			    xfont->height + LINE_SPACE ,
			    xfont->height_to_baseline + LINE_SPACE / 2 ,
			    LINE_SPACE / 2 ,
			    LINE_SPACE / 2 + LINE_SPACE % 2) ;
		x += candidate_width( cand_screen->font_man ,
				      &cand_screen->candidates[i]) ;

		/*
		 * +---------------
		 * |1:cand0 2:cand2
		 *         ^
		 */
		x_window_clear( &cand_screen->window , x , 1 ,
				xfont->width , win_height - 2) ;
		x += xfont->width ;
	}
}

static void
draw_screen(
	x_im_candidate_screen_t *  cand_screen
	)
{
	u_int  top ;
	u_int  last ;

	VISIBLE_INDEX(cand_screen->num_of_candidates ,
		      cand_screen->num_per_window ,
		      cand_screen->index ,
		      top , last) ;

	if( cand_screen->is_vertical_direction)
	{
		draw_screen_vertical( cand_screen , top , last) ;
	}
	else
	{
		draw_screen_horizontal( cand_screen , top , last) ;
	}
}

static void
adjust_window_position(
	x_im_candidate_screen_t *  cand_screen ,
	int *  x ,
	int *  y
	)
{
	u_int  dh ;
	u_int  dw ;
	u_int  top ;
	u_int  last ;
	u_int  num_of_digits ;

	VISIBLE_INDEX( cand_screen->num_of_candidates ,
		       cand_screen->num_per_window ,
		       cand_screen->index ,
		       top , last) ;

	if( cand_screen->is_vertical_direction)
	{
		if( cand_screen->num_of_candidates > cand_screen->num_per_window)
		{
			NUM_OF_DIGITS( num_of_digits , cand_screen->num_per_window) ;
		}
		else
		{
			NUM_OF_DIGITS( num_of_digits , last) ;
		}
	}
	else
	{
		num_of_digits = 1 ;
	}

	if( cand_screen->is_vertical_term)
	{
		return ;
	}

	dh = DisplayHeight( cand_screen->window.display ,
			    cand_screen->window.screen) ;
	dw = DisplayWidth( cand_screen->window.display ,
			   cand_screen->window.screen) ;


	if( *y + cand_screen->window.height > dh)
	{
		*y -= (cand_screen->window.height + cand_screen->line_height + MARGIN * 2) ;
	}

	if( num_of_digits)
	{
		*x -= x_get_usascii_font( cand_screen->font_man)->width * (num_of_digits + 1) + MARGIN ;
	}

	if( *x + cand_screen->window.width > dw)
	{
		*x = dw - cand_screen->window.width ;
	}
}

/*
 * methods of x_im_candidate_screen_t
 */

static int
delete(
	x_im_candidate_screen_t *  cand_screen
	)
{
	free_candidates( cand_screen->candidates ,
			 cand_screen->num_of_candidates) ;

	x_window_manager_remove_root( cand_screen->window.win_man ,
				      &cand_screen->window) ;

	free( cand_screen) ;

	return  1 ;
}

static int
show(
	x_im_candidate_screen_t *  cand_screen
	)
{
	x_window_map( &cand_screen->window) ;

	return  1 ;
}

static int
hide(
	x_im_candidate_screen_t *  cand_screen
	)
{
	if( cand_screen->is_focused)
	{
		return  1 ;
	}

	x_window_unmap( &cand_screen->window) ;

	return  1 ;
}

static int
set_spot(
	x_im_candidate_screen_t *  cand_screen ,
	int  x ,
	int  y
	)
{
	adjust_window_position( cand_screen , &x ,&y) ;

	if( cand_screen->window.x != x || cand_screen->window.y != y)
	{
		x_window_move( &cand_screen->window , x , y) ;
	}

	return  1 ;
}

static int
init_candidates(
	x_im_candidate_screen_t *  cand_screen ,
	u_int num_of_candidates ,
	u_int num_per_window
	)
{
	int  i ;

	if( cand_screen->candidates)
	{
		free_candidates( cand_screen->candidates ,
				 cand_screen->num_of_candidates) ;

		cand_screen->candidates = NULL ;
	}

	cand_screen->num_of_candidates = num_of_candidates ;
	cand_screen->num_per_window = num_per_window ;

	/* allocate candidates(x_im_candidate_t) array */
	if( ( cand_screen->candidates = malloc( sizeof( x_im_candidate_t) * cand_screen->num_of_candidates)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif

		return  0 ;
	}

	/* initialize candidate array */
	for( i = 0 ; i < cand_screen->num_of_candidates ; i++)
	{
		cand_screen->candidates[i].chars = NULL ;
		cand_screen->candidates[i].num_of_chars = 0 ;
		cand_screen->candidates[i].filled_len = 0 ;
	}

	cand_screen->index = 0 ;

	return  1 ;
}

static int
set_candidate(
	x_im_candidate_screen_t *  cand_screen ,
	mkf_parser_t *  parser ,
	u_char *  str ,
	u_int  index
	)
{
	int  count = 0 ;
	mkf_char_t  ch ;
	ml_char_t *  p ;

	if( index >= cand_screen->num_of_candidates)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " index of candidates is too large number [num_of_candidates: %d, index: %d]\n" , cand_screen->num_of_candidates , index) ;
	#endif
		return  0 ;
	}

	/*
	 * count number of characters to allocate candidates[index].chars
	 */

	(*parser->init)( parser) ;
	(*parser->set_str)( parser , str , strlen( str)) ;

	while( (*parser->next_char)( parser , &ch))
	{
		count++ ;
	}

	if( cand_screen->candidates[index].chars)
	{
		ml_str_delete( cand_screen->candidates[index].chars ,
			       cand_screen->candidates[index].num_of_chars) ;
	}

	if( ! ( cand_screen->candidates[index].chars = ml_str_new( count)))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_str_new() failed.\n") ;
	#endif

		cand_screen->candidates[index].num_of_chars = 0 ;
		cand_screen->candidates[index].filled_len = 0 ;

		return  0 ;
	}

	cand_screen->candidates[index].num_of_chars = count ;

	/*
	 * im encoding -> term encoding
	 */

	(*parser->init)( parser) ;
	(*parser->set_str)( parser , str , strlen(str)) ;

	p = cand_screen->candidates[index].chars;

	ml_str_init( p , cand_screen->candidates[index].num_of_chars) ;

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
		cand_screen->candidates[index].filled_len++ ;
	}

	return  1 ;
}

static int
select_candidate(
	x_im_candidate_screen_t *  cand_screen ,
	u_int index
	)
{
	x_im_candidate_t *  cand ;
	int  i ;

	if( index >= cand_screen->num_of_candidates)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Selected index [%d] is larger than number of candidates [%d].\n", index , cand_screen->num_of_candidates);
	#endif

		return  0 ;
	}

	cand = &cand_screen->candidates[cand_screen->index] ;

	if( cand->chars)
	{
		for( i = 0 ; i < cand->filled_len ; i++)
		{
			ml_char_set_fg_color( &cand->chars[i] , ML_FG_COLOR) ;
			ml_char_set_bg_color( &cand->chars[i] , ML_BG_COLOR) ;
		}
	}

	cand = &cand_screen->candidates[index] ;

	if( cand->chars)
	{
		for( i = 0 ; i < cand->filled_len ; i++)
		{
			ml_char_set_fg_color( &cand->chars[i] , ML_BG_COLOR) ;
			ml_char_set_bg_color( &cand->chars[i] , ML_FG_COLOR) ;
		}
	}

	cand_screen->index = index ;

	draw_screen( cand_screen) ;

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
	x_im_candidate_screen_t *  cand_screen ;

	cand_screen = (x_im_candidate_screen_t*) win ;

	x_window_set_fg_color( win , x_get_color( cand_screen->color_man ,
						  ML_FG_COLOR)->pixel) ;
	x_window_set_bg_color( win , x_get_color( cand_screen->color_man ,
						  ML_BG_COLOR)->pixel) ;

	x_window_set_override_redirect( &cand_screen->window , 1) ;
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
	draw_screen( (x_im_candidate_screen_t *) win) ;
}

static void
window_focused(
	x_window_t *  win
	)
{
	((x_im_candidate_screen_t *) win)->is_focused = 1 ;
}

static void
window_unfocused(
	x_window_t *  win
	)
{
	((x_im_candidate_screen_t *) win)->is_focused = 0 ;
}

static void
button_pressed(
	x_window_t *  win ,
	XButtonEvent *  event ,
	int  click_num
	)
{
	x_im_candidate_screen_t *  cand_screen ;
	u_int  index ;
	u_int  top ;
	u_int  last ;

	cand_screen = (x_im_candidate_screen_t *) win ;

	if ( event->button != 1)
	{
		return ;
	}

	if( ! cand_screen->listener.selected)
	{
		return ;
	}

	VISIBLE_INDEX(cand_screen->num_of_candidates ,
		      cand_screen->num_per_window ,
		      cand_screen->index ,
		      top , last) ;

	index = event->y / (x_get_usascii_font( cand_screen->font_man)->height + LINE_SPACE);

	index += top ;

	if( ! select_candidate( cand_screen , index))
	{
		return ;
	}

	draw_screen( cand_screen) ;

	(*cand_screen->listener.selected)( cand_screen->listener.self , index) ;


}

static void
button_released(
	x_window_t *  win ,
	XButtonEvent *  event
	)
{
	/* do nothing */
}


/* --- global functions --- */

x_im_candidate_screen_t *
x_im_candidate_screen_new(
	x_window_manager_t *  win_man ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	int  is_vertical_term ,
	int  is_vertical_direction ,
	u_int  line_height_of_screen ,
	int  x ,
	int  y
	)
{
	x_im_candidate_screen_t *  cand_screen ;

	if( ( cand_screen = malloc( sizeof( x_im_candidate_screen_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif

		return  NULL ;
	}

	cand_screen->font_man = font_man ;
	cand_screen->color_man = color_man ;

	cand_screen->candidates = NULL ;
	cand_screen->num_of_candidates = 0 ;

	cand_screen->index = 0 ;

	cand_screen->is_focused = 0 ;

	cand_screen->line_height = line_height_of_screen ;

	cand_screen->is_vertical_term = is_vertical_term ;
	cand_screen->is_vertical_direction = is_vertical_direction ;

	if( ! x_window_init( &cand_screen->window , MARGIN * 2 , MARGIN * 2 ,
			     MARGIN * 2 , MARGIN * 2 , MARGIN * 2 , MARGIN * 2 ,
			     1 , 1 , MARGIN))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_window_init failed.\n") ;
	#endif

		goto  error ;
	}

	x_window_init_event_mask( &cand_screen->window ,
				  ButtonPressMask | ButtonReleaseMask) ;


	/*
	 * +------------+
	 * | x_window.c | --- window events ---> +-------------------------+
	 * +------------+                        | x_im_candidate_screen.c |
	 * +------------+ ----- methods  ------> +-------------------------+
	 * | im plugin  | <---- callbacks ------
	 * +------------+
	 */

	/* callbacks for window events */
	cand_screen->window.window_realized = window_realized ;
	cand_screen->window.window_finalized = window_finalized ;
	cand_screen->window.window_exposed = window_exposed ;
	cand_screen->window.window_focused = window_focused ;
	cand_screen->window.window_unfocused = window_unfocused ;
	cand_screen->window.button_released = button_released ;
	cand_screen->window.button_pressed = button_pressed ;
#if 0
	cand_screen->window.button_press_continued = button_press_continued ;
	cand_screen->window.window_deleted = window_deleted ;
	cand_screen->window.mapping_notify = mapping_notify ;
#endif

	/* methods of x_im_candidate_screen_t */
	cand_screen->delete = delete ;
	cand_screen->show = show ;
	cand_screen->hide = hide ;
	cand_screen->set_spot = set_spot ;
	cand_screen->init = init_candidates ;
	cand_screen->set = set_candidate ;
	cand_screen->select = select_candidate ;

	/* callbacks for events of candidate_screen */
	cand_screen->listener.self = NULL ;
	cand_screen->listener.selected = NULL ;

	if( ! x_window_manager_show_root( win_man ,
					  &cand_screen->window ,
					  x , y , 0 ,
					  "mlterm-candidate-window"))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_window_manager_show_root() failed.\n") ;
	#endif

		goto  error ;
	}

	return  cand_screen ;

error:
	free( cand_screen) ;

	return  NULL ;
}

#else  /* ! USE_IM_PLUGIN */

x_im_candidate_screen_t *
x_im_candidate_screen_new(
	x_window_manager_t *  win_man ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	int  is_vertical_term ,
	int  is_vertical_direction ,
	u_int  line_height_of_screen ,
	int  x ,
	int  y
	)
{
	return  NULL ;
}

#endif
