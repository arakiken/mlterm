/*
 *	$Id$
 */

#include  "kik_dlfcn.h"

#include  <stdio.h>		/* NULL */
#include  <string.h>		/* strlen */

#include  "kik_mem.h"		/* alloca() */
#ifdef  DEBUG
#include  "kik_debug.h"
#endif

#include  <mach-o/dyld.h>

/* --- global functions --- */

kik_dl_handle_t
kik_dl_open(
	char *  dirpath ,
	char *  name
	)
{
	NSObjectFileImage  file_image;
	NSObjectFileImageReturnCode  ret;
	char *  path ;

	if( ( path = alloca( strlen( dirpath) + strlen( name) + 7)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif
		return  NULL ;
	}

	sprintf( path , "%slib%s.so" , dirpath , name) ;

	if( ( ret = NSCreateObjectFileImageFromFile( path , &file_image)) != NSObjectFileImageSuccess)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " NSCreateObjectFileImageFromFile() failed. [ret: %d]\n" , ret) ;
	#endif
		return  NULL ;
	}

	return  (kik_dl_handle_t)NSLinkModule( file_image , path ,
#if 1
			NSLINKMODULE_OPTION_BINDNOW);
#else
 			NSLINKMODULE_OPTION_PRIVATE | NSLINKMODULE_OPTION_PRIVATE) ; 
#endif
}

int
kik_dl_close(
	kik_dl_handle_t  handle
	)
{
	NSModule  module ;

	module = (NSModule)handle ;

	if( NSUnLinkModule( module , NSUNLINKMODULE_OPTION_NONE) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " NSUnLinkModule() failed.\n" ) ;
	#endif
		return  1 ;
	}

	return  0 ;
}

void *
kik_dl_func_symbol(
	kik_dl_handle_t  unused ,
	char *  symbol
	)
{
	NSSymbol  nssymbol = NULL ;
	char *  symbol_name ;

	if( ( symbol_name = alloca( strlen( symbol) + 2)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n" ) ;
	#endif
		return  NULL ;
	}

	sprintf( symbol_name , "_%s" , symbol) ;

	if( ! NSIsSymbolNameDefined( symbol_name))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " NSIsSymbolNameDefined() failed. [symbol_name: %s]\n" , symbol_name) ;
	#endif
		return  NULL ;
	}

	if( ( nssymbol = NSLookupAndBindSymbol( symbol_name)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " NSLookupAndBindSymbol() failed. [symbol_name: %s]\n" , symbol_name) ;
	#endif
		return  NULL ;
	}

	return  NSAddressOfSymbol( nssymbol) ;
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

	if( strcmp( &name[len - 3] , ".so") == 0)
	{
		return  1 ;
	}

	return  0 ;
}

