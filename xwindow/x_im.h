/*
 *	$Id$
 */

#ifndef  __X_IM_H__
#define  __X_IM_H__

#include  <kiklib/kik_dlfcn.h>
#include  <X11/Xlib.h>	/* KeySym, XKeyEvent */

#include  "ml_term.h"
#include  "x_im_candidate_screen.h"
#include  "x_im_status_screen.h"

#define X_IM_PREEDIT_NOCURSOR -1

/*
 * the current preedit information
 */
typedef struct x_im_preedit
{
	ml_char_t *  chars ;
	u_int  num_of_chars ;	/* == array size */
	u_int  filled_len ;

	int  segment_offset ;
	int  cursor_offset ;

} x_im_preedit_t ;

typedef struct x_im_event_listener
{
	void *  self ;

	int (*get_spot)( void * , ml_char_t * , int , int * , int *) ;
	u_int (*get_line_height)( void *) ;
	int (*is_vertical)( void *) ;
	int (*draw_preedit_str)( void * , ml_char_t * , u_int , int) ;
	void (*im_changed)( void * , u_char *) ;
	int (*compare_key_state_with_modmap)( void * , u_int , int * , int * ,
					      int * , int * , int * , int * ,
					      int *) ;
	void (*write_to_term)( void * , u_char * , size_t) ;

	x_window_manager_t * (*get_win_man)( void *) ;
	x_font_manager_t * (*get_font_man)( void *) ;
	x_color_manager_t * (*get_color_man)( void *) ;

} x_im_event_listener_t ;

/*
 * a dirty hack to replace -export-dynamic option of libtool
 */
typedef struct x_im_export_syms
{
	int (*ml_str_init)( ml_char_t * , u_int) ;
	int (*ml_str_delete)( ml_char_t * , u_int) ;
	int (*ml_char_combine)( ml_char_t * , u_char * , size_t ,
				mkf_charset_t , int , int , ml_color_t ,
				ml_color_t , int , int) ;
	int (*ml_char_set)( ml_char_t * , u_char * , size_t ,
			    mkf_charset_t  cs , int , int , ml_color_t ,
			    ml_color_t , int , int) ;
	char * (*ml_get_char_encoding_name)( ml_char_encoding_t) ;
	int (*ml_get_char_encoding)( char *) ;
	int (*ml_is_msb_set)( mkf_charset_t  cs) ;
	ml_iscii_keymap_t (*ml_iscii_keymap_new)( int) ;
	int (*ml_iscii_keymap_delete)( ml_iscii_keymap_t) ;
	size_t (*ml_convert_ascii_to_iscii)( ml_iscii_keymap_t , u_char * ,
					     size_t , u_char * , size_t) ;
	mkf_parser_t * (*ml_parser_new)( ml_char_encoding_t) ;
	mkf_conv_t * (*ml_conv_new)( ml_char_encoding_t) ;
	x_im_candidate_screen_t *  (*x_im_candidate_screen_new)(
						x_window_manager_t * ,
						x_font_manager_t * ,
						x_color_manager_t * ,
						int , int , u_int , int , int) ;
	x_im_status_screen_t *  (*x_im_status_screen_new)(
						x_window_manager_t * ,
						x_font_manager_t * ,
						x_color_manager_t * ,
						int , int , int) ;
	int  (*x_term_manager_add_fd)( int , void (*handler)(void)) ;
	int  (*x_term_manager_remove_fd)( int) ;

} x_im_export_syms_t ;

/*
 * input method module object
 */
typedef struct x_im
{
	kik_dl_handle_t  handle ;
	char *  name ;

	x_im_event_listener_t *  listener ;

	x_im_candidate_screen_t *  cand_screen ;
	x_im_status_screen_t *  stat_screen ;

	x_im_preedit_t  preedit ;

	/* methods */
	int (*delete)( struct x_im *) ;
	int (*key_event)( struct x_im * , u_char , KeySym , XKeyEvent *) ;
	int (*switch_mode)( struct x_im *) ;
	void (*focused)( struct x_im *) ;
	void (*unfocused)( struct x_im *) ;

} x_im_t ;


x_im_t *  x_im_new( ml_char_encoding_t  term_encoding ,
		    x_im_event_listener_t *  im_listener ,
		    char *  input_method ,
		    u_int  mod_ignore_mask) ;

void  x_im_delete( x_im_t *  xim) ;

void  x_im_redraw_preedit( x_im_t *  im , int  is_focused) ;

#define  IM_API_VERSION  0x08
#define  IM_API_COMPAT_CHECK_MAGIC			\
	 ((IM_API_VERSION & 0xff << 28) |		\
	 ((sizeof( x_im_t) & 0xff) << 20) |		\
	 ((sizeof( x_im_export_syms_t) & 0xff) << 12) |	\
	 (sizeof( x_im_candidate_screen_t) & 0xfff))

#endif

