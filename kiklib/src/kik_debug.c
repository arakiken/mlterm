/*
 *	update: <2001/11/26(02:37:24)>
 *	$Id$
 */

#include  "kik_debug.h"

#include  <stdio.h>
#include  <stdarg.h>

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

	return  vfprintf( stderr , new_format , arg_list) ;
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

	return  vfprintf( stderr , new_format , arg_list) ;
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

	return  vfprintf( stderr , new_format , arg_list) ;
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

	va_start( arg_list , format) ;

	return  vfprintf( stderr , format , arg_list) ;
}
