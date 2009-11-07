/*
 *	$Id$
 */

#ifndef  __X_DISPLAY_H__
#define  __X_DISPLAY_H__


#include  <kiklib/kik_types.h>		/* u_int */

#include  "x.h"
#include  "x_gc.h"


/* Defined in x_window.h */
typedef struct  x_window *  x_window_ptr_t ;

typedef struct  x_modifier_mapping
{
	u_long  serial ;
	XModifierKeymap *  map ;

} x_modifier_mapping_t ;

typedef struct  x_display
{
	/*
	 * Public(read only)
	 */
	Display *  display ;
	int  screen ;		/* DefaultScreen */

	/*
	 * Private
	 */
	char *  name ;
	Window  my_window ;	/* DefaultRootWindow */
	x_gc_t *  gc ;

	Window group_leader ;
	char *  icon_path ;
	Pixmap  icon ;
	Pixmap  mask ;
	u_int32_t *  cardinal ;

	x_window_ptr_t *  roots ;
	u_int  num_of_roots ;

	x_window_ptr_t  selection_owner ;

	x_modifier_mapping_t  modmap ;

#ifndef  USE_WIN32GUI
	Cursor  cursors[3] ;
#endif

} x_display_t ;


#ifdef  USE_WIN32GUI
int  x_display_set_hinstance( HINSTANCE  hinst) ;
#endif

x_display_t *  x_display_open( char *  name) ;

int  x_display_close( x_display_t *  disp) ;

int  x_display_close_all(void) ;

x_display_t **  x_get_opened_displays( u_int *  num) ;

int  x_display_fd( x_display_t *  disp) ;

int  x_display_show_root( x_display_t *  disp , x_window_ptr_t  root ,
	int  x , int  y , int  hint , char *  app_name) ;

int  x_display_remove_root( x_display_t *  disp , x_window_ptr_t  root) ;

void  x_display_idling( x_display_t *  disp) ;

int  x_display_receive_next_event( x_display_t *  disp) ;


/*
 * Folloing functions called from x_window.c
 */

int  x_display_own_selection( x_display_t *  disp , x_window_ptr_t  win) ;

int  x_display_clear_selection( x_display_t *  disp , x_window_ptr_t  win) ;

XID  x_display_get_group( x_display_t *  disp) ;

XModifierKeymap *  x_display_get_modifier_mapping( x_display_t *  disp) ;

#ifndef  USE_WIN32GUI
Cursor  x_display_get_cursor( x_display_t *  disp , u_int  shape) ;
#endif

void  x_display_update_modifier_mapping( x_display_t *  disp ,	u_int  serial) ;

int x_display_set_icon( x_window_ptr_t  win , char *  icon_path) ;


#endif
