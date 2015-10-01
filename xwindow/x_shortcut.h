/*
 *	$Id$
 */

/*
 * This manages short-cut keys of x_screen key events.
 */
 
#ifndef  __X_SHORTCUT_H__
#define  __X_SHORTCUT_H__


#include  "x.h"
#include  <kiklib/kik_types.h>


typedef enum  x_key_func
{
	IM_HOTKEY ,
	EXT_KBD ,
	OPEN_SCREEN ,
	OPEN_PTY ,
	NEXT_PTY ,
	PREV_PTY ,
	HSPLIT_SCREEN ,
	VSPLIT_SCREEN ,
	NEXT_SCREEN ,
	PREV_SCREEN ,
	CLOSE_SCREEN ,
	HEXPAND_SCREEN ,
	VEXPAND_SCREEN ,
	PAGE_UP ,
	PAGE_DOWN ,
	SCROLL_UP ,
	SCROLL_DOWN ,
	INSERT_SELECTION ,
	EXIT_PROGRAM ,
	
	MAX_KEY_MAPS

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

typedef struct  x_shortcut
{
	x_key_t  map[MAX_KEY_MAPS] ;
	x_str_key_t *  str_map ;
	u_int  str_map_size ;
	
} x_shortcut_t ;


int  x_shortcut_init( x_shortcut_t *  shortcut) ;

int  x_shortcut_final( x_shortcut_t *  shortcut) ;

int  x_shortcut_match( x_shortcut_t *  shortcut , x_key_func_t  func , KeySym  sym , u_int  state) ;

char *  x_shortcut_str( x_shortcut_t *  shortcut , KeySym  sym , u_int  state) ;

int  x_shortcut_parse( x_shortcut_t *  shortcut , char *  key , char *  oper) ;


#endif
