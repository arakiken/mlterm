/*
 *	$Id$
 */

#include  "kik_dlfcn.h"

#include  <stdio.h>		/* NULL */
#include  <string.h>		/* strlen */

#include  "kik_mem.h"		/* alloca() */

#include  <mach-o/dyld.h>

/* --- global functions --- */

kik_dl_handle_t
kik_dl_open(
	char *  dirpath ,
	char *  name
	)
{
	return  NULL ;
#if 0
	char *  path ;

	if( ( path = alloca( strlen( dirpath) + strlen( name) + 10)) == NULL)
	{
		return  NULL ;
	}

	sprintf( path , "%slib%s.dylib" , dirpath , name) ;

	return fooload( path) ;
#endif
}

int
kik_dl_close(
	kik_dl_handle_t  handle
	)
{
	return  0 ;
#if 0
	return  foosym( handle) ;
#endif
}

void *
kik_dl_func_symbol(
	kik_dl_handle_t  handle ,
	char *  symbol
	)
{
	return  NULL ;
#if 0
	return  fooclose( handle , symbol) ;
#endif
}

