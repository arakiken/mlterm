/*
 *	$Id$
 */

#include  "kik_dlfcn.h"

#include  <stdio.h>		/* NULL */
#include  <string.h>		/* strlen */

#include  "kik_mem.h"		/* alloca() */

#include  <dl.h>

/* --- global functions --- */

kik_dl_handle_t
kik_dl_open(
	const char *  dirpath ,
	const char *  name
	)
{
	char *  path ;
	shl_t  handle ;

	if( ( path = alloca( strlen( dirpath) + strlen( name) + 7)) == NULL)
	{
		return  NULL ;
	}


	/*
	 * libfoo.sl --> foo.sl
	 */

	sprintf( path , "%slib%s.sl" , dirpath , name) ;
	if( ( handle = shl_load( path , BIND_DEFERRED , 0x0)))
	{
		return  (kik_dl_handle_t) handle ;
	}

	sprintf( path , "%s%s.sl" , dirpath , name) ;
	if( ( handle = shl_load( path , BIND_DEFERRED , 0x0)))
	{
		return  (kik_dl_handle_t) handle ;
	}

	return  NULL ;
}

int
kik_dl_close(
	kik_dl_handle_t  handle
	)
{
	return  shl_unload( (shl_t)handle) ;
}

void *
kik_dl_func_symbol(
	kik_dl_handle_t  handle ,
	const char *  symbol
	)
{
	void *  func ;

	if( shl_findsym( (shl_t*)&handle , symbol , TYPE_PROCEDURE , &func) == -1)
	{
		return  NULL ;
	}

	return  func ;
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

	if( strcmp(&name[len - 3] , ".sl") == 0)
	{
		return  1 ;
	}

	return  0 ;
}

