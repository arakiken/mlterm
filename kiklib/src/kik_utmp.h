/*
 *	$Id$
 */

#ifndef  __KIK_UTMP_BSD_H__
#define  __KIK_UTMP_BSD_H__


typedef struct kik_utmp *  kik_utmp_t ;


kik_utmp_t  kik_utmp_new( char *  tty , char *  host) ;

int  kik_utmp_delete( kik_utmp_t  utmp) ;


#endif
