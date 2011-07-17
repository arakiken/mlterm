/*
 *	$Id$
 */

#ifndef  __X_TYPE_ENGINE_H__
#define  __X_TYPE_ENGINE_H__


typedef enum x_type_engine
{
	TYPE_XCORE ,
	TYPE_XFT ,
	TYPE_CAIRO ,

	TYPE_ENGINE_MAX

} x_type_engine_t ;


x_type_engine_t  x_get_type_engine_by_name( char *  name) ;

char *  x_get_type_engine_name( x_type_engine_t  mode) ;


#endif
