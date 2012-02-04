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


/*
 * Possible patterns are:
 *  NOT_USE_UNICODE_FONT(0x1)
 *  USE_UNICODE_PROPERTY(0x2)
 *  NOT_USE_UNICODE_FONT|USE_UNICODE_PROPERTY(0x3)
 *  ONLY_USE_UNICODE_FONT(0x4)
 */
typedef enum  ml_unicode_policy
{
	NO_UNICODE_POLICY = 0x0 ,
	NOT_USE_UNICODE_FONT = 0x1 ,
	USE_UNICODE_PROPERTY = 0x2 ,
	ONLY_USE_UNICODE_FONT = 0x4 ,
	
	UNICODE_POLICY_MAX

} ml_unicode_policy_t ;

typedef enum  ml_mouse_report_mode
{
	NO_MOUSE_REPORT = 0 ,

	MOUSE_REPORT = 0x1 ,
	BUTTON_EVENT_MOUSE_REPORT = 0x2 ,
	ANY_EVENT_MOUSE_REPORT = 0x3 ,

	/* OR with above values. */
	EXTENDED_MOUSE_REPORT = 0x4

} ml_mouse_report_mode_t ;

typedef struct  ml_char_buffer
{
	ml_char_t  chars[PTY_WR_BUFFER_SIZE] ;
	u_int  len ;

	/* for "CSI b"(REP) sequence */
	ml_char_t *  last_ch ;

	int (*output_func)( ml_screen_t * , ml_char_t *  chars , u_int) ;

}  ml_char_buffer_t ;

typedef struct  ml_xterm_event_listener
{
	void *  self ;

	void (*start)( void *) ;	/* called in *visual* context. (Note that not logical) */
	void (*stop)( void *) ;		/* called in visual context. */
	
	void (*resize)( void * , u_int , u_int) ;	/* called in visual context. */
	void (*reverse_video)( void * , int) ;		/* called in visual context. */
	void (*set_mouse_report)( void * , ml_mouse_report_mode_t) ;/* called in visual context. */
	void (*set_window_name)( void * , u_char *) ;	/* called in logical context. */
	void (*set_icon_name)( void * , u_char *) ;	/* called in logical context. */
	void (*bel)( void *) ;				/* called in visual context. */
	int (*im_is_active)( void *) ;			/* called in logical context. */
	void (*switch_im_mode)( void *) ;		/* called in logical context. */
	void (*set_selection)( void * , ml_char_t * , u_int) ;	/* called in logical context. */

} ml_xterm_event_listener_t ;

/*
 * !! Notice !!
 * Validation of Keys and vals is not checked before these event called by ml_vt100_parser.
 */
typedef struct  ml_config_event_listener
{
	void *  self ;

	/* Assume that exec, set and get affect each window. */
	int (*exec)( void * , char *) ;
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

	ml_unicode_policy_t  unicode_policy ;

	ml_xterm_event_listener_t *  xterm_listener ;
	ml_config_event_listener_t *  config_listener ;

	int  log_file ;

	ml_mouse_report_mode_t  mouse_mode ;

	/* Used for non iso2022 encoding */
	int8_t  is_dec_special_in_gl ;
	int8_t  is_so ;
	int8_t  is_dec_special_in_g0 ;
	int8_t  is_dec_special_in_g1 ;
	
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

	int8_t  is_app_keypad ;
	int8_t  is_app_cursor_keys ;
	int8_t  is_app_escape ;
	int8_t  is_bracketed_paste_mode ;

	int8_t  im_is_active ;

	/* for save/restore cursor */
	ml_vt100_storable_states_t  saved_normal ;
	ml_vt100_storable_states_t  saved_alternate ;

} ml_vt100_parser_t ;


ml_vt100_parser_t *  ml_vt100_parser_new( ml_screen_t *  screen , ml_char_encoding_t  encoding ,
	ml_unicode_policy_t  policy , u_int  col_size_a ,
	int  use_char_combining , int  use_multi_col_char) ;

int  ml_vt100_parser_delete( ml_vt100_parser_t *  vt100_parser) ;

int  ml_vt100_parser_set_pty( ml_vt100_parser_t *  vt100_parser , ml_pty_ptr_t  pty) ;

int  ml_vt100_parser_set_xterm_listener( ml_vt100_parser_t *  vt100_parser ,
	ml_xterm_event_listener_t *  xterm_listener) ;

int  ml_vt100_parser_set_config_listener( ml_vt100_parser_t *  vt100_parser ,
	ml_config_event_listener_t *  config_listener) ;

int  ml_vt100_parser_set_unicode_policy( ml_vt100_parser_t *  vt100_parser ,
	ml_unicode_policy_t  policy) ;

int  ml_parse_vt100_sequence( ml_vt100_parser_t *  vt100_parser) ;

int  ml_parse_vt100_write_loopback( ml_vt100_parser_t *  vt100_parser ,
	u_char *  buf , size_t  len) ;

int  ml_vt100_parser_change_encoding( ml_vt100_parser_t *  vt100_parser ,
	ml_char_encoding_t  encoding) ;

ml_char_encoding_t  ml_vt100_parser_get_encoding( ml_vt100_parser_t *  vt100_parser) ;

size_t  ml_vt100_parser_convert_to( ml_vt100_parser_t *  vt100_parser ,
	u_char *  dst , size_t  len , mkf_parser_t *  parser) ;

int  ml_init_encoding_parser( ml_vt100_parser_t *  vt100_parser) ;

int  ml_init_encoding_conv( ml_vt100_parser_t *  vt100_parser) ;

int  ml_vt100_parser_set_col_size_of_width_a( ml_vt100_parser_t *  vt100_parser ,
	u_int  col_size_a) ;

#define  ml_vt100_parser_get_col_size_of_width_a( vt100_parser) \
		((vt100_parser)->col_size_of_width_a)

int  ml_vt100_parser_set_use_char_combining( ml_vt100_parser_t *  vt100_parser , int  flag) ;

#define  ml_vt100_parser_is_using_char_combining( vt100_parser) \
		((vt100_parser)->use_char_combining)

int  ml_vt100_parser_set_use_multi_col_char( ml_vt100_parser_t *  vt100_parser , int  flag) ;

#define  ml_vt100_parser_is_using_multi_col_char( vt100_parser) \
		((vt100_parser)->use_multi_col_char)

int  ml_vt100_parser_set_logging_vt_seq( ml_vt100_parser_t *  vt100_parser , int  flag) ;

#define  ml_vt100_parser_is_logging_vt_seq( vt100_parser)  ((vt100_parser)->logging_vt_seq)

#define  ml_vt100_parser_get_mouse_report_mode( vt100_parser) \
		((vt100_parser)->mouse_mode & ~EXTENDED_MOUSE_REPORT)

#define  ml_vt100_parser_is_extended_mouse_report_mode( vt100_parser) \
		((vt100_parser)->mouse_mode & EXTENDED_MOUSE_REPORT)

#define  ml_vt100_parser_is_app_keypad( vt100_parser)  ((vt100_parser)->is_app_keypad)

#define  ml_vt100_parser_is_app_cursor_keys( vt100_parser)  ((vt100_parser)->is_app_cursor_keys)

#define  ml_vt100_parser_is_app_escape( vt100_parser)  ((vt100_parser)->is_app_escape)

#define  ml_vt100_parser_is_bracketed_paste_mode( vt100_parser) \
		((vt100_parser)->is_bracketed_paste_mode)

#endif
