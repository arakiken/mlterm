/*
 *	$Id$
 */

#ifndef  __X_SCREEN_MANAGER_H__
#define  __X_SCREEN_MANAGER_H__


#include  <stdio.h>	/* FILE */
#include  "x_screen.h"
#include  "x_main_config.h"


int  x_screen_manager_init( char *  mlterm_version , u_int  depth ,
	u_int  max_screens_multiple , u_int  num_of_startup_screens ,
	x_main_config_t *  main_config) ;

int  x_screen_manager_final(void) ;

u_int  x_screen_manager_startup(void) ;

int  x_close_dead_screens(void) ;

u_int  x_get_all_screens( x_screen_t ***  _screens) ;

int  x_mlclient( char *  args , FILE *  fp) ;


#endif
