/*
 *	$Id$
 */

#ifndef  __X_SB_VIEW_FACTORY_H__
#define  __X_SB_VIEW_FACTORY_H__


#include  "x_sb_view.h"

typedef  x_sb_view_t * (*x_sb_view_new_func_t)(void) ;
typedef  x_sb_view_t * (*x_sb_engine_new_func_t)( x_sb_view_conf_t *  conf, int is_transparent) ;

x_sb_view_t *  x_sb_view_new( char *  name) ;

x_sb_view_t *  x_transparent_scrollbar_view_new( char *  name) ;

int  x_unload_scrollbar_view_lib( char *  name) ;


#endif
