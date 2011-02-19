/*
 *	$Id$
 */

#include  "kik_debug.h"

#include  <stdio.h>
#include  <stdarg.h>
#include  <string.h>    /* strlen */

#include  "kik_mem.h"	/* alloca */


#if  0
#define  __DEBUG
#endif


/* --- global functions --- */

/*
 * this is usually used between #ifdef __DEBUG ... #endif
 */
int
kik_debug_printf(
	const char *  format ,
	...
	)
{
	va_list  arg_list ;
	char  prefix[] = "DEBUG: " ;
	char *  new_format = NULL ;
	int  ret ;

	va_start( arg_list , format) ;

	if( ( new_format = alloca( sizeof( prefix) + strlen( format) + 1)) == NULL)
	{
		/* error */

		return  0 ;
	}
	else
	{
		sprintf( new_format , "%s%s" , prefix , format) ;
	}

	ret = vfprintf( stderr , new_format , arg_list) ;
#ifdef  USE_WIN32API
	fflush(stderr) ;
#endif

	return  ret ;
}

/*
 * this is usually used between #ifdef DEBUG ... #endif
 */
int
kik_warn_printf(
	const char *  format ,
	...
	)
{
	va_list  arg_list ;
	char  prefix[] = "WARN: " ;
	char *  new_format = NULL ;
	int  ret ;

	va_start( arg_list , format) ;

	if( ( new_format = alloca( sizeof( prefix) + strlen( format) + 1)) == NULL)
	{
		/* error */

		return  0 ;
	}
	else
	{
		sprintf( new_format , "%s%s" , prefix , format) ;
	}

	ret = vfprintf( stderr , new_format , arg_list) ;
#ifdef  USE_WIN32API
	fflush(stderr) ;
#endif

	return  ret ;

}

/*
 * this is usually used without #ifdef ... #endif
 */
int
kik_error_printf(
	const char *  format ,
	...
	)
{
	va_list  arg_list ;
	char  prefix[] = "*** ERROR HAPPEND ***  " ;
	char *  new_format = NULL ;
	int  ret ;

	va_start( arg_list , format) ;

	if( ( new_format = alloca( sizeof( prefix) + strlen( format) + 1)) == NULL)
	{
		/* error */

		return  0 ;
	}
	else
	{
		sprintf( new_format , "%s%s" , prefix , format) ;
	}

	ret = vfprintf( stderr , new_format , arg_list) ;
#ifdef  USE_WIN32API
	fflush(stderr) ;
#endif

	return  ret ;
}

/*
 * for noticing message.
 */
int
kik_msg_printf(
	const char *  format ,
	...
	)
{
	va_list  arg_list ;
	int  ret ;

	va_start( arg_list , format) ;

	ret = vfprintf( stderr , format , arg_list) ;
#ifdef  USE_WIN32API
	fflush(stderr) ;
#endif

	return  ret ;
}
