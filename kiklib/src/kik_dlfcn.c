/*
 *	$Id$
 */

#include  "kik_dlfcn.h"

#include  <stdio.h>		/* NULL */
#include  <string.h>		/* strlen */

#include  "kik_mem.h"		/* alloca() */


/* --- global functions --- */

#if  defined(HAVE_DL_H)

kik_dl_handle_t
kik_dl_open(
	char *  path
	)
{
	char *  _path ;
	
	if( ( _path = alloca( strlen( path) + 4)) == NULL)
	{
		return  NULL ;
	}

	sprintf( _path , "%s.sl" , path) ;

	return  shl_load( _path , BIND_DEFERRED , 0x0) ;
}

int
kik_dl_close(
	kik_dl_handle_t  handle
	)
{
	return  shl_unload( handle) ;
}

void *
kik_dl_func_symbol(
	kik_dl_handle_t  handle ,
	char *  symbol
	)
{
	void *  func ;
	
	if( shl_findsym( &handle , symbol , TYPE_PROCEDURE , &func) == -1)
	{
		return  NULL ;
	}

	return  func ;
}

#elif  defined(HAVE_DLFCN_H)

kik_dl_handle_t
kik_dl_open(
	char *  path
	)
{
	char *  _path ;
	
	if( ( _path = alloca( strlen( path) + 5)) == NULL)
	{
		return  NULL ;
	}

#ifndef __CYGWIN__
	sprintf( _path , "%s.so" , path) ;
#else
	sprintf( _path , "%s.dll" , path) ;
#endif
	
	return  dlopen( _path , RTLD_LAZY) ;
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

#else

/*
 * dummy codes
 */
 
kik_dl_handle_t
kik_dl_open(
	char *  path
	)
{
	return  NULL ;
}

int
kik_dl_close(
	kik_dl_handle_t  handle
	)
{
	return  0 ;
}

void *
kik_dl_func_symbol(
	kik_dl_handle_t  handle ,
	char *  symbol
	)
{
	return  NULL ;
}

#endif
