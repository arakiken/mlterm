/*
 *	$Id$
 */

#ifndef  __ML_PICTURE_H__
#define  __ML_PICTURE_H__


#include  <X11/Xlib.h>


#include  "ml_window.h"


typedef struct  ml_picture
{
	ml_window_t *  win ;
	Pixmap  pixmap ;
	
} ml_picture_t ;


int  ml_picture_init( ml_picture_t *  pic , ml_window_t *  win) ;

int  ml_picture_final( ml_picture_t *  pic) ;

int  ml_picture_load( ml_picture_t *  pic , char *  file_path) ;


#endif
