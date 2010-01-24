/*
 *	$Id$
 */

#include  "kik_dlfcn.h"

#include  <stdio.h>		/* NULL */
#include  <string.h>		/* strlen */

#include  "kik_mem.h"		/* alloca() */


/* --- global functions --- */

/*
 * dummy codes
 */
 
kik_dl_handle_t
kik_dl_open(
	const char *  dirpath ,
	const char *  name
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
	const char *  symbol
	)
{
	return  NULL ;
}

int
kik_dl_is_module(
	const char *  name
	)
{
	return  0 ;
}

