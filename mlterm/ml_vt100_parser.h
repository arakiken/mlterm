/*
 *	$Id$
 */

#ifndef  __ML_VT100_PARSER_H__
#define  __ML_VT100_PARSER_H__


#include  <kiklib/kik_types.h>	/* u_xxx */
#include  <mkf/mkf_parser.h>
#include  <mkf/mkf_conv.h>

#include  "ml_pty.h"
#include  "ml_screen.h"
#include  "ml_char_encoding.h"


/* the same as kterm BUF_SIZE in ptyx.h */
#define  PTYMSG_BUFFER_SIZE	4096


typedef enum  ml_unicode_font_policy
{
	NO_UNICODE_FONT_POLICY = 0x0 ,
	NOT_USE_UNICODE_FONT ,
	ONLY_USE_UNICODE_FONT ,
	
} ml_unicode_font_policy_t ;

typedef struct  ml_char_buffer
{
	ml_char_t  chars[PTYMSG_BUFFER_SIZE] ;
	
	u_int  len ;
	int (*output_func)( ml_screen_t * , ml_char_t *  chars , u_int) ;

}  ml_char_buffer_t ;

typedef struct  ml_xterm_event_listener
{
	void *  self ;

	void (*start)( void *) ;
	void (*start_2)( void *) ;
	void (*stop)( void *) ;
	
	void (*set_app_keypad)( void * , int) ;
	void (*set_app_cursor_keys)( void * , int) ;
	void (*resize_columns)( void * , u_int) ;
	void (*reverse_video)( void * , int) ;
	void (*set_mouse_report)( void * , int) ;
	void (*set_window_name)( void * , u_char *) ;
	void (*set_icon_name)( void * , u_char *) ;
	void (*bel)( void *) ;

} ml_xterm_event_listener_t ;

typedef struct  ml_config_event_listener
{
	void *  self ;

	void (*set)( void * , char * , char * , char *) ;
	void (*get)( void * , char * , char * , int) ;

} ml_config_event_listener_t ;

typedef struct  ml_vt100_parser
{
	u_char  seq[PTYMSG_BUFFER_SIZE] ;
	size_t  len ;
	size_t  left ;

	ml_char_buffer_t  buffer ;
	
	ml_pty_t *  pty ;
	
	ml_screen_t *  screen ;

	mkf_parser_t *  cc_parser ;	/* char code parser */
	mkf_conv_t *  cc_conv ;		/* char code converter */
	ml_char_encoding_t  encoding ;

	ml_color_t  fg_color ;
	ml_color_t  bg_color ;
	
	mkf_charset_t  cs ;

	ml_unicode_font_policy_t  unicode_font_policy ;

	ml_xterm_event_listener_t *  xterm_listener ;
	ml_config_event_listener_t *  config_listener ;

	int  log_file ;

	int8_t  is_dec_special_in_gl ;
	int8_t  is_so ;
	int8_t  is_dec_special_in_g0 ;
	int8_t  is_dec_special_in_g1 ;
	
	int8_t  not_use_unicode_font ;
	int8_t  only_use_unicode_font ;
	
	u_int8_t  col_size_of_east_asian_width_a ;	/* 1 or 2 */
	
	int8_t  is_bold ;
	int8_t  is_underlined ;
	int8_t  is_reversed ;
	int8_t  use_char_combining ;
	int8_t  use_multi_col_char ;
	int8_t  logging_vt_seq ;
	
} ml_vt100_parser_t ;


ml_vt100_parser_t *  ml_vt100_parser_new( ml_screen_t *  screen , ml_char_encoding_t  encoding ,
	ml_unicode_font_policy_t  policy , u_int  col_size_a ,
	int  use_char_combining , int  use_multi_col_char) ;

int  ml_vt100_parser_delete( ml_vt100_parser_t *  vt100_parser) ;

int  ml_vt100_parser_set_pty( ml_vt100_parser_t *  vt100_parser , ml_pty_t *  pty) ;

int  ml_vt100_parser_set_xterm_listener( ml_vt100_parser_t *  vt100_parser ,
	ml_xterm_event_listener_t *  xterm_listener) ;

int  ml_vt100_parser_set_config_listener( ml_vt100_parser_t *  vt100_parser ,
	ml_config_event_listener_t *  config_listener) ;

int  ml_vt100_parser_set_unicode_font_policy( ml_vt100_parser_t *  vt100_parser ,
	ml_unicode_font_policy_t  policy) ;

int  ml_parse_vt100_sequence( ml_vt100_parser_t *  vt100_parser) ;

int  ml_vt100_parser_change_encoding( ml_vt100_parser_t *  vt100_parser , ml_char_encoding_t  encoding) ;

ml_char_encoding_t  ml_vt100_parser_get_encoding( ml_vt100_parser_t *  vt100_parser) ;

size_t  ml_vt100_parser_convert_to( ml_vt100_parser_t *  vt100_parser ,
	u_char *  dst , size_t  len , mkf_parser_t *  parser) ;

int  ml_init_encoding_parser( ml_vt100_parser_t *  vt100_parser) ;

int  ml_init_encoding_conv( ml_vt100_parser_t *  vt100_parser) ;

int  ml_vt100_parser_enable_logging_vt_seq( ml_vt100_parser_t *  vt100_parser) ;


#endif
