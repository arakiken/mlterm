/*
 *	$Id$
 */

#ifndef  __ML_PICTURE_H__
#define  __ML_PICTURE_H__


#include  <X11/Xlib.h>
#include  <kiklib/kik_types.h>		/* u_int8_t */


/* stub */
typedef struct ml_window *  ml_window_ptr_t ;

typedef struct ml_picture_modifier
{
	u_int8_t  brightness ;
	
} ml_picture_modifier_t ;

typedef struct ml_picture
{
	ml_window_ptr_t  win ;
	Pixmap  pixmap ;
	ml_picture_modifier_t *  mod ;
	
} ml_picture_t ;


int  ml_picture_display_opened( Display *  display) ;

int  ml_picture_display_closed( Display *  display) ;

int  ml_picture_init( ml_picture_t *  pic , ml_window_ptr_t  win , ml_picture_modifier_t *  mod) ;

int  ml_picture_final( ml_picture_t *  pic) ;

int  ml_picture_load_file( ml_picture_t *  pic , char *  file_path) ;

int  ml_picture_load_background( ml_picture_t *  pic) ;


#endif
