/*
 *	update: <2001/11/14(05:08:20)>
 *	$Id$
 */

#ifndef  __ML_VT100_PARSER_H__
#define  __ML_VT100_PARSER_H__


#include  <kiklib/kik_types.h>	/* u_xxx */
#include  <mkf/mkf_parser.h>
#include  <mkf/mkf_conv.h>

#include  "ml_pty.h"
#include  "ml_term_screen.h"
#include  "ml_encoding.h"


/* the same as kterm BUF_SIZE in ptyx.h */
#define  PTYMSG_BUFFER_SIZE	4096


typedef struct  ml_char_buffer
{
	ml_char_t  chars[PTYMSG_BUFFER_SIZE] ;
	
	u_int  len ;
	int (*output_func)(ml_term_screen_t * , ml_char_t *  chars , u_int) ;

}  ml_char_buffer_t ;

typedef struct  ml_vt100_parser
{
	u_char  seq[PTYMSG_BUFFER_SIZE] ;
	size_t  len ;
	size_t  left ;

	ml_char_buffer_t  buffer ;
	
	ml_pty_t *  pty ;

	mkf_parser_t *  cc_parser ;	/* char code parser */
	mkf_conv_t *  cc_conv ;		/* char code converter */
	ml_encoding_type_t  encoding ;

	int  is_graphic_char_in_gl ;
	
	int  unicode_to_other_cs ;	/* whether unicode chars are converted to other cs or not */
	int  all_cs_to_unicode ;	/* whether all chars are converted to unicode or not */
	int  conv_to_generic_iso2022 ;
	size_t  col_size_of_east_asian_width_a ;
	
	ml_font_attr_t  font_attr ;
	ml_font_decor_t  font_decor ;
	ml_font_attr_t  saved_attr ;
	ml_font_decor_t  saved_decor ;
	u_long  fg_color ;
	u_long  bg_color ;

	int  is_reversed ;
	
	mkf_charset_t  cs ;
	ml_font_t *  font ;

	ml_term_screen_t *  termscr ;

	ml_encoding_event_listener_t  encoding_listener ;

} ml_vt100_parser_t ;


ml_vt100_parser_t *  ml_vt100_parser_new( ml_term_screen_t *  term_window ,
	ml_encoding_type_t  type , int  unicode_to_other_cs , int  all_cs_to_unicode ,
	int  conv_to_generic_iso2022 , size_t  col_size_a) ;

int  ml_vt100_parser_delete( ml_vt100_parser_t *  vt100_parser) ;

int  ml_vt100_parser_set_pty( ml_vt100_parser_t *  vt100_parser , ml_pty_t *  pty) ;
	
int  ml_set_col_size_of_east_asian_width_a( int  col_size) ;

int  ml_parse_vt100_sequence( ml_vt100_parser_t *  vt100_parser) ;


#endif
