/*
 *	$Id$
 */

/*
 * ANTI_ALIAS macro should be concerned.
 */

#ifndef  __X_TERM_MANAGER_H__
#define  __X_TERM_MANAGER_H__


int  x_term_manager_init( int  argc , char **  argv) ;

int  x_term_manager_final(void) ;

void  x_term_manager_event_loop(void) ;


#endif
