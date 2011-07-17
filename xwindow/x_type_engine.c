/*
 *	$Id$
 */

#include  "x_type_engine.h"

#include  <string.h>		/* strcmp */


/* --- static variables --- */

/* Order of this table must be same as x_type_engine_t. */
static char *   type_engine_name_table[] =
{
	"xcore" , "xft" , "cairo" ,
} ;


/* --- global functions --- */

x_type_engine_t
x_get_type_engine_by_name(
	char *  name
	)
{
#ifdef  USE_TYPE_XCORE
	if( strcmp( "xcore" , name) == 0)
	{
		return  TYPE_XCORE ;
	}
#endif
#ifdef  USE_TYPE_XFT
	if( strcmp( "xft" , name) == 0)
	{
		return  TYPE_XFT ;
	}
#endif
#ifdef  USE_TYPE_CAIRO
	if( strcmp( "cairo" , name) == 0)
	{
		return  TYPE_CAIRO ;
	}
#endif

#if  defined(USE_TYPE_XCORE)
	return  TYPE_XCORE ;
#elif  defined(USE_TYPE_XFT)
	return  TYPE_XFT ;
#else
	return  TYPE_CAIRO ;
#endif
}

char *
x_get_type_engine_name(
	x_type_engine_t  engine
	)
{
	if( engine < 0 || TYPE_ENGINE_MAX <= engine)
	{
	#if  defined(USE_TYPE_XCORE)
		/* default value */
		engine = TYPE_XCORE ;
	#elif  defined(USE_TYPE_XFT)
		engine = TYPE_XFT ;
	#else
		engine = TYPE_CAIRO ;
	#endif
	}

	return  type_engine_name_table[engine] ;
}
