/*
 *	$Id$
 */

#include  "x_type_loader.h"


#ifndef  NO_DYNAMIC_LOAD_TYPE

#include  <stdio.h>		/* NULL */
#include  <kiklib/kik_dlfcn.h>
#include  <kiklib/kik_debug.h>


#ifndef  LIBDIR
#define  TYPELIB_DIR  "/usr/local/lib/mlterm/"
#else
#define  TYPELIB_DIR  LIBDIR "/mlterm/"
#endif


/* --- global functions --- */

void *
x_load_type_xft_func(
	x_type_id_t  id
	)
{
	static void **  func_table ;
	static int  is_tried ;

	if( ! is_tried)
	{
		kik_dl_handle_t  handle ;

		is_tried = 1 ;

		if( ( ! ( handle = kik_dl_open( TYPELIB_DIR , "type_xft")) &&
		      ! ( handle = kik_dl_open( "" , "type_xft"))))
		{
			kik_error_printf( "xft: Could not load.\n") ;

			return  NULL ;
		}

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Loading libtype_xft.so\n") ;
	#endif

		kik_dl_close_at_exit( handle) ;

		func_table = kik_dl_func_symbol( handle , "x_type_xft_func_table") ;

		if( (u_int32_t)func_table[TYPE_API_COMPAT_CHECK] != TYPE_API_COMPAT_CHECK_MAGIC)
		{
			kik_dl_close( handle) ;
			func_table = NULL ;
			kik_error_printf( "Incompatible type engine API.\n") ;

			return  NULL ;
		}
	}

	if( func_table)
	{
		return  func_table[id] ;
	}
	else
	{
		return  NULL ;
	}
}

void *
x_load_type_cairo_func(
	x_type_id_t  id
	)
{
	static void **  func_table ;
	static int  is_tried ;

	if( ! is_tried)
	{
		kik_dl_handle_t  handle ;

		is_tried = 1 ;

		if( ( ! ( handle = kik_dl_open( TYPELIB_DIR , "type_cairo")) &&
		      ! ( handle = kik_dl_open( "" , "type_cairo"))))
		{
			kik_error_printf( "cairo: Could not load.\n") ;

			return  NULL ;
		}

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Loading libtype_cairo.so\n") ;
	#endif

		kik_dl_close_at_exit( handle) ;

		func_table = kik_dl_func_symbol( handle , "x_type_cairo_func_table") ;

		if( (u_int32_t)func_table[TYPE_API_COMPAT_CHECK] != TYPE_API_COMPAT_CHECK_MAGIC)
		{
			kik_dl_close( handle) ;
			func_table = NULL ;
			kik_error_printf( "Incompatible type engine API.\n") ;

			return  NULL ;
		}
	}

	if( func_table)
	{
		return  func_table[id] ;
	}
	else
	{
		return  NULL ;
	}
}

#endif	/* NO_DYNAMIC_LOAD_TYPE */
