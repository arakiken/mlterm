/*
 *	$Id$
 */

#include  "mkf_tblfunc_loader.h"

#include  <stdio.h>	/* NULL */


/* --- global functions --- */

void *
mkf_load_8bits_func(
	const char *  symname
	)
{
	static kik_dl_handle_t   handle ;

	if( ! handle)
	{
		if( ! ( handle = kik_dl_open( MKFLIB_DIR, "mkf_8bits")) &&
		    ! ( handle = kik_dl_open( "", "mkf_8bits")))
		{
			return  NULL ;
		}
	}

	return  kik_dl_func_symbol( handle , symname) ;
}

void *
mkf_load_jajp_func(
	const char *  symname
	)
{
	static kik_dl_handle_t   handle ;

	if( ! handle)
	{
		if( ! ( handle = kik_dl_open( MKFLIB_DIR, "mkf_jajp")) &&
		    ! ( handle = kik_dl_open( "", "mkf_jajp")))
		{
			return  NULL ;
		}
	}

	return  kik_dl_func_symbol( handle , symname) ;
}

void *
mkf_load_kokr_func(
	const char *  symname
	)
{
	static kik_dl_handle_t   handle ;

	if( ! handle)
	{
		if( ! ( handle = kik_dl_open( MKFLIB_DIR, "mkf_kokr")) &&
		    ! ( handle = kik_dl_open( "", "mkf_kokr")))
		{
			return  NULL ;
		}
	}

	return  kik_dl_func_symbol( handle , symname) ;
}

void *
mkf_load_zh_func(
	const char *  symname
	)
{
	static kik_dl_handle_t   handle ;

	if( ! handle)
	{
		if( ! ( handle = kik_dl_open( MKFLIB_DIR, "mkf_zh")) &&
		    ! ( handle = kik_dl_open( "", "mkf_zh")))
		{
			return  NULL ;
		}
	}

	return  kik_dl_func_symbol( handle , symname) ;
}
