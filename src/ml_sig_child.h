/*
 *	$Id$
 */

#ifndef  __ML_SIG_CHILD_H__
#define  __ML_SIG_CHILD_H__


#include  <kiklib/kik_types.h>	/* pid_t */


int  ml_sig_child_init(void) ;

int  ml_sig_child_final(void) ;

int  ml_add_sig_child_listener( void *  self , void (*exited)( void * , pid_t)) ;

int  ml_remove_sig_child_listener( void *  self) ;


#endif
