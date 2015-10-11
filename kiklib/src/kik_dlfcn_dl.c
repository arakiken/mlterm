/*
 *	$Id$
 */

#include  "kik_dlfcn.h"

#include  <stdio.h>		/* NULL */
#include  <string.h>		/* strlen */

#include  "kik_mem.h"		/* alloca() */

#include  <dlfcn.h>

/* --- global functions --- */

kik_dl_handle_t
kik_dl_open(
	const char *  dirpath ,
	const char *  name
	)
{
	char *  path ;
	void *  ret ;

	if( ( path = alloca( strlen( dirpath) + strlen( name) + 7)) == NULL)
	{
		return  NULL ;
	}

	/*
	 * libfoo.so --> foo.so --> libfoo.sl --> foo.sl
	 */

	sprintf( path , "%slib%s.so" , dirpath , name) ;
	if( ( ret = dlopen( path , RTLD_LAZY)))
	{
		return  (kik_dl_handle_t)ret ;
	}

#ifndef  __APPLE__
	sprintf( path , "%slib%s.sl" , dirpath , name) ;
	if( ( ret = dlopen( path , RTLD_LAZY)))
	{
		return  (kik_dl_handle_t)ret ;
	}

	sprintf( path , "%s%s.so" , dirpath , name) ;
	if( ( ret = dlopen( path , RTLD_LAZY)))
	{
		return  (kik_dl_handle_t)ret ;
	}

	sprintf( path , "%s%s.sl" , dirpath , name) ;
	if( ( ret = dlopen( path , RTLD_LAZY)))
	{
		return  (kik_dl_handle_t)ret ;
	}
#else
	{
		char *  p ;

		/* XXX Hack */
		if( ( strcmp( (p = dirpath + strlen(dirpath) - 4), "mkf/") == 0 ||
		      strcmp( (p -= 3) , "mlterm/") == 0) &&
		    ( path = alloca( 21 + strlen(p) + 3 + strlen(name) + 3 + 1)))
		{
			sprintf( path , "@executable_path/lib/%slib%s.so" , p , name) ;
		}
		else
		{
			return  NULL ;
		}

		if( ( ret = dlopen( path , RTLD_LAZY)))
		{
			return  (kik_dl_handle_t)ret ;
		}
	}
#endif

	return  NULL ;
}

int
kik_dl_close(
	kik_dl_handle_t  handle
	)
{
	return  dlclose( handle) ;
}

void *
kik_dl_func_symbol(
	kik_dl_handle_t  handle ,
	const char *  symbol
	)
{
	return  dlsym( handle , symbol) ;
}

int
kik_dl_is_module(
	const char * name
	)
{
	size_t  len ;

	if ( ! name)
	{
		return  0 ;
	}

	if( ( len = strlen( name)) < 3)
	{
		return  0 ;
	}

	if( strcmp( &name[len - 3] , ".so") == 0 ||
	    strcmp( &name[len - 3] , ".sl") == 0)
	{
		return  1 ;
	}

	return  0 ;
}

