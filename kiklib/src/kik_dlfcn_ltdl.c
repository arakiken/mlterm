/*
 *	$Id$
 */

#include  "kik_dlfcn.h"

#include  <stdio.h>		/* NULL */
#include  <string.h>		/* strlen */

#include  "kik_mem.h"		/* alloca() */

#include  <ltdl.h>

static int ready_sym_table = 0;

/* --- global functions --- */

kik_dl_handle_t
kik_dl_open(
	char *  dirpath ,
	char *  name
	)
{
	lt_dlhandle  handle ;
	char *  path ;

	if( ! ready_sym_table) {
		LTDL_SET_PRELOADED_SYMBOLS() ;
		ready_sym_table = 1 ;
	}

	if(lt_dlinit())
	{
		return  NULL ;
	}

	if( ( path = alloca( strlen( dirpath) + strlen( name) + 4)) == NULL)
	{
		return  NULL ;
	}

	sprintf( path , "%slib%s" , dirpath , name) ;

	handle = lt_dlopenext( path ) ;

	return  (kik_dl_handle_t)handle;
}

int
kik_dl_close(
	kik_dl_handle_t  handle
	)
{
	int  ret ;

	ret = lt_dlclose( (lt_dlhandle)handle) ;

	lt_dlexit();

	return  ret ;
}

void *
kik_dl_func_symbol(
	kik_dl_handle_t  handle ,
	char *  symbol
	)
{
	return  lt_dlsym( (lt_dlhandle)handle , symbol) ;
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

	if( strcmp( &name[len - 3] , ".la") == 0)
	{
		return  1 ;
	}

	return  0 ;
}

