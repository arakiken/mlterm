/*
 *	$Id$
 */

#ifndef  __ML_CONFIG_MENU_H__
#define  __ML_CONFIG_MENU_H__


#include  <kiklib/kik_types.h>		/* pid_t */


typedef struct  ml_config_menu
{	
	pid_t  pid ;
	int  fd ;
	
} ml_config_menu_t ;


int  ml_config_menu_init( ml_config_menu_t *  config_menu) ;

int  ml_config_menu_final( ml_config_menu_t *  config_menu) ;

int  ml_config_menu_start( ml_config_menu_t *  config_menu , char *  cmd_path ,
	int  x , int  y , char *  display , int  fd) ;

int  ml_config_menu_write( ml_config_menu_t *  config_menu , u_char *  buf , size_t  len) ;


#endif
