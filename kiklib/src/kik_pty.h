/*
 *	$Id$
 */

#ifndef  __KIK_PTY_H__
#define  __KIK_PTY_H__


#include  "kik_types.h"		/* pid_t */


pid_t  kik_pty_fork( int *  master , int *  slave , char **  slave_name) ;


#endif
