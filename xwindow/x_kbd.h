/*
 *	$Id$
 */

#ifndef  __X_KBD_H__
#define  __X_KBD_H__


#include  <mkf/mkf_parser.h>
#include  <ml_iscii.h>

#include  "x_window.h"


typedef enum x_kbd_type
{
	KBD_ARABIC ,
	KBD_ISCII_INSCRIPT ,
	KBD_ISCII_PHONETIC ,

} x_kbd_type_t ;

typedef struct x_kbd
{
	x_kbd_type_t  type ;
	
	x_window_t *  window ;
	
	mkf_parser_t *  parser ;

	int (*delete)( struct x_kbd *  kbd) ;
	size_t (*get_str)( struct x_kbd * , u_char * , size_t , mkf_parser_t ** ,
		KeySym * , XKeyEvent *) ;

} x_kbd_t ;


x_kbd_t *  x_arabic_kbd_new( x_window_t *  win) ;

x_kbd_t *  x_iscii_phonetic_kbd_new( x_window_t *  win) ;

x_kbd_t *  x_iscii_inscript_kbd_new( x_window_t *  win) ;

#if  0

x_kbd_t *  x_canna_kbd_new( x_window_t *  win) ;

#endif


#endif
