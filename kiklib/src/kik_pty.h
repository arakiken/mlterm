/*
 *	$Id$
 */

#ifndef  __KIK_PTY_H__
#define  __KIK_PTY_H__


#include  "kik_types.h"		/* pid_t */


pid_t  kik_pty_fork( int *  master , int *  slave) ;

int  kik_pty_helper_close( int  pty) ;

void  kik_pty_helper_set_flag( int  lastlog , int  utmp , int  wtmp) ;


#endif
