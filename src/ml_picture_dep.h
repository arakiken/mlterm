/*
 *	$Id$
 */

#ifndef  __ML_PICTURE_DEP_H__
#define  __ML_PICTURE_DEP_H__


#include  "ml_window.h"
#include  "ml_picture.h"


int  ml_picdep_display_opened( Display *  disp) ;

int  ml_picdep_display_closed( Display *  disp) ;

int  ml_picdep_root_pixmap_available( Display *  disp) ;

Pixmap  ml_picdep_load_file( ml_window_t *  win , char *  file_path , ml_picture_modifier_t *  pic_mod) ;

Pixmap  ml_picdep_load_background( ml_window_t *  win , ml_picture_modifier_t *  pic_mod) ;


#endif
