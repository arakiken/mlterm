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

	if( ( ret == dlopen( path , RTLD_LAZY)) == NULL)
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

