/*
 *	$Id$
 */

#ifndef  __DAEMON_H__
#define  __DAEMON_H__


#include  <kiklib/kik_config.h>	/* USE_WIN32API */


#ifndef  USE_WIN32API

int  daemon_init(void) ;

int  daemon_final(void) ;

int  daemon_get_fd(void) ;

#else

#define  daemon_init()  (0)
#define  daemon_final()  (0)
#define  daemon_get_fd()  (-1)

#endif


#endif
