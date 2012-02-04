/*
 *	$Id$
 */

#ifndef  __X_EVENT_SOURCE_H__
#define  __X_EVENT_SOURCE_H__


int  x_event_source_init(void) ;

int  x_event_source_final(void) ;

int  x_event_source_process(void) ;

int  x_event_source_add_fd( int  fd , void  (*handler)(void)) ;

int  x_event_source_remove_fd( int  fd) ;


#endif
