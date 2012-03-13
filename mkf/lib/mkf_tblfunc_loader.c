/*
 *	$Id$
 */

#include  "mkf_tblfunc_loader.h"

#include  <stdio.h>	/* NULL */


/* --- static functions --- */

static const char *  alt_lib_dir = "" ;


/* --- global functions --- */

int
mkf_set_alt_lib_dir(
	const char *  dir
	)
{
	alt_lib_dir = dir ;

	return  1 ;
}

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
		    ! ( handle = kik_dl_open( alt_lib_dir, "mkf_8bits")))
		{
			return  NULL ;
		}
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
		    ! ( handle = kik_dl_open( alt_lib_dir, "mkf_jajp")))
		{
			return  NULL ;
		}
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
		    ! ( handle = kik_dl_open( alt_lib_dir, "mkf_kokr")))
		{
			return  NULL ;
		}
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
		    ! ( handle = kik_dl_open( alt_lib_dir, "mkf_zh")))
		{
			return  NULL ;
		}
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
