/*
 *	$Id$
 */

#ifndef  __X_WINDOW_MANAGER_H__
#define  __X_WINDOW_MANAGER_H__


#include  <kiklib/kik_types.h>		/* u_int */

#include  "x_window.h"


typedef struct  x_window_manager
{
	Display *  display ;
	int  screen ;
	Window  my_window ;

	Window group_leader ;
	Pixmap  icon ;
	Pixmap  mask ;
	u_int32_t *  cardinal ;

	x_window_t **  roots ;
	u_int  num_of_roots ;

	x_window_t *  selection_owner ;
	
} x_window_manager_t ;


int  x_window_manager_init( x_window_manager_t *  win_man , Display *  display) ;

int  x_window_manager_final( x_window_manager_t *  win_man) ;

int  x_window_manager_show_root( x_window_manager_t *  win_man , x_window_t *  root ,
	int  x , int  y , int  hint , char *  app_name) ;

int  x_window_manager_remove_root( x_window_manager_t *  win_man , x_window_t *  root) ;

int  x_window_manager_own_selection( x_window_manager_t *  win_man , x_window_t *  win) ;

int  x_window_manager_clear_selection( x_window_manager_t *  win_man , x_window_t *  win) ;

void  x_window_manager_idling( x_window_manager_t *  win_man) ;

int  x_window_manager_receive_next_event( x_window_manager_t *  win_man) ;

XID  x_window_manager_get_group( x_window_manager_t *  win_man) ;


#endif
