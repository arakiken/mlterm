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
	char *  dirpath ,
	char *  name
	)
{
	char *  path ;
	void *  ret ;

	if( ( path = alloca( strlen( dirpath) + strlen( name) + 7)) == NULL)
	{
		return  NULL ;
	}

	sprintf( path , "%slib%s.so" , dirpath , name) ;

	if( ( ret = dlopen( path , RTLD_LAZY)) == NULL)
	{
		/* HP-UX libfoo.sl */
		sprintf( path , "%slib%s.sl" , dirpath , name) ;
		ret = dlopen( path , RTLD_LAZY) ;
	}

	return  ret ;
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
	char *  symbol
	)
{
	return  dlsym( handle , symbol) ;
}

int
kik_dl_is_module(
	char * name
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

