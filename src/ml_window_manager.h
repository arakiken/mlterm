/*
 *	update: <2001/11/3(13:13:49)>
 *	$Id$
 */

#ifndef  __ML_WINDOW_MANAGER_H__
#define  __ML_WINDOW_MANAGER_H__


#include  "ml_window.h"


typedef struct  ml_window_manager
{
	Display *  display ;
	int  screen ;
	Window  my_window ;

	struct
	{
		ml_window_t *  root ;
	
		int  x ;
		int  y ;
		
	} roots[5] ;

	u_int  num_of_roots ;

	ml_window_t *  selection_owner ;
	
} ml_window_manager_t ;


int  ml_window_manager_init( ml_window_manager_t *  win_man , char *  disp_name) ;

int  ml_window_manager_final( ml_window_manager_t *  win_man) ;

int  ml_window_manager_show_root( ml_window_manager_t *  win_man , ml_window_t *  root , int  x , int  y) ;

int  ml_window_manager_remove_root( ml_window_manager_t *  win_man , ml_window_t *  root) ;

int  ml_window_manager_own_selection( ml_window_manager_t *  win_man , ml_window_t *  win) ;

int  ml_window_manager_clear_selection( ml_window_manager_t *  win_man , ml_window_t *  win) ;

void  ml_window_manager_idling( ml_window_manager_t *  win_man) ;

int  ml_window_manager_receive_next_event( ml_window_manager_t *  win_man) ;


#endif
