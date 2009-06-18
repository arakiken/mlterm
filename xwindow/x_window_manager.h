/*
 *	$Id$
 */

#ifndef  __X_WINDOW_MANAGER_H__
#define  __X_WINDOW_MANAGER_H__


#include  <kiklib/kik_types.h>		/* u_int */

#include  "x_window.h"


typedef struct  x_modifier_mapping
{
	u_long  serial ;
	XModifierKeymap *  map ;

} x_modifier_mapping_t ;

typedef struct  x_window_manager
{
	Display *  display ;
	int  screen ;		/* DefaultScreen */
	Window  my_window ;	/* DefaultRootWindow */
	x_gc_t *  gc ;

	Window group_leader ;
	char *icon_path;
	Pixmap  icon ;
	Pixmap  mask ;
	u_int32_t *  cardinal ;

	x_window_t **  roots ;
	u_int  num_of_roots ;

	x_window_t *  selection_owner ;
	x_modifier_mapping_t  modmap ;

} x_window_manager_t ;


int  x_window_manager_init( x_window_manager_t *  win_man , Display *  display) ;

int  x_window_manager_final( x_window_manager_t *  win_man) ;

int  x_window_manager_show_root( x_window_manager_t *  win_man , x_window_t *  root ,
	int  x , int  y , int  hint , char *  app_name) ;

int  x_window_manager_remove_root( x_window_manager_t *  win_man , x_window_t *  root) ;

void  x_window_manager_idling( x_window_manager_t *  win_man) ;

int  x_window_manager_receive_next_event( x_window_manager_t *  win_man) ;


/*
 * Folloing functions called from x_window.c
 */

int  x_window_manager_own_selection( x_window_manager_t *  win_man , x_window_t *  win) ;

int  x_window_manager_clear_selection( x_window_manager_t *  win_man , x_window_t *  win) ;

XID  x_window_manager_get_group( x_window_manager_t *  win_man) ;

XModifierKeymap *  x_window_manager_get_modifier_mapping( x_window_manager_t *  win_man) ;

void  x_window_manager_update_modifier_mapping( x_window_manager_t *  win_man ,	u_int  serial) ;

int x_window_manager_set_icon( x_window_t *  win , char *  icon_path) ;


#endif
