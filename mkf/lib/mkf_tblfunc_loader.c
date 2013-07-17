/*
 *	$Id$
 */

#include  "mkf_tblfunc_loader.h"


#ifndef  NO_DYNAMIC_LOAD_TABLE


#include  <stdio.h>	/* NULL */


/* --- global functions --- */

void *
mkf_load_8bits_func(
	const char *  symname
	)
{
	static kik_dl_handle_t   handle ;
	static int  is_tried ;

	if( ! is_tried)
	{
		is_tried = 1 ;

		if( ! ( handle = kik_dl_open( MKFLIB_DIR, "mkf_8bits")) &&
		    ! ( handle = kik_dl_open( "", "mkf_8bits")))
		{
			return  NULL ;
		}

		kik_dl_close_at_exit( handle) ;
	}

	if( handle)
	{
		return  kik_dl_func_symbol( handle , symname) ;
	}
	else
	{
		return  NULL ;
	}
}

void *
mkf_load_jajp_func(
	const char *  symname
	)
{
	static kik_dl_handle_t   handle ;
	static int  is_tried ;

	if( ! is_tried)
	{
		is_tried = 1 ;

		if( ! ( handle = kik_dl_open( MKFLIB_DIR, "mkf_jajp")) &&
		    ! ( handle = kik_dl_open( "", "mkf_jajp")))
		{
			return  NULL ;
		}

		kik_dl_close_at_exit( handle) ;
	}

	if( handle)
	{
		return  kik_dl_func_symbol( handle , symname) ;
	}
	else
	{
		return  NULL ;
	}
}

void *
mkf_load_kokr_func(
	const char *  symname
	)
{
	static kik_dl_handle_t   handle ;
	static int  is_tried ;

	if( ! is_tried)
	{
		is_tried = 1 ;

		if( ! ( handle = kik_dl_open( MKFLIB_DIR, "mkf_kokr")) &&
		    ! ( handle = kik_dl_open( "", "mkf_kokr")))
		{
			return  NULL ;
		}

		kik_dl_close_at_exit( handle) ;
	}

	if( handle)
	{
		return  kik_dl_func_symbol( handle , symname) ;
	}
	else
	{
		return  NULL ;
	}
}

void *
mkf_load_zh_func(
	const char *  symname
	)
{
	static kik_dl_handle_t   handle ;
	static int  is_tried ;

	if( ! is_tried)
	{
		is_tried = 1 ;

		if( ! ( handle = kik_dl_open( MKFLIB_DIR, "mkf_zh")) &&
		    ! ( handle = kik_dl_open( "", "mkf_zh")))
		{
			return  NULL ;
		}

		kik_dl_close_at_exit( handle) ;
	}

	if( handle)
	{
		return  kik_dl_func_symbol( handle , symname) ;
	}
	else
	{
		return  NULL ;
	}
}

#endif	/* NO_DYNAMIC_LOAD_TABLE */
