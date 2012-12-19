/*
 *	$Id$
 */

#ifndef  __ML_CONFIG_MENU_H__
#define  __ML_CONFIG_MENU_H__


#include  <kiklib/kik_types.h>		/* pid_t */

#include  "ml_pty.h"


typedef struct  ml_config_menu
{
	/* These members are regarded as HANDLE in win32. */
	pid_t  pid ;
	int  fd ;

#ifdef  USE_LIBSSH2
	ml_pty_ptr_t  pty ;
#endif

} ml_config_menu_t ;


#ifdef  NO_TOOLS

#define  ml_config_menu_init( config_menu)  (1)
#define  ml_config_menu_final( config_menu)  (0)
#define  ml_config_menu_start( config_menu , cmd_path , x , y , display , pty)  (0)
#define  ml_config_menu_write( config_menu , buf , len)  (0)

#else	/* NO_TOOLS */

int  ml_config_menu_init( ml_config_menu_t *  config_menu) ;

int  ml_config_menu_final( ml_config_menu_t *  config_menu) ;

int  ml_config_menu_start( ml_config_menu_t *  config_menu , char *  cmd_path ,
	int  x , int  y , char *  display , ml_pty_ptr_t  pty) ;

int  ml_config_menu_write( ml_config_menu_t *  config_menu , u_char *  buf , size_t  len) ;

#endif	/* NO_TOOLS */


#endif
