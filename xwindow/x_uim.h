/*
 *	$Id$
 */

#ifndef  __X_UIM_H__
#define  __X_UIM_H__

#include  <uim/uim.h>
#include  <uim/uim-helper.h>

#include  "x_window.h"

/* #define  UIM_DEBUG */

typedef struct  x_uim_event_listener
{
	void *  self ;

	int  (*get_spot)( void * , ml_char_t * , u_int , int * , int *) ;
	int  (*draw_preedit_str)( void * , ml_char_t * , u_int , int) ;
	void (*write_to_term)( void * , u_char * , size_t) ;

} x_uim_event_listener_t ;

typedef struct candwin_helper {
	int  pid ;
	FILE *  pipe_r ;
	FILE *  pipe_w ;
	int  x ;
	int  y ;
	int  is_visible ;
} candwin_helper_t ;

typedef struct  x_uim
{
	uim_context  context ;
	char *  encoding ;	/* uim encoding */

	mkf_parser_t *  parser ;	/* for string recieved from uim */
	mkf_conv_t *  conv ;		/* to convert tty encoding from uim encoding */

	struct
	{
		ml_char_t *  chars ;
		u_int  num_of_chars ;
		int  segment_offset ;
		int  cursor_position ;
	} preedit ;

	x_uim_event_listener_t *  listener ;

	candwin_helper_t  candwin ;

} x_uim_t ;

int x_uim_init( int  use_uim) ;

x_uim_t *  x_uim_new( ml_char_encoding_t  term_encoding , x_uim_event_listener_t *  uim_listener , char *  engine) ;

void  x_uim_delete( x_uim_t *  uim) ;

int  x_uim_filter_key_event( x_uim_t *  uim , KeySym  ksym , XKeyEvent *  event) ;

void x_uim_focused( x_uim_t *  uim) ;

void x_uim_unfocused( x_uim_t *  uim) ;

void x_uim_redraw_preedit( x_uim_t *  uim) ;

int x_uim_get_helper_fd( void) ;

void x_uim_parse_helper_messege(void) ;

#endif

