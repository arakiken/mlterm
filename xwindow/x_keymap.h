/*
 *	$Id$
 */

/*
 * this manages short-cut keys of x_screen key events.
 */
 
#ifndef  __X_KEYMAP_H__
#define  __X_KEYMAP_H__


#include  <X11/Xlib.h>
#include  <kiklib/kik_types.h>


typedef enum  x_key_func
{
	XIM_OPEN ,
	XIM_CLOSE ,
	NEW_PTY ,
	PAGE_UP ,
	PAGE_DOWN ,
	SCROLL_UP ,
	SCROLL_DOWN ,
	INSERT_SELECTION ,
	EXIT_PROGRAM ,
	
	MAX_KEY_MAPS ,

} x_key_func_t ;

typedef struct  x_key
{
	KeySym  ksym ;
	u_int  state ;
	int  is_used ;
	
} x_key_t ;

typedef struct  x_str_key
{
	KeySym  ksym ;
	u_int  state ;
	char *  str ;

} x_str_key_t ;

typedef struct  x_keymap
{
	x_key_t  map[MAX_KEY_MAPS] ;
	x_str_key_t *  str_map ;
	u_int  str_map_size ;
	
} x_keymap_t ;


int  x_keymap_init( x_keymap_t *  keymap) ;

int  x_keymap_final( x_keymap_t *  keymap) ;

int  x_keymap_read_conf( x_keymap_t *  keymap , char *  filename) ;

int  x_keymap_match( x_keymap_t *  keymap , x_key_func_t  func , KeySym  sym , u_int  state) ;

char *  x_keymap_str( x_keymap_t *  keymap , KeySym  sym , u_int  state) ;


#endif
