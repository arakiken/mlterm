/*
 *	$Id$
 */

#include  "ml_vt100_parser.h"

#include  <stdio.h>
#include  <string.h>		/* memmove */
#include  <stdlib.h>		/* atoi */
#include  <kiklib/kik_debug.h>

#include  <mkf/mkf_ucs4_map.h>
#include  <mkf/mkf_ko_kr_map.h>

#include  "ml_color.h"


#define  CTLKEY_BEL	0x07
#define  CTLKEY_BS	0x08
#define  CTLKEY_TAB	0x09
#define  CTLKEY_LF	0x0a
#define  CTLKEY_VT	0x0b
#define  CTLKEY_CR	0x0d
#define  CTLKEY_ESC	0x1b

#define  CURRENT_STR_P(vt100_parser)  (&vt100_parser->seq[(vt100_parser)->len - (vt100_parser)->left])

#if  0
#define  __DEBUG
#endif

#if  0
#define  ESCSEQ_DEBUG
#endif


/* --- static variables --- */

/*
 * MSB of these charsets are not set , but must be set manually for X font.
 */
static mkf_charset_t  msb_set_cs_table[] =
{
	JISX0201_KATA ,
	ISO8859_1_R ,
	ISO8859_2_R ,
	ISO8859_3_R ,
	ISO8859_4_R ,
	ISO8859_5_R ,
	ISO8859_6_R ,
	ISO8859_7_R ,
	ISO8859_8_R ,
	ISO8859_9_R ,
	ISO8859_10_R ,
	TIS620_2533 ,
	ISO8859_13_R ,
	ISO8859_14_R ,
	ISO8859_15_R ,
	ISO8859_16_R ,
	TCVN5712_3_1993 ,

} ;


/* --- static functions --- */

static int
is_msb_set(
	mkf_charset_t  cs
	)
{
	int  counter ;

	for( counter = 0 ; counter < sizeof( msb_set_cs_table) / sizeof( msb_set_cs_table[0]) ; counter ++)
	{
		if( msb_set_cs_table[counter] == cs)
		{
			return  1 ;
		}
	}

	return  0 ;
}

static size_t
receive_bytes(
	ml_vt100_parser_t *  vt100_parser
	)
{
	size_t  ret ;
	size_t  left ;

	if( vt100_parser->left > 0)
	{
		memmove( vt100_parser->seq , CURRENT_STR_P(vt100_parser) ,
			vt100_parser->left * sizeof( u_char)) ;
	}

	left = PTYMSG_BUFFER_SIZE - vt100_parser->left ;

	if( ( ret = ml_read_pty( vt100_parser->pty ,
		&vt100_parser->seq[vt100_parser->left] , left)) == 0)
	{
		return  0 ;
	}

	vt100_parser->len = ( vt100_parser->left += ret) ;

#ifdef  __DEBUG
	{
		int  counter ;

		kik_debug_printf( KIK_DEBUG_TAG " pty msg (len %d) is received:" , vt100_parser->left) ;

	#if  0
		for( counter = 0 ; counter < vt100_parser->left ; counter ++)
		{
			fprintf( stderr , "%c" , vt100_parser->seq[counter]) ;
		}
		fprintf( stderr , "[END]\n") ;
	#endif

	#if  1
		for( counter = 0 ; counter < vt100_parser->left ; counter ++)
		{
			fprintf( stderr , "%.2x" , vt100_parser->seq[counter]) ;
		}
		fprintf( stderr , "[END]\n") ;
	#endif
	}
#endif

	return  1 ;
}

static void
change_font(
	ml_vt100_parser_t *  vt100_parser
	)
{
	if( ( vt100_parser->font = ml_term_screen_get_font( vt100_parser->termscr ,
		vt100_parser->font_attr | vt100_parser->cs)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" no font was found by font attr 0x%x , default USASCII font is used instead.\n" ,
			vt100_parser->font_attr | vt100_parser->cs) ;
	#endif

		/*
		 * using US_ASCII font instead.
		 *
		 * XXX
		 * better algorithm should be used to search alternative font...
		 * e.g.) choose another 2 bytes font for non-existing 2 bytes font.
		 */
		 
		if( ( vt100_parser->font = ml_term_screen_get_font( vt100_parser->termscr ,
				vt100_parser->font_attr | US_ASCII)) == NULL &&
			( vt100_parser->font = ml_term_screen_get_font( vt100_parser->termscr ,
				DEFAULT_FONT_ATTR(US_ASCII))) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " this should never happen(unrecoverable error).\n") ;
		#endif
		
			return ;
		}
	}
}

static void
clear_buffer(
	ml_vt100_parser_t *  vt100_parser
	)
{
	vt100_parser->buffer.len = 0 ;
}

static int
flush_buffer(
	ml_vt100_parser_t *  vt100_parser
	)
{
	ml_char_buffer_t *  buffer ;

	buffer = &vt100_parser->buffer ;

	if( buffer->len == 0)
	{
		return  0 ;
	}
	
#ifdef  __DEBUG
	{
		int  counter ;

		fprintf( stderr , "\nflushing chars(%d)...==>" , buffer->len) ;
		for( counter = 0 ; counter < buffer->len ; counter ++)
		{
			if( ml_char_size( &buffer->chars[counter]) == 2)
			{
			#if  0
				fprintf( stderr , "%x%x" ,
					ml_char_bytes( &buffer->chars[counter])[0] | 0x80 ,
					ml_char_bytes( &buffer->chars[counter])[1] | 0x80) ;
			#else
				fprintf( stderr , "%c%c" ,
					ml_char_bytes( &buffer->chars[counter])[0] | 0x80 ,
					ml_char_bytes( &buffer->chars[counter])[1] | 0x80) ;
			#endif
			}
			else
			{
			#if  0
				fprintf( stderr , "%x" , ml_char_bytes( &buffer->chars[counter])[0]) ;
			#else
				fprintf( stderr , "%c" , ml_char_bytes( &buffer->chars[counter])[0]) ;
			#endif
			}
		}

		fprintf( stderr , "<===\n") ;
	}
#endif

	(*buffer->output_func)( vt100_parser->termscr , buffer->chars , buffer->len) ;

	clear_buffer( vt100_parser) ;

#ifdef __DEBUG
	ml_image_dump( vt100_parser->termscr->image) ;
#endif

	return  1 ;
}

static void
put_char(
	ml_vt100_parser_t *  vt100_parser ,
	u_char *  ch ,
	size_t  len ,
	mkf_charset_t  cs ,
	mkf_property_t  prop
	)
{
	ml_font_attr_t  width_attr ;
	ml_color_t  fg_color ;
	ml_color_t  bg_color ;
	
	if( vt100_parser->buffer.len == PTYMSG_BUFFER_SIZE)
	{
		if( flush_buffer( vt100_parser))
		{
			ml_term_screen_redraw_image( vt100_parser->termscr) ;
		}
	}

	if( cs == ISO10646_UCS2_1 || cs == ISO10646_UCS4_1)
	{
		/*
		 * checking East Aisan Width property of the char.
		 */
		 
		if( prop & MKF_BIWIDTH)
		{
			width_attr = FONT_BIWIDTH ;
		}
		else if( (prop & MKF_AWIDTH) && vt100_parser->col_size_of_east_asian_width_a == 2)
		{
			width_attr = FONT_BIWIDTH ;
		}
		else
		{
			width_attr = 0 ;
		}
	}
	else
	{
		width_attr = 0 ;
	}

	if( vt100_parser->font == NULL || vt100_parser->cs != cs ||
		(vt100_parser->font_attr & FONT_BIWIDTH) != width_attr)
	{
		vt100_parser->cs = cs ;

		vt100_parser->font_attr &= ~FONT_BIWIDTH ;
		vt100_parser->font_attr |= width_attr ;

		change_font( vt100_parser) ;
	}

	if( vt100_parser->is_reversed)
	{
		fg_color = vt100_parser->bg_color ;
		bg_color = vt100_parser->fg_color ;
	}
	else
	{
		fg_color = vt100_parser->fg_color ;
		bg_color = vt100_parser->bg_color ;
	}

	if( prop & MKF_COMBINING)
	{
		if( vt100_parser->buffer.len == 0)
		{
			if( ml_term_screen_combine_with_prev_char( vt100_parser->termscr ,
				ch , len , vt100_parser->font , vt100_parser->font_decor ,
				vt100_parser->fg_color , vt100_parser->bg_color))
			{
				return  ;
			}
		}
		else
		{
			if( ml_char_combine( &vt100_parser->buffer.chars[vt100_parser->buffer.len - 1] ,
				ch , len , vt100_parser->font , vt100_parser->font_decor ,
				vt100_parser->fg_color , vt100_parser->bg_color))
			{
				return  ;
			}
		}

		/*
		 * if combining failed , char is normally appended.
		 */
	}

	ml_char_set( &vt100_parser->buffer.chars[vt100_parser->buffer.len++] , ch , len ,
		vt100_parser->font , vt100_parser->font_decor , fg_color , bg_color) ;

	return ;
}


/*
 * VT100_PARSER Escape Sequence Commands.
 */
 
static void
change_font_attr(
	ml_vt100_parser_t *  vt100_parser ,
	int  flag
	)
{
	ml_font_attr_t  attr ;

	attr = vt100_parser->font_attr ;

	if( flag == 0)
	{
		/* Normal */
		vt100_parser->font_decor = 0 ;
		attr = DEFAULT_FONT_ATTR(0) ;
		vt100_parser->fg_color = MLC_FG_COLOR ;
		vt100_parser->bg_color = MLC_BG_COLOR ;
		vt100_parser->is_reversed = 0 ;
	}
	else if( flag == 1)
	{
		/* Bold */
		RESET_FONT_THICKNESS(attr) ;
		attr |= FONT_BOLD ;
	}
	else if( flag == 4)
	{
		/* Underscore */
		vt100_parser->font_decor |= FONT_UNDERLINE ;
	}
	else if( flag == 5)
	{
		/* Blink */
	}
	else if( flag == 7)
	{
		/* Inverse */
		
		vt100_parser->is_reversed = 1 ;
	}
	else if( flag == 22)
	{
		attr &= ~FONT_BOLD ;
	}
	else if( flag == 24)
	{
		vt100_parser->font_decor &= ~FONT_UNDERLINE ;
	}
	else if( flag == 25)
	{
		/* blink */
	}
	else if( flag == 27)
	{
		vt100_parser->is_reversed = 0 ;
	}
	else if( flag == 30)
	{
		vt100_parser->fg_color = MLC_BLACK ;
	}
	else if( flag == 31)
	{
		vt100_parser->fg_color = MLC_RED ;
	}
	else if( flag == 32)
	{
		vt100_parser->fg_color = MLC_GREEN ;
	}
	else if( flag == 33)
	{
		vt100_parser->fg_color = MLC_YELLOW ;
	}
	else if( flag == 34)
	{
		vt100_parser->fg_color = MLC_BLUE ;
	}
	else if( flag == 35)
	{
		vt100_parser->fg_color = MLC_MAGENTA ;
	}
	else if( flag == 36)
	{
		vt100_parser->fg_color = MLC_CYAN ;
	}
	else if( flag == 37)
	{
		vt100_parser->fg_color = MLC_WHITE ;
	}
	else if( flag == 39)
	{
		/* default fg */
		
		vt100_parser->fg_color = MLC_FG_COLOR ;
		vt100_parser->is_reversed = 0 ;
	}
	else if( flag == 40)
	{
		vt100_parser->bg_color = MLC_BLACK ;
	}
	else if( flag == 41)
	{
		vt100_parser->bg_color = MLC_RED ;
	}
	else if( flag == 42)
	{
		vt100_parser->bg_color = MLC_GREEN ;
	}
	else if( flag == 43)
	{
		vt100_parser->bg_color = MLC_YELLOW ;
	}
	else if( flag == 44)
	{
		vt100_parser->bg_color = MLC_BLUE ;
	}
	else if( flag == 45)
	{
		vt100_parser->bg_color = MLC_MAGENTA ;
	}
	else if( flag == 46)
	{
		vt100_parser->bg_color = MLC_CYAN ;
	}
	else if( flag == 47)
	{
		vt100_parser->bg_color = MLC_WHITE ;
	}
	else if( flag == 49)
	{
		vt100_parser->bg_color = MLC_BG_COLOR ;
		vt100_parser->is_reversed = 0 ;
	}
#ifdef  DEBUG
	else
	{
		kik_warn_printf( KIK_DEBUG_TAG " unknown font attr flag(%d).\n" , flag) ;
	}
#endif
	
	if( attr != vt100_parser->font_attr)
	{
		vt100_parser->font_attr = attr ;
		change_font( vt100_parser) ;
	}
}

static void
clear_display_all(
	ml_vt100_parser_t *  vt100_parser
	)
{
	clear_buffer( vt100_parser) ;
	
	ml_term_screen_goto_home( vt100_parser->termscr) ;
	ml_term_screen_clear_below( vt100_parser->termscr) ;
}

static void
clear_display_below(
	ml_vt100_parser_t *  vt100_parser
	)
{
	flush_buffer( vt100_parser) ;
	
	ml_term_screen_clear_below( vt100_parser->termscr) ;
}

static void
clear_display_above(
	ml_vt100_parser_t *  vt100_parser
	)
{
	flush_buffer( vt100_parser) ;

	ml_term_screen_clear_above( vt100_parser->termscr) ;
}

static void
clear_line_all(
	ml_vt100_parser_t *  vt100_parser
	)
{
	flush_buffer( vt100_parser) ;
	
	ml_term_screen_goto_beg_of_line( vt100_parser->termscr) ;
	ml_term_screen_clear_line_to_right( vt100_parser->termscr) ;
}

static void
clear_line_to_right(
	ml_vt100_parser_t *  vt100_parser
	)
{
	flush_buffer( vt100_parser) ;

	ml_term_screen_clear_line_to_right( vt100_parser->termscr) ;
}

static void
clear_line_to_left(
	ml_vt100_parser_t *  vt100_parser
	)
{
	flush_buffer( vt100_parser) ;

	ml_term_screen_clear_line_to_left( vt100_parser->termscr) ;
}

static void
cursor_up(
	ml_vt100_parser_t *  vt100_parser ,
	u_int  size
	)
{
	flush_buffer( vt100_parser) ;
	
	ml_term_screen_go_upward( vt100_parser->termscr , size) ;
}

static void
cursor_down(
	ml_vt100_parser_t *  vt100_parser ,
	u_int  size
	)
{
	flush_buffer( vt100_parser) ;
	
	ml_term_screen_go_downward( vt100_parser->termscr , size) ;
}

static void
cursor_forward(
	ml_vt100_parser_t *  vt100_parser ,
	u_int  size
	)
{
	flush_buffer( vt100_parser) ;
	
	ml_term_screen_go_forward( vt100_parser->termscr , size) ;
}

static void
cursor_back(
	ml_vt100_parser_t *  vt100_parser ,
	u_int  size
	)
{
	flush_buffer( vt100_parser) ;
	
	ml_term_screen_go_back( vt100_parser->termscr , size) ;
}

static void
cursor_go_horizontally(
	ml_vt100_parser_t *  vt100_parser ,
	int  col
	)
{
	flush_buffer( vt100_parser) ;
	
	ml_term_screen_go_horizontally( vt100_parser->termscr , col) ;
}

static void
cursor_go_vertically(
	ml_vt100_parser_t *  vt100_parser ,
	int  col
	)
{
	flush_buffer( vt100_parser) ;

	ml_term_screen_go_vertically( vt100_parser->termscr , col) ;
}

static void
cursor_goto(
	ml_vt100_parser_t *  vt100_parser ,
	int  col ,
	int  row
	)
{
	flush_buffer( vt100_parser) ;

	ml_term_screen_goto( vt100_parser->termscr , col , row) ;
}

static void
delete_cols(
	ml_vt100_parser_t *  vt100_parser ,
	u_int  len
	)
{
	flush_buffer( vt100_parser) ;
	
	ml_term_screen_delete_cols( vt100_parser->termscr , len) ;
}

static void
scroll_down(
	ml_vt100_parser_t *  vt100_parser
	)
{
	flush_buffer( vt100_parser) ;
	
	ml_term_screen_scroll_image_downward( vt100_parser->termscr , 1) ;
}

static void
scroll_up(
	ml_vt100_parser_t *  vt100_parser
	)
{
	flush_buffer( vt100_parser) ;

	ml_term_screen_scroll_image_upward( vt100_parser->termscr , 1) ;
}

static int
increment_str(
	u_char **  str ,
	size_t *  left
	)
{
	if( -- (*left) == 0)
	{
		return  0 ;
	}

	(*str) ++ ;

	return  1 ;
}

static int
parse_vt100_escape_sequence(
	ml_vt100_parser_t *  vt100_parser
	)
{
	u_char *  str_p ;
	size_t  left ;

	if( vt100_parser->left == 0)
	{
		/* end of string */
		
		return  1 ;
	}

	str_p = CURRENT_STR_P(vt100_parser) ;
	left = vt100_parser->left ;

	while( 1)
	{
		if( *str_p == CTLKEY_ESC)
		{			
			if( increment_str( &str_p , &left) == 0)
			{
				return  0 ;
			}

		#ifdef  ESCSEQ_DEBUG
			fprintf( stderr , "RECEIVED ESCAPE SEQUENCE: ESC - %c" , *str_p) ;
		#endif

			if( *str_p < 0x20 || 0x7e < *str_p)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " illegal escape sequence ESC - 0x%x.\n" , 
					*str_p) ;
			#endif
			}
			else if( *str_p == '#')
			{
				if( increment_str( &str_p , &left) == 0)
				{
					return  0 ;
				}

				if( *str_p == '8')
				{
					ml_term_screen_fill_all_with_e( vt100_parser->termscr) ;
				}
			}
			else if( *str_p == '7')
			{
				/* save cursor */

				/*
				 * XXX
				 * rxvt-2.7.7 saves following parameters.
				 *  col,row,rstyle,charset,char
				 * (see rxvt_scr_cursor() in screen.c)
				 *
				 * on the other hand , mlterm saves only
				 *   col,row,fg_color,bg_color(aka ml_cursor_t),
				 *   and font_decor,font_attr.
				 * in other words , "char" is not saved , but maybe it works.
				 */

				/* cursor position should be determined before save_cursor */
				flush_buffer( vt100_parser) ;
				
				ml_term_screen_save_cursor( vt100_parser->termscr) ;
				vt100_parser->saved_decor = vt100_parser->font_decor ;
				vt100_parser->saved_attr = vt100_parser->font_attr ;
			}
			else if( *str_p == '8')
			{
				/* restore cursor */

				flush_buffer( vt100_parser) ;

				if( ml_term_screen_restore_cursor( vt100_parser->termscr))
				{
					/* if restore failed , this won't be done. */
					vt100_parser->font_decor = vt100_parser->saved_decor ;
					vt100_parser->font_attr = vt100_parser->saved_attr ;
					vt100_parser->saved_attr = DEFAULT_FONT_ATTR(0) ;
					vt100_parser->saved_decor = 0 ;
				}
			}
			else if( *str_p == '=')
			{
				/* application keypad */

				ml_term_screen_set_app_keypad( vt100_parser->termscr) ;
			}
			else if( *str_p == '>')
			{
				/* normal keypad */

				ml_term_screen_set_normal_keypad( vt100_parser->termscr) ;
			}
			else if( *str_p == 'D')
			{
				/* index(scroll up) */

				scroll_up( vt100_parser) ;
			}
			else if( *str_p == 'E')
			{
				/* next line */
				
				flush_buffer( vt100_parser) ;
				ml_term_screen_line_feed( vt100_parser->termscr) ;
				ml_term_screen_goto_beg_of_line( vt100_parser->termscr) ;
			}
			else if( *str_p == 'F')
			{
				/* cursor to lower left corner of screen */

			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" cursor to lower left corner is not implemented.\n") ;
			#endif
			}
			else if( *str_p == 'H')
			{
				/* set tab */

				ml_term_screen_set_tab_stop( vt100_parser->termscr) ;
			}
			else if( *str_p == 'M')
			{
				/* reverse index(scroll down) */

				scroll_down( vt100_parser) ;
			}
			else if( *str_p == 'Z')
			{
				/* return terminal id */

			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" return terminal id is not implemented.\n") ;
			#endif
			}
			else if( *str_p == 'c')
			{
				/* full reset */

				clear_display_all( vt100_parser) ;

				/* XXX  is this necessary ? */
				change_font_attr( vt100_parser , 0) ;
			}
			else if( *str_p == 'l')
			{
				/* memory lock */

			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " memory lock is not implemented.\n") ;
			#endif
			}
			else if( *str_p == 'm')
			{
				/* memory unlock */

			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " memory unlock is not implemented.\n") ;
			#endif
			}
			else if( *str_p == '[')
			{
				int  is_dec_priv ;
				int  ps[5] ;
				size_t  num ;

			#ifdef  ESCSEQ_DEBUG
				fprintf( stderr , " - ") ;
			#endif

				if( increment_str( &str_p , &left) == 0)
				{
					return  0 ;
				}

				if( *str_p == '?')
				{
				#ifdef  ESCSEQ_DEBUG
					fprintf( stderr , "%c - " , *str_p) ;
				#endif
				
					is_dec_priv = 1 ;

					if( increment_str( &str_p , &left) == 0)
					{
						return  0 ;
					}
				}
				else
				{
					is_dec_priv = 0 ;
				}

				num = 0 ;
				while( num < 5)
				{
					if( '0' <= *str_p && *str_p <= '9')
					{
						u_char  digit[20] ;
						int  counter ;

						digit[0] = *str_p ;

						for( counter = 1 ; counter < 19 ; counter ++)
						{
							if( increment_str( &str_p , &left) == 0)
							{
								return  0 ;
							}

							if( *str_p < '0' || '9' < *str_p)
							{
								break ;
							}
							
							digit[counter] = *str_p ;
						}

						digit[counter] = '\0' ;

						ps[num ++] = atoi( digit) ;

					#ifdef  ESCSEQ_DEBUG
						fprintf( stderr , "%d - " , ps[num - 1]) ;
					#endif

						if( *str_p != ';')
						{
							break ;
						}
					}
					else if( *str_p == ';')
					{
						ps[num ++] = 0 ;
					}
					else
					{
						break ;
					}
					
				#ifdef  ESCSEQ_DEBUG
					fprintf( stderr , "; - ") ;
				#endif
				
					if( increment_str( &str_p , &left) == 0)
					{
						return  0 ;
					}
				}

				/*
				 * ingoring end 0 numbers.
				 */
				while( num > 0 && ps[num - 1] == 0)
				{
					num -- ;
				}

			#ifdef  ESCSEQ_DEBUG
				if( *str_p < 0x20 || 0x7e < *str_p)
				{
					fprintf( stderr , "<%x>" , *str_p) ;
				}
				else
				{
					fprintf( stderr , "%c" , *str_p) ;
				}
			#endif

				/*
				 * cursor-control characters inside ESC sequences
				 */
				if( *str_p == 0x8)
				{
					cursor_back( vt100_parser , 1) ;
					if( increment_str( &str_p , &left) == 0)
					{
						return  0 ;
					}
				}
				
				if( *str_p < 0x20 || 0x7e < *str_p)
				{
				#ifdef  DEBUG 
					kik_warn_printf( KIK_DEBUG_TAG
						" illegal csi sequence ESC - [ - 0x%x.\n" , 
						*str_p) ; 
				#endif
					/*
					 * XXX
					 * hack for screen command (ESC [ 0xfa m).
					 */
					if( increment_str( &str_p , &left) == 0)
					{
						return  0 ;
					}
				}
				else if( is_dec_priv)
				{
					/* DEC private mode */

					if( *str_p == 'h')
					{
						/* DEC Private Mode Set */

						if( ps[0] == 1)
						{
							ml_term_screen_set_app_cursor_keys(
								vt100_parser->termscr) ;
						}
						else if( ps[0] == 3)
						{
							ml_term_screen_resize_columns(
								vt100_parser->termscr , 132) ;
						}
						else if( ps[0] == 5)
						{
							ml_term_screen_reverse_video(
								vt100_parser->termscr) ;
						}
						else if( ps[0] == 47)
						{
							/* Use Alternate Screen Buffer */

							ml_term_screen_use_alternative_image(
								vt100_parser->termscr) ;
						}
						else if( ps[0] == 1000)
						{
							ml_term_screen_set_mouse_pos_sending(
								vt100_parser->termscr) ;
						}
					#if  0
						else if( ps[0] == 1001)
						{
							/* X11 mouse highlighting */
						}
					#endif
						else
						{
						#ifdef  DEBUG
							kik_warn_printf( KIK_DEBUG_TAG
								" ESC - [ ? %d h is not implemented.\n" ,
								ps[0]) ;
						#endif
						}
					}
					else if( *str_p == 'l')
					{
						/* DEC Private Mode Reset */

						if( ps[0] == 1)
						{
							ml_term_screen_set_normal_cursor_keys(
								vt100_parser->termscr) ;
						}
						else if( ps[0] == 3)
						{
							ml_term_screen_resize_columns(
								vt100_parser->termscr , 80) ;
						}
						else if( ps[0] == 5)
						{
							ml_term_screen_restore_video(
								vt100_parser->termscr) ;
						}
						else if( ps[0] == 47)
						{
							/* Use Normal Screen Buffer */
							ml_term_screen_use_normal_image(
								vt100_parser->termscr) ;
						}
						else if( ps[0] == 1000)
						{
							ml_term_screen_unset_mouse_pos_sending(
								vt100_parser->termscr) ;
						}
						else
						{
						#ifdef  DEBUG
							kik_warn_printf( KIK_DEBUG_TAG
								" ESC - [ ? %d l is not implemented.\n" ,
								ps[0]) ;
						#endif
						}
					}
					else if( *str_p == 'r')
					{
						/* Restore DEC Private Mode */
					#ifdef  DEBUG
						kik_warn_printf( KIK_DEBUG_TAG
							" ESC - [ ? %d r is not implemented.\n" ,
							ps[0]) ;
					#endif
					}
					else if( *str_p == 's')
					{
						/* Save DEC Private Mode */
					#ifdef  DEBUG
						kik_warn_printf( KIK_DEBUG_TAG
							" ESC - [ ? %d s is not implemented.\n" ,
							ps[0]) ;
					#endif
					}
				#ifdef  DEBUG
					else
					{
						kik_warn_printf( KIK_DEBUG_TAG
							" receiving unknown csi sequence ESC - [ - ? - %c.\n"
							, *str_p) ;
					}
				#endif
				}
				else
				{
					if( *str_p == '@')
					{
						/* insert blank chars */

						if( num == 0)
						{
							ps[0] = 1 ;
						}

						/*
						 * inserting ps[0] blank characters.
						 */
						 
						flush_buffer( vt100_parser) ;

						ml_term_screen_insert_blank_chars( vt100_parser->termscr ,
							ps[0]) ;
					}
					else if( *str_p == 'A' || *str_p == 'e')
					{
						if( num == 0)
						{
							ps[0] = 1 ;
						}

						cursor_up( vt100_parser , ps[0]) ;
					}
					else if( *str_p == 'B')
					{
						if( num == 0)
						{
							ps[0] = 1 ;
						}

						cursor_down( vt100_parser , ps[0]) ;
					}
					else if( *str_p == 'C' || *str_p == 'a')
					{
						if( num == 0)
						{
							ps[0] = 1 ;
						}

						cursor_forward( vt100_parser , ps[0]) ;
					}
					else if( *str_p == 'D')
					{
						if( num == 0)
						{
							ps[0] = 1 ;
						}

						cursor_back( vt100_parser , ps[0]) ;
					}
					else if( *str_p == 'E')
					{
						/* down and goto first column */

						if( num == 0)
						{
							ps[0] = 1 ;
						}

						cursor_down( vt100_parser , ps[0]) ;
						ml_term_screen_goto_beg_of_line( vt100_parser->termscr) ;
					}
					else if( *str_p == 'F')
					{
						/* up and goto first column */

						if( num == 0)
						{
							ps[0] = 1 ;
						}

						cursor_up( vt100_parser , ps[0]) ;
						ml_term_screen_goto_beg_of_line( vt100_parser->termscr) ;
					}
					else if( *str_p == 'G' || *str_p == '`')
					{
						/* cursor position absolute(CHA or HPA) */
						
						if( num == 0)
						{
							ps[0] = 1 ;
						}

						cursor_go_horizontally( vt100_parser , ps[0] - 1) ;
					}
					else if( *str_p == 'H' || *str_p == 'f')
					{
						if( num == 0)
						{
							ps[0] = 1 ;
							ps[1] = 1 ;
						}
						else
						{
							/*
							 * some applications e.g. vin sometimes use 0 :(
							 */
							if( ps[0] == 0)
							{
								ps[0] = 1 ;
							}

							if( ps[1] == 0)
							{
								ps[1] = 1 ;
							}
						}

						cursor_goto( vt100_parser , ps[1] - 1 , ps[0] - 1) ;
					}
					else if( *str_p == 'I')
					{
					#ifdef  DEBUG
						kik_warn_printf( KIK_DEBUG_TAG
							" ESC - [ - I is not implemented.\n") ;
					#endif
					}
					else if( *str_p == 'J')
					{
						/* Erase in Display */

						if( num == 0 || ps[0] == 0)
						{
							clear_display_below( vt100_parser) ;
						}
						else if( ps[0] == 1)
						{
							clear_display_above( vt100_parser) ;
						}
						else if( ps[0] == 2)
						{
							clear_display_all( vt100_parser) ;
						}
					}
					else if( *str_p == 'K')
					{
						/* Erase in Line */

						if( num == 0 || ps[0] == 0)
						{
							clear_line_to_right( vt100_parser) ;
						}
						else if( ps[0] == 1)
						{
							clear_line_to_left( vt100_parser) ;
						}
						else if( ps[0] == 2)
						{
							clear_line_all( vt100_parser) ;
						}
					}
					else if( *str_p == 'L')
					{
						if( num == 0)
						{
							ps[0] = 1 ;
						}

						flush_buffer( vt100_parser) ;

						ml_term_screen_insert_new_lines(
							vt100_parser->termscr , ps[0]) ;
					}
					else if( *str_p == 'M')
					{
						if( num == 0)
						{
							ps[0] = 1 ;
						}

						flush_buffer( vt100_parser) ;
						ml_term_screen_delete_lines(
							vt100_parser->termscr , ps[0]) ;
					}
					else if( *str_p == 'P')
					{
						/* delete chars */

						if( num == 0)
						{
							ps[0] = 1 ;
						}

						delete_cols( vt100_parser , ps[0]) ;
					}
					else if( *str_p == 'S')
					{
					#ifdef  DEBUG
						kik_warn_printf( KIK_DEBUG_TAG
							" ESC - [ - S is not implemented.\n") ;
					#endif
					}
					else if( *str_p == 'T' || *str_p == '^')
					{
						/* initiate hilite mouse tracking. */

					#ifdef  DEBUG
						kik_warn_printf( KIK_DEBUG_TAG
							" ESC - [ - T(^) is not implemented.\n") ;
					#endif
					}
					else if( *str_p == 'X')
					{
					#ifdef  DEBUG
						kik_warn_printf( KIK_DEBUG_TAG
							" ESC - [ - X is not implemented.\n") ;
					#endif
					}
					else if( *str_p == 'c')
					{
						/* send device attributes */

						ml_term_screen_send_device_attr( vt100_parser->termscr) ;
					}
					else if( *str_p == 'd')
					{
						/* line position absolute(VPA) */

						if( num == 0)
						{
							ps[0] = 1 ;
						}

						cursor_go_vertically( vt100_parser , ps[0] - 1) ;
					}
					else if( *str_p == 'g')
					{
						/* tab clear */

						if( num == 0)
						{
							ml_term_screen_clear_tab_stop(
								vt100_parser->termscr) ;
						}
						else if( num == 1 && ps[0] == 3)
						{
							ml_term_screen_clear_all_tab_stops(
								vt100_parser->termscr) ;
						}
					}
					else if( *str_p == 'l')
					{
						if( num == 1)
						{
							if( ps[0] == 4)
							{
								/* replace mode */

								flush_buffer( vt100_parser) ;
								vt100_parser->buffer.output_func =
									ml_term_screen_overwrite_chars ;
							}
						}
					}
					else if( *str_p == 'h')
					{
						if( num == 1)
						{
							if( ps[0] == 4)
							{
								/* insert mode */

								flush_buffer( vt100_parser) ;
								vt100_parser->buffer.output_func =
									ml_term_screen_insert_chars ;
							}
						}
					}
					else if( *str_p == 'm')
					{
						int  counter ;
						
						if( num == 0)
						{
							ps[0] = 0 ;
							num = 1 ;
						}

						for( counter = 0 ; counter < num ; counter ++)
						{
							change_font_attr( vt100_parser , ps[counter]) ;
						}
					}
					else if( *str_p == 'n')
					{
						/* device status report */

						if( num == 1)
						{
							if( ps[0] == 5)
							{
								ml_term_screen_report_device_status(
									vt100_parser->termscr) ;
							}
							else if( ps[0] == 6)
							{
								ml_term_screen_report_cursor_position(
									vt100_parser->termscr) ;
							}
						}
					}
					else if( *str_p == 'r')
					{
						/* set scroll region */
						if( num == 2)
						{
							/*
							 * in case 0 is used.
							 */
							if( ps[0] == 0)
							{
								ps[0] = 1 ;
							}

							if( ps[1] == 0)
							{
								ps[1] = 1 ;
							}
							
							ml_term_screen_set_scroll_region(
								vt100_parser->termscr ,
								ps[0] - 1 , ps[1] - 1) ;
						}
					}
					else if( *str_p == 'x')
					{
						/* request terminal parameters */

					#ifdef  DEBUG
						kik_warn_printf( KIK_DEBUG_TAG
							" ESC - [ - x is not implemented.\n") ;
					#endif
					}
				#ifdef  DEBUG
					else
					{
						kik_warn_printf( KIK_DEBUG_TAG
							" unknown csi sequence ESC - [ - 0x%x is received.\n" ,
							*str_p , *str_p) ;
					}
				#endif
				}
			}
			else if( *str_p == ']')
			{
				u_char  digit[10] ;
				int  counter ;
				int  ps ;
				u_char *  pt ;

				if( increment_str( &str_p , &left) == 0)
				{
					return  0 ;
				}

				counter = 0 ;
				while( '0' <= *str_p && *str_p <= '9')
				{
					digit[counter++] = *str_p ;

					if( increment_str( &str_p , &left) == 0)
					{
						return  0 ;
					}
				}

				digit[counter] = '\0' ;

				/* if digit is illegal , ps is set 0. */
				ps = atoi( digit) ;

				if( *str_p == ';')
				{
					if( increment_str( &str_p , &left) == 0)
					{
						return  0 ;
					}
					
					pt = str_p ;
					while( *str_p != CTLKEY_BEL)
					{
						if( increment_str( &str_p , &left) == 0)
						{
							return  0 ;
						}
					}

					*str_p = '\0' ;

					if( ps == 0)
					{
						/* change icon name and window title */
						ml_term_screen_set_window_name( vt100_parser->termscr ,
							pt) ;
						ml_term_screen_set_icon_name( vt100_parser->termscr ,
							pt) ;
					}
					else if( ps == 1)
					{
						/* change icon name */
						ml_term_screen_set_icon_name( vt100_parser->termscr ,
							pt) ;
					}
					else if( ps == 2)
					{
						/* change window title */
						ml_term_screen_set_window_name( vt100_parser->termscr ,
							pt) ;
					}
					else if( ps == 46)
					{
						/* change log file */
					}
					else if( ps == 50)
					{
						/* set font */
					}
				}
			}
			else if( *str_p == '(' && IS_ENCODING_BASED_ON_ISO2022(vt100_parser->encoding) == 0)
			{
				/*
				 * this is for the case that vt100_parser->cc_parser is not compatible
				 * with ISO2022.
				 */
				 
				if( increment_str( &str_p , &left) == 0)
				{
					return  0 ;
				}
				
				if( *str_p == '0')
				{
					vt100_parser->is_graphic_char_in_gl = 1 ;
				}
				else if( *str_p == 'B')
				{
					vt100_parser->is_graphic_char_in_gl = 0 ;
				}
				else
				{
					/* not VT100 control sequence */

					return  1 ;
				}
			}
			else
			{
				/* not VT100 control sequence. */

				return  1 ;
			}

		#ifdef  ESCSEQ_DEBUG
			fprintf( stderr , "\n") ;
		#endif
		}
		else if( *str_p == CTLKEY_LF || *str_p == CTLKEY_VT)
		{
		#ifdef  ESCSEQ_DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " receiving LF\n") ;
		#endif

			flush_buffer( vt100_parser) ;
			ml_term_screen_line_feed( vt100_parser->termscr) ;
		}
		else if( *str_p == CTLKEY_CR)
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " receiving CR\n") ;
		#endif

			flush_buffer( vt100_parser) ;
			ml_term_screen_goto_beg_of_line( vt100_parser->termscr) ;
		}
		else if( *str_p == CTLKEY_TAB)
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " receiving TAB\n") ;
		#endif

			flush_buffer( vt100_parser) ;
			ml_term_screen_vertical_tab( vt100_parser->termscr) ;
		}
		else if( *str_p == CTLKEY_BS)
		{
		#ifdef  ESCSEQ_DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " receiving BS\n") ;
		#endif

			cursor_back( vt100_parser , 1) ;
		}
		else if( *str_p == CTLKEY_BEL)
		{
		#ifdef  ESCSEQ_DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " receiving BEL\n") ;
		#endif

			ml_term_screen_bel( vt100_parser->termscr) ;
		}
		else
		{
			/* not VT100 control sequence */
			
			return  1 ;
		}

		left -- ;
		str_p ++ ;
		
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " --> dumping\n") ;
		ml_image_dump( vt100_parser->termscr->image) ;
	#endif

		if( ( vt100_parser->left = left) == 0)
		{
			return  1 ;
		}
	}
}


/*
 * callbacks of pty encoding listener
 */
 
static int
encoding_changed(
	void *  p ,
	ml_char_encoding_t  encoding
	)
{
	ml_vt100_parser_t *  vt100_parser ;
	mkf_parser_t *  cc_parser ;
	mkf_conv_t *  cc_conv ;

	vt100_parser = p ;

	cc_conv = ml_conv_new( encoding) ;
	cc_parser = ml_parser_new( encoding) ;

	if( cc_parser == NULL || cc_conv == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " encoding not changed.\n") ;
	#endif
		if( cc_parser)
		{
			(*cc_parser->delete)( cc_parser) ;
		}

		if( cc_conv)
		{
			(*cc_conv->delete)( cc_conv) ;
		}

		return  0 ;
	}
	
#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " encoding changed.\n") ;
#endif

	(*vt100_parser->cc_parser->delete)( vt100_parser->cc_parser) ;
	(*vt100_parser->cc_conv->delete)( vt100_parser->cc_conv) ;

	vt100_parser->encoding = encoding ;
	vt100_parser->cc_parser = cc_parser ;
	vt100_parser->cc_conv = cc_conv ;

	/* reset */
	vt100_parser->is_graphic_char_in_gl = 0 ;
	
	return  1 ;
}

static ml_char_encoding_t
pty_encoding(
	void *  p
	)
{
	ml_vt100_parser_t *  vt100_parser ;

	vt100_parser = p ;

	return  vt100_parser->encoding ;
}

static size_t
convert_to_pty_encoding(
	void *  p ,
	u_char *  dst ,
	size_t  len ,
	mkf_parser_t *  parser
	)
{
	ml_vt100_parser_t *  vt100_parser ;

	vt100_parser = p ;

	return  (*vt100_parser->cc_conv->convert)( vt100_parser->cc_conv , dst , len , parser) ;
}

static int
init_pty_encoding(
	void *  p
	)
{
	ml_vt100_parser_t *  vt100_parser ;

	vt100_parser = p ;

	(*vt100_parser->cc_conv->init)( vt100_parser->cc_conv) ;
	(*vt100_parser->cc_parser->init)( vt100_parser->cc_parser) ;

	return  1 ;
}


/* --- global functions --- */

ml_vt100_parser_t *
ml_vt100_parser_new(
	ml_term_screen_t *  termscr ,
	ml_char_encoding_t  encoding ,
	int  unicode_to_other_cs ,
	int  all_cs_to_unicode ,
	u_int  col_size_a
	)
{
	ml_vt100_parser_t *  vt100_parser ;

	if( ( vt100_parser = malloc( sizeof( ml_vt100_parser_t))) == NULL)
	{
		return  NULL ;
	}
	
	vt100_parser->left = 0 ;
	vt100_parser->len = 0 ;

	ml_str_init( vt100_parser->buffer.chars , PTYMSG_BUFFER_SIZE) ;	
	vt100_parser->buffer.len = 0 ;
	vt100_parser->buffer.output_func = ml_term_screen_overwrite_chars ;

	vt100_parser->pty = NULL ;
	
	vt100_parser->encoding_listener.self = vt100_parser ;
	vt100_parser->encoding_listener.encoding_changed = encoding_changed ;
	vt100_parser->encoding_listener.encoding = pty_encoding ;
	vt100_parser->encoding_listener.convert = convert_to_pty_encoding ;
	vt100_parser->encoding_listener.init = init_pty_encoding ;

	if( ! ml_set_encoding_listener( termscr , &vt100_parser->encoding_listener))
	{
		goto error ;
	}
	
	vt100_parser->termscr = termscr ;
	
	vt100_parser->font_attr = DEFAULT_FONT_ATTR(0) ;
	vt100_parser->font_decor = 0 ;
	vt100_parser->saved_attr = DEFAULT_FONT_ATTR(0) ;
	vt100_parser->saved_decor = 0 ;
	vt100_parser->fg_color = MLC_FG_COLOR ;
	vt100_parser->bg_color = MLC_BG_COLOR ;
	vt100_parser->is_reversed = 0 ;
	vt100_parser->font = NULL ;
	vt100_parser->cs = UNKNOWN_CS ;

	vt100_parser->unicode_to_other_cs = unicode_to_other_cs ;
	vt100_parser->all_cs_to_unicode = all_cs_to_unicode ;

	if( ( vt100_parser->cc_conv = ml_conv_new( encoding)) == NULL)
	{
		goto  error ;
	}

	if( ( vt100_parser->cc_parser = ml_parser_new( encoding)) == NULL)
	{
		(*vt100_parser->cc_conv->delete)( vt100_parser->cc_conv) ;

		goto  error ;
	}

	vt100_parser->encoding = encoding ;

	vt100_parser->is_graphic_char_in_gl = 0 ;

	if( col_size_a == 1 || col_size_a == 2)
	{
		vt100_parser->col_size_of_east_asian_width_a = col_size_a ;
	}
	else if( col_size_a == 0)
	{
		/* default value */
		
		vt100_parser->col_size_of_east_asian_width_a = 1 ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG , " col size should be 1 or 2. default value 1 is used.\n") ;
	#endif
	
		vt100_parser->col_size_of_east_asian_width_a = 1 ;
	}

	return  vt100_parser ;

error:
	free( vt100_parser) ;

	return  NULL ;
}

int
ml_vt100_parser_delete(
	ml_vt100_parser_t *  vt100_parser
	)
{
	ml_str_final( vt100_parser->buffer.chars , PTYMSG_BUFFER_SIZE) ;
	(*vt100_parser->cc_parser->delete)( vt100_parser->cc_parser) ;
	(*vt100_parser->cc_conv->delete)( vt100_parser->cc_conv) ;

	free( vt100_parser) ;
	
	return  1 ;
}

int
ml_vt100_parser_set_pty(
	ml_vt100_parser_t *  vt100_parser ,
	ml_pty_t *  pty
	)
{
	if( vt100_parser->pty)
	{
		/* already set */
		
		return  0 ;
	}
	
	vt100_parser->pty = pty ;

	return  1 ;
}

int
ml_parse_vt100_sequence(
	ml_vt100_parser_t *  vt100_parser
	)
{
	mkf_char_t  ch ;
	size_t  prev_left ;

	if( vt100_parser->pty == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " pty is not set.\n") ;
	#endif
	
		return  0 ;
	}

	ml_term_screen_unhighlight_cursor( vt100_parser->termscr) ;

	while( receive_bytes( vt100_parser))
	{
		ml_term_screen_stop_bidi( vt100_parser->termscr) ;

		/*
		 * bidi is always stopped from here.
		 */
		 
		while( 1)
		{
			prev_left = vt100_parser->left ;

			/*
			 * parsing character encoding.
			 */

			(*vt100_parser->cc_parser->set_str)( vt100_parser->cc_parser ,
				CURRENT_STR_P(vt100_parser) , vt100_parser->left) ;

			while( (*vt100_parser->cc_parser->next_char)( vt100_parser->cc_parser , &ch))
			{
				/*
				 * NON UCS <-> NON UCS
				 */
				{
					/*
					 * XXX hack
					 * how to deal with johab 10-4-4(8-4-4) font ?
					 * is there any uhc font ?
					 */
					 
					mkf_char_t  uhc ;

					if( ch.cs == JOHAB)
					{
						if( mkf_map_johab_to_uhc( &uhc , &ch) == 0)
						{
							continue ;
						}

						ch = uhc ;
					}

					/*
					 * XXX
					 * switching option whether this conversion is done should
					 * be introduced.
					 */
					if( ch.cs == UHC)
					{
						if( mkf_map_uhc_to_ksc5601_1987( &ch , &uhc) == 0)
						{
							continue ;
						}
					}
				}

				/*
				 * UCS <-> OTHER CS
				 */ 
				if( ch.cs == ISO10646_UCS4_1)
				{
					if( ch.ch[0] == 0x00 && ch.ch[1] == 0x00 &&
						ch.ch[2] == 0x00 && ch.ch[3] <= 0x7f
						)
					{
						/* this is always done */
						ch.ch[0] = ch.ch[3] ;
						ch.size = 1 ;
						ch.cs = US_ASCII ;
					}
					else if( vt100_parser->unicode_to_other_cs)
					{
						/* convert ucs4 to appropriate charset */

						mkf_char_t  non_ucs ;

						if( mkf_map_ucs4_to( &non_ucs , &ch) == 0)
						{
						#ifdef  DEBUG
							kik_warn_printf( KIK_DEBUG_TAG
							" failed to convert ucs4 to other cs , ignored.\n") ;
						#endif
							continue ;
						}
						else
						{
							ch = non_ucs ;
						}
					}
				#ifndef  USE_UCS4
					else
					{
						/* change UCS4 to UCS2 */
						ch.ch[0] = ch.ch[2] ;
						ch.ch[1] = ch.ch[3] ;
						ch.size = 2 ;
						ch.cs = ISO10646_UCS2_1 ;
					}
				#endif
				}
				else if( vt100_parser->all_cs_to_unicode && ch.cs != US_ASCII)
				{
					mkf_char_t  ucs ;

					if( mkf_map_to_ucs4( &ucs , &ch))
					{
					#ifdef  USE_UCS4
						ch = ucs ;
					#else
						ch.ch[0] = ucs.ch[2] ;
						ch.ch[1] = ucs.ch[3] ;
						ch.size = 2 ;
						ch.cs = ISO10646_UCS2_1 ;
						ch.property = ucs.property ;
					#endif
					}
				#ifdef  DEBUG
					else
					{
						kik_warn_printf( KIK_DEBUG_TAG
							" mkf_convert_to_ucs4_char() failed, ignored.\n") ;
					}
				#endif
				}

				if( ch.size == 1 && ch.ch[0] == 0x0)
				{
				#ifdef  DEBUG
					kik_warn_printf( KIK_DEBUG_TAG
						" 0x0 sequence is received , ignored...\n") ;
				#endif
				}
				else if( ch.size == 1 && 0x1 <= ch.ch[0] && ch.ch[0] <= 0x1f)
				{
					/*
					 * this is a control sequence.
					 * reparsing this char in vt100_escape_sequence() ...
					 */

					vt100_parser->cc_parser->left ++ ;
					vt100_parser->cc_parser->is_eos = 0 ;

					break ;
				}
				else
				{
					vt100_parser->left = vt100_parser->cc_parser->left ;

					if( is_msb_set( ch.cs))
					{
						SET_MSB( ch.ch[0]) ;
					}

					if( ( ch.cs == US_ASCII && vt100_parser->is_graphic_char_in_gl) ||
						ch.cs == DEC_SPECIAL)
					{
						if( ch.ch[0] == 0x5f)
						{
							ch.ch[0] = 0x7f ;
						}
						else if( 0x5f < ch.ch[0] && ch.ch[0] < 0x7f)
						{
							ch.ch[0] -= 0x5f ;
						}

						ch.cs = DEC_SPECIAL ;
						ch.property = 0 ;
					}
					
					put_char( vt100_parser , ch.ch , ch.size , ch.cs ,
						ch.property) ;
				}
			}

			vt100_parser->left = vt100_parser->cc_parser->left ;

			if( vt100_parser->cc_parser->is_eos)
			{
				break ;
			}

			/*
			 * parsing other vt100 sequences.
			 */

			if( ! parse_vt100_escape_sequence( vt100_parser))
			{
				/* shortage of chars */

				break ;
			}

			if( vt100_parser->left == prev_left)
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG
					" unrecognized sequence is received , ignored...\n") ;
			#endif

				vt100_parser->left -- ;
			}

			if( vt100_parser->left == 0)
			{
				break ;
			}
		}
		
		flush_buffer( vt100_parser) ;

		ml_term_screen_render_bidi( vt100_parser->termscr) ;
		
		/*
		 * bidi is always stopped until here.
		 */

		ml_term_screen_start_bidi( vt100_parser->termscr) ;
		
		ml_term_screen_redraw_image( vt100_parser->termscr) ;
	}

	ml_term_screen_highlight_cursor( vt100_parser->termscr) ;
	
	ml_term_screen_start_bidi( vt100_parser->termscr) ;

	return  1 ;
}
