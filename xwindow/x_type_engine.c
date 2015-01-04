/*
 *	$Id$
 */

#include  "x_type_engine.h"

#include  <string.h>		/* strcmp */
#include  <kiklib/kik_types.h>	/* u_int */


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
	if( strcmp( "xcore" , name) == 0)
	{
		return  TYPE_XCORE ;
	}
	else if( strcmp( "xft" , name) == 0)
	{
		return  TYPE_XFT ;
	}
	else if( strcmp( "cairo" , name) == 0)
	{
		return  TYPE_CAIRO ;
	}

	/* default value */
	return  TYPE_XCORE ;
}

char *
x_get_type_engine_name(
	x_type_engine_t  engine
	)
{
	if( (u_int)engine >= TYPE_ENGINE_MAX)
	{
		/* default value */
		engine = TYPE_XCORE ;
	}

	return  type_engine_name_table[engine] ;
}
