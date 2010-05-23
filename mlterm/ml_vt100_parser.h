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


/*
 * kterm BUF_SIZE in ptyx.h is 4096.
 * Following size is adjusted to suppress sizeof(ml_vt100_parser_t) to 4KB.
 * (3984 bytes in ILP32, 4020 bytes in LP64.)
 * Maximum size of sequence parsed once is PTY_RD_BUFFER_SIZE * 3.
 * (see ml_parse_vt100_sequence)
 */
#define  PTY_RD_BUFFER_SIZE  3072
#define  PTY_WR_BUFFER_SIZE  100


typedef enum  ml_unicode_font_policy
{
	NO_UNICODE_FONT_POLICY = 0x0 ,
	NOT_USE_UNICODE_FONT ,
	ONLY_USE_UNICODE_FONT ,
	
	UNICODE_FONT_POLICY_MAX

} ml_unicode_font_policy_t ;

typedef struct  ml_char_buffer
{
	ml_char_t  chars[PTY_WR_BUFFER_SIZE] ;
	
	u_int  len ;
	int (*output_func)( ml_screen_t * , ml_char_t *  chars , u_int) ;

}  ml_char_buffer_t ;

typedef struct  ml_xterm_event_listener
{
	void *  self ;

	void (*start)( void *) ;
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

/*
 * !! Notice !!
 * Validation of Keys and vals is not checked before these event called by ml_vt100_parser.
 */
typedef struct  ml_config_event_listener
{
	void *  self ;

	/* Assume that set and get affect each window. */
	void (*set)( void * , char * , char * , char *) ;
	void (*get)( void * , char * , char * , int) ;

	/* Assume that saved, set_font and set_color affect all window. */
	void (*saved)(void) ;		/* Event that mlterm/main file was changed. */
	
	void (*set_font)( void * , char * , char * , char * , int) ;
	void (*get_font)( void * , char * , char * , char * , int) ;
	void (*set_color)( void * , char * , char * , char * , int) ;

} ml_config_event_listener_t ;

typedef struct  ml_vt100_storable_states
{
	int8_t  is_saved ;
	
	int8_t  is_bold ;
	int8_t  is_underlined ;
	int8_t  is_reversed ;
	ml_color_t  fg_color ;
	ml_color_t  bg_color ;
	mkf_charset_t  cs ;

} ml_vt100_storable_states_t ;


typedef struct  ml_vt100_parser
{
	u_char  seq[PTY_RD_BUFFER_SIZE] ;
	size_t  len ;
	size_t  left ;

	ml_char_buffer_t  buffer ;
	
	ml_pty_ptr_t  pty ;
	
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
	
	int8_t  is_bold ;
	int8_t  is_underlined ;
	int8_t  is_reversed ;

	u_int8_t  col_size_of_width_a ;	/* 1 or 2 */

	/*
	 * XXX
	 * ml_term can access these 3 flags directly.
	 */
	int8_t  use_char_combining ;
	int8_t  use_multi_col_char ;
	int8_t  logging_vt_seq ;

	/* for save/restore cursor */
	ml_vt100_storable_states_t  saved_normal ;
	ml_vt100_storable_states_t  saved_alternate ;

} ml_vt100_parser_t ;


ml_vt100_parser_t *  ml_vt100_parser_new( ml_screen_t *  screen , ml_char_encoding_t  encoding ,
	ml_unicode_font_policy_t  policy , u_int  col_size_a ,
	int  use_char_combining , int  use_multi_col_char) ;

int  ml_vt100_parser_delete( ml_vt100_parser_t *  vt100_parser) ;

int  ml_vt100_parser_set_pty( ml_vt100_parser_t *  vt100_parser , ml_pty_ptr_t  pty) ;

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

int  ml_vt100_parser_set_col_size_of_width_a( ml_vt100_parser_t *  vt100_parser ,
	u_int  col_size_a) ;

u_int  ml_vt100_parser_get_col_size_of_width_a( ml_vt100_parser_t *  vt100_parser) ;


#endif
