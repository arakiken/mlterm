/*
 *	$Id$
 */

/*
 * this manages short-cut keys of ml_term_screen key events.
 */
 
#ifndef  __ML_KEYMAP_H__
#define  __ML_KEYMAP_H__


#include  <X11/Xlib.h>
#include  <kiklib/kik_types.h>


typedef enum  ml_key_func
{
	XIM_OPEN ,
	XIM_CLOSE ,
	NEW_PTY ,
	PAGE_UP ,
	SCROLL_UP ,
	INSERT_SELECTION ,
	EXIT_PROGRAM ,
	
	MAX_KEY_MAPS ,

} ml_key_func_t ;

typedef struct  ml_key
{
	KeySym  ksym ;
	u_int  state ;
	int  is_used ;
	
} ml_key_t ;

typedef struct  ml_keymap
{
	ml_key_t  map[MAX_KEY_MAPS] ;
	
} ml_keymap_t ;


int  ml_keymap_init( ml_keymap_t *  keymap) ;

int  ml_keymap_final( ml_keymap_t *  keymap) ;

int  ml_keymap_read_conf( ml_keymap_t *  keymap , char *  filename) ;

int  ml_keymap_match( ml_keymap_t *  keymap , ml_key_func_t  func , KeySym  sym , u_int  state) ;


#endif
