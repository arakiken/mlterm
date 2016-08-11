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
#include  "ml_drcs.h"
#include  "ml_termcap.h"


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
	ONLY_USE_UNICODE_FONT = 0x2 ,
	NOT_USE_UNICODE_BOXDRAW_FONT = 0x4 ,
	ONLY_USE_UNICODE_BOXDRAW_FONT = 0x8 ,
	USE_UNICODE_DRCS = 0x10 ,
	CONVERT_UNICODE_TO_ISCII = 0x20 ,

	UNICODE_POLICY_MAX

} ml_unicode_policy_t ;

typedef enum  ml_mouse_report_mode
{
	NO_MOUSE_REPORT = 0 ,

	MOUSE_REPORT = 0x1 ,
	BUTTON_EVENT_MOUSE_REPORT = 0x2 ,
	ANY_EVENT_MOUSE_REPORT = 0x3 ,
	LOCATOR_CHARCELL_REPORT = 0x4 ,
	LOCATOR_PIXEL_REPORT = 0x5 ,

} ml_mouse_report_mode_t ;

typedef enum  ml_extended_mouse_report_mode
{
	NO_EXTENDED_MOUSE_REPORT = 0 ,

	EXTENDED_MOUSE_REPORT_UTF8 = 0x1 ,
	EXTENDED_MOUSE_REPORT_SGR = 0x2 ,
	EXTENDED_MOUSE_REPORT_URXVT = 0x3 ,

} ml_extended_mouse_report_mode_t ;

typedef enum  ml_locator_report_mode
{
	LOCATOR_BUTTON_DOWN = 0x1 ,
	LOCATOR_BUTTON_UP = 0x2 ,
	LOCATOR_ONESHOT = 0x4 ,
	LOCATOR_REQUEST = 0x8 ,
	LOCATOR_FILTER_RECT = 0x10 ,

} ml_locator_report_mode_t ;

typedef enum  ml_alt_color_mode
{
	ALT_COLOR_BOLD = 0x1 ,
	ALT_COLOR_ITALIC = 0x2 ,
	ALT_COLOR_UNDERLINE = 0x4 ,
	ALT_COLOR_BLINKING = 0x8 ,
	ALT_COLOR_CROSSED_OUT = 0x10 ,

} ml_alt_color_mode_t ;

typedef struct  ml_write_buffer
{
	ml_char_t  chars[PTY_WR_BUFFER_SIZE] ;
	u_int  filled_len ;

	/* for "CSI b"(REP) sequence */
	ml_char_t *  last_ch ;

	int (*output_func)( ml_screen_t * , ml_char_t *  chars , u_int) ;

} ml_write_buffer_t ;

typedef struct  ml_read_buffer
{
	u_char *  chars ;
	size_t  len ;
	size_t  filled_len ;
	size_t  left ;
	size_t  new_len ;

} ml_read_buffer_t ;

typedef struct  ml_xterm_event_listener
{
	void *  self ;

	void (*start)( void *) ;	/* called in *visual* context. (Note that not logical) */
	void (*stop)( void *) ;		/* called in visual context. */
	void (*interrupt)( void *) ;	/* called in visual context. */

	void (*resize)( void * , u_int , u_int) ;	/* called in visual context. */
	void (*reverse_video)( void * , int) ;		/* called in visual context. */
	void (*set_mouse_report)( void *) ;	/* called in visual context. */
	void (*request_locator)( void *) ;	/* called in visual context. */
	void (*set_window_name)( void * , u_char *) ;	/* called in logical context. */
	void (*set_icon_name)( void * , u_char *) ;	/* called in logical context. */
	void (*bel)( void *) ;				/* called in visual context. */
	int (*im_is_active)( void *) ;			/* called in logical context. */
	void (*switch_im_mode)( void *) ;		/* called in logical context. */
	void (*set_selection)( void * , ml_char_t * , u_int , u_char *) ;	/* called in logical context. */
	int (*get_window_size)( void * , u_int * , u_int *) ;	/* called in logical context. */
	int (*get_rgb)( void * , u_int8_t * , u_int8_t * ,
			u_int8_t * , ml_color_t) ;		/* called in logical context. */
	ml_char_t *  (*get_picture_data)( void * , char * ,
			int * , int * , u_int32_t **) ; /* called in logical context. */
	int (*get_emoji_data)( void * , ml_char_t * , ml_char_t *) ;	/* called in logical context. */
	void (*show_sixel)( void * , char *) ;		/* called in logical context. */
	void (*add_frame_to_animation)( void * , char * , int * , int *) ; /* called in logical context. */
	void (*hide_cursor)( void * , int) ;		/* called in logical context. */

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
	int (*set)( void * , char * , char * , char *) ;
	void (*get)( void * , char * , char * , int) ;

	/* Assume that saved, set_font and set_color affect all window. */
	void (*saved)(void) ;		/* Event that mlterm/main file was changed. */
	
	void (*set_font)( void * , char * , char * , char * , int) ;
	void (*get_font)( void * , char * , char * , int) ;
	void (*set_color)( void * , char * , char * , char * , int) ;
	void (*get_color)( void * , char * , int) ;

} ml_config_event_listener_t ;

typedef struct  ml_vt100_parser *  ml_vt100_parser_ptr_t ;

typedef struct  ml_vt100_storable_states
{
	int8_t  is_saved ;
	
	int8_t  is_bold ;
	int8_t  is_italic ;
	int8_t  underline_style ;
	int8_t  is_reversed ;
	int8_t  is_crossed_out ;
	int8_t  is_blinking ;
	ml_color_t  fg_color ;
	ml_color_t  bg_color ;
	mkf_charset_t  cs ;

} ml_vt100_storable_states_t ;

typedef struct  ml_vt100_saved_names
{
	char **  names ;
	u_int  num ;

} ml_vt100_saved_names_t ;

typedef struct  ml_macro
{
	u_char *  str ;
	int8_t  is_sixel ;
	u_int8_t  sixel_num ;

} ml_macro_t ;

typedef struct  ml_vt100_parser
{
	ml_read_buffer_t  r_buf ;
	ml_write_buffer_t  w_buf ;
	
	ml_pty_ptr_t  pty ;
	ml_pty_hook_t  pty_hook ;

	ml_screen_t *  screen ;
	ml_termcap_ptr_t  termcap ;

	mkf_parser_t *  cc_parser ;	/* char code parser */
	mkf_conv_t *  cc_conv ;		/* char code converter */
	ml_char_encoding_t  encoding ;

	ml_color_t  fg_color ;
	ml_color_t  bg_color ;
	
	mkf_charset_t  cs ;

	ml_xterm_event_listener_t *  xterm_listener ;
	ml_config_event_listener_t *  config_listener ;

	int  log_file ;

	char *  win_name ;
	char *  icon_name ;

	struct
	{
		u_int16_t  top ;
		u_int16_t  left ;
		u_int16_t  bottom ;
		u_int16_t  right ;

	} loc_filter ;

	/* ml_unicode_policy_t */ int8_t  unicode_policy ;

	/* ml_mouse_report_mode_t */ int8_t  mouse_mode ;
	/* ml_extended_mouse_report_mode_t */ int8_t  ext_mouse_mode ;
	/* ml_locator_report_mode_t */ int8_t  locator_mode ;

	/* Used for non iso2022 encoding */
	mkf_charset_t  gl ;
	mkf_charset_t  g0 ;
	mkf_charset_t  g1 ;
	int8_t  is_so ;

	int8_t  is_bold ;
	int8_t  is_italic ;
	int8_t  underline_style ;
	int8_t  is_reversed ;
	int8_t  is_crossed_out ;
	int8_t  is_blinking ;

	int8_t  alt_color_mode ;

	u_int8_t  col_size_of_width_a ;	/* 1 or 2 */

	int8_t  use_char_combining ;
	int8_t  use_multi_col_char ;
	int8_t  logging_vt_seq ;

	int8_t  is_app_keypad ;
	int8_t  is_app_cursor_keys ;
	int8_t  is_app_escape ;
	int8_t  is_bracketed_paste_mode ;
	int8_t  allow_deccolm ;

	int8_t  want_focus_event ;

	int8_t  im_is_active ;

#if  0
	int8_t  modify_cursor_keys ;
	int8_t  modify_function_keys ;
#endif
	int8_t  modify_other_keys ;

#ifdef  USE_VT52
	int8_t  is_vt52_mode ;
#endif

	int8_t  sixel_scrolling ;
	int8_t  cursor_to_right_of_sixel ;
	int8_t  yield ;

	int8_t  is_auto_encoding ;
	int8_t  use_auto_detect ;

	/* for save/restore cursor */
	ml_vt100_storable_states_t  saved_normal ;
	ml_vt100_storable_states_t  saved_alternate ;

	ml_vt100_saved_names_t  saved_win_names ;
	ml_vt100_saved_names_t  saved_icon_names ;

	ml_drcs_t *  drcs ;

	ml_macro_t *  macros ;
	u_int  num_of_macros ;

	u_int32_t *  sixel_palette ;

} ml_vt100_parser_t ;


void  ml_set_use_alt_buffer( int  use) ;

void  ml_set_use_ansi_colors( int  use) ;

void  ml_set_unicode_noconv_areas( char *  areas) ;

void  ml_set_full_width_areas( char *  areas) ;

void  ml_set_use_ttyrec_format( int  use) ;

#ifdef  USE_LIBSSH2
void  ml_set_use_scp_full( int  use) ;
#else
#define  ml_set_use_scp_full(use)  (0)
#endif

void  ml_set_timeout_read_pty( u_long  timeout) ;

void  ml_set_primary_da( char *  da) ;

void  ml_set_secondary_da( char *  da) ;

void  ml_vt100_parser_init(void) ;

void  ml_vt100_parser_final(void) ;

ml_vt100_parser_t *  ml_vt100_parser_new( ml_screen_t *  screen ,
	ml_termcap_ptr_t  termcap , ml_char_encoding_t  encoding ,
	int  is_auto_encoding , int  use_auto_detect , int  logging_vt_seq ,
	ml_unicode_policy_t  policy , u_int  col_size_a ,
	int  use_char_combining , int  use_multi_col_char ,
	const char *  win_name , const char *  icon_name , ml_alt_color_mode_t  alt_color_mode) ;

int  ml_vt100_parser_delete( ml_vt100_parser_t *  vt100_parser) ;

void  ml_vt100_parser_set_pty( ml_vt100_parser_t *  vt100_parser , ml_pty_ptr_t  pty) ;

void  ml_vt100_parser_set_xterm_listener( ml_vt100_parser_t *  vt100_parser ,
	ml_xterm_event_listener_t *  xterm_listener) ;

void  ml_vt100_parser_set_config_listener( ml_vt100_parser_t *  vt100_parser ,
	ml_config_event_listener_t *  config_listener) ;

int  ml_parse_vt100_sequence( ml_vt100_parser_t *  vt100_parser) ;

void  ml_reset_pending_vt100_sequence( ml_vt100_parser_t *  vt100_parser) ;


int  ml_vt100_parser_write_modified_key( ml_vt100_parser_t *  vt100_parser ,
	int  key , int  modcode) ;

int  ml_vt100_parser_write_special_key( ml_vt100_parser_t *  vt100_parser ,
	ml_special_key_t  key , int  modcode , int  is_numlock) ;

/* Must be called in visual context. */
int  ml_vt100_parser_write_loopback( ml_vt100_parser_t *  vt100_parser ,
	const u_char *  buf , size_t  len) ;

/* Must be called in visual context. */
int  ml_vt100_parser_show_message( ml_vt100_parser_t *  vt100_parser , char *  msg) ;

#if  defined(__ANDROID__) || defined(__APPLE__)
/* Must be called in visual context. */
int  ml_vt100_parser_preedit( ml_vt100_parser_t *  vt100_parser ,
	const u_char *  buf , size_t  len) ;
#endif

/* Must be called in visual context. */
int  ml_vt100_parser_local_echo( ml_vt100_parser_t *  vt100_parser ,
	const u_char *  buf , size_t  len) ;

int  ml_vt100_parser_change_encoding( ml_vt100_parser_t *  vt100_parser ,
	ml_char_encoding_t  encoding) ;

#define  ml_vt100_parser_get_encoding( vt100_parser)  ((vt100_parser)->encoding)

size_t  ml_vt100_parser_convert_to( ml_vt100_parser_t *  vt100_parser ,
	u_char *  dst , size_t  len , mkf_parser_t *  parser) ;

int  ml_init_encoding_parser( ml_vt100_parser_t *  vt100_parser) ;

int  ml_init_encoding_conv( ml_vt100_parser_t *  vt100_parser) ;

#define  ml_get_window_name( vt100_parser)  ((vt100_parser)->win_name)

#define  ml_get_icon_name( vt100_parser)  ((vt100_parser)->icon_name)

#define  ml_vt100_parser_set_use_char_combining( vt100_parser , use) \
		((vt100_parser)->use_char_combining = (use))

#define  ml_vt100_parser_is_using_char_combining( vt100_parser) \
		((vt100_parser)->use_char_combining)

#define  ml_vt100_parser_set_use_multi_col_char( vt100_parser , use) \
		((vt100_parser)->use_multi_col_char = (use))

#define  ml_vt100_parser_is_using_multi_col_char( vt100_parser) \
		((vt100_parser)->use_multi_col_char)

#define  ml_vt100_parser_get_mouse_report_mode( vt100_parser)  ((vt100_parser)->mouse_mode)

#define  ml_vt100_parser_is_bracketed_paste_mode( vt100_parser) \
		((vt100_parser)->is_bracketed_paste_mode)

#define  ml_vt100_parser_want_focus_event( vt100_parser)  ((vt100_parser)->want_focus_event)

#define  ml_vt100_parser_set_unicode_policy( vt100_parser , policy) \
		((vt100_parser)->unicode_policy = (policy))

#define  ml_vt100_parser_get_unicode_policy( vt100_parser)  ((vt100_parser)->unicode_policy)

int  ml_set_auto_detect_encodings( char *  encodings) ;

int  ml_convert_to_internal_ch( mkf_char_t *  ch , ml_unicode_policy_t  unicode_policy ,
		mkf_charset_t  gl) ;

#define  ml_vt100_parser_select_drcs( vt100_parser)  ml_drcs_select( (vt100_parser)->drcs)

void  ml_vt100_parser_set_alt_color_mode( ml_vt100_parser_t *  vt100_parser ,
	ml_alt_color_mode_t  mode) ;

#define  ml_vt100_parser_get_alt_color_mode( vt100_parser)  ((vt100_parser)->alt_color_mode)

int  true_or_false( const char *  str) ;

int  ml_vt100_parser_get_config( ml_vt100_parser_t *  vt100_parser , ml_pty_ptr_t  output ,
	char *  key , int  to_menu , int *  flag) ;

int  ml_vt100_parser_set_config( ml_vt100_parser_t *  vt100_parser , char *  key , char *  val) ;

int  ml_vt100_parser_exec_cmd( ml_vt100_parser_t *  vt100_parser , char *  cmd) ;

void  ml_vt100_parser_report_mouse_tracking( ml_vt100_parser_t *  vt100_parser ,
	int  col , int  row , int  button , int  is_released ,
	int  key_state , int  button_state) ;


#endif
