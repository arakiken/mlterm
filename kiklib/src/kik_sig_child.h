/*
 *	$Id$
 */

#ifndef  __KIK_SIG_CHILD_H__
#define  __KIK_SIG_CHILD_H__


#include  "kik_types.h"	/* pid_t */


int  kik_sig_child_init(void) ;

int  kik_sig_child_final(void) ;

int  kik_sig_child_suspend(void) ;

int  kik_sig_child_resume(void) ;

int  kik_add_sig_child_listener( void *  self , void (*exited)( void * , pid_t)) ;

int  kik_remove_sig_child_listener( void *  self , void (*exited)( void * , pid_t)) ;


#endif
