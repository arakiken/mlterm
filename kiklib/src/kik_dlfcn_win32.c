/*
 *	$Id$
 */

#include  "kik_dlfcn.h"

#include  <stdio.h>		/* NULL */
#include  <string.h>		/* strlen */
#include  <limits.h>		/* MAX_PATH */
#include  "kik_mem.h"		/* alloca() */

#include  <windows.h>
#include  <winbase.h>

/* --- global functions --- */

kik_dl_handle_t
kik_dl_open(
	char *  dirpath ,
	char *  name
	)
{
	char *  path ;
#ifdef __CYGWIN__
	char  winpath[MAX_PATH];
#endif

	if( ( path = alloca( strlen( dirpath) + strlen( name) + 8)) == NULL)
	{
		return  NULL ;
	}

#ifdef __CYGWIN__
	sprintf( path , "%scyg%s.dll" , dirpath , name) ;
	cygwin_conv_to_win32_path( path , winpath);
	path = winpath ;
#else
	sprintf( path , "%slib%s.dll" , dirpath , name) ;
#endif

	return  ( kik_dl_handle_t)LoadLibrary( path) ;
}

int
kik_dl_close(
	kik_dl_handle_t  handle
	)
{
	HINSTANCE  hinstance ;

	hinstance = ( HINSTANCE)handle ;

	return  FreeLibrary( hinstance) ;
}

void *
kik_dl_func_symbol(
	kik_dl_handle_t  handle ,
	char *  symbol
	)
{
	HINSTANCE  hinstance ;

	hinstance = ( HINSTANCE)handle ;

	return  GetProcAddress( hinstance , symbol) ;
}

