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
	char *  dirpath ,
	char *  name
	)
{
	char *  path ;

	if( ( path = alloca( strlen( dirpath) + strlen( LIB_PREFIX) + strlen( name) + 4)) == NULL)
	{
		return  NULL ;
	}

	sprintf( path , "%s%s%s.sl" , dirpath , LIB_PREFIX , name) ;

	return  shl_load( path , BIND_DEFERRED , 0x0) ;
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
	char *  dirpath ,
	char *  name
	)
{
	char *  path ;

	if( ( path = alloca( strlen( dirpath) + strlen( LIB_PREFIX) + strlen( name) + strlen( LIB_SUFFIX) + 1)) == NULL)
	{
		return  NULL ;
	}

	sprintf( path , "%s%s%s%s" , dirpath , LIB_PREFIX , name , LIB_SUFFIX) ;

	return  dlopen( path , RTLD_LAZY) ;
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
	char *  dirpath ,
	char *  name
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
