/*
 *	$Id$
 */

#ifndef  __X_TERMCAP_H__
#define  __X_TERMCAP_H__


#include  <kiklib/kik_types.h>
#include  <ml_termcap.h>


typedef struct  x_termcap
{
	ml_termcap_t *  entries ;
	u_int  num_of_entries ;

} x_termcap_t ;


int  x_termcap_init( x_termcap_t *  termcap) ;

int  x_termcap_final( x_termcap_t *  termcap) ;

int  x_termcap_read_conf( x_termcap_t *  termcap , char *  filename) ;

ml_termcap_t *  x_termcap_get_entry( x_termcap_t *  termcap , char *  name) ;


#endif
