/*
 *	$Id$
 */

#ifndef  __KIK_TIME_H__
#define  __KIK_TIME_H__


#include  <time.h>


time_t  kik_time_string_date_to_time_t( const char *  format , const char *  date) ;

struct tm *  kik_time_string_date_to_tm( struct tm *  tm_info , const char *  format , const char *  date) ;

int  kik_time_string_wday_to_int( const char *  wday) ;

char *  kik_time_int_wday_to_string( int  wday) ;

char *  kik_time_int_wday_to_abbrev_string( int  wday) ;

int  kik_time_string_month_to_int( const char *  month) ;

char *  kik_time_int_month_to_string( int  month) ;

char *  kik_time_int_month_to_abbrev_string( int  month) ;


#endif
