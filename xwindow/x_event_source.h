/*
 *	$Id$
 */

#ifndef  __X_EVENT_SOURCE_H__
#define  __X_EVENT_SOURCE_H__


int  x_event_source_init(void) ;

int  x_event_source_final(void) ;

int  x_event_source_process(void) ;

#ifdef  USE_WIN32API

/* for x_im_export_syms_t in x_im.c */
#define x_event_source_add_fd  (NULL)
#define x_event_source_remove_fd  (NULL)

#else

int  x_event_source_add_fd( int  fd , void  (*handler)(void)) ;

int  x_event_source_remove_fd( int  fd) ;
#endif


#endif
