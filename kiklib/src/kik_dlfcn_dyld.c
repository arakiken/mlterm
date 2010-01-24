/*
 *	$Id$
 */

#include  "kik_dlfcn.h"

#include  <stdio.h>		/* NULL */
#include  <string.h>		/* strlen */

#include  "kik_mem.h"		/* alloca() */
#include  "kik_list.h"
#ifdef  DEBUG
#include  "kik_debug.h"
#endif

#include  <mach-o/dyld.h>

typedef struct  loaded_module
{
	kik_dl_handle_t  handle ;
	char *  dirpath ;
	char *  name ;

	u_int  ref_count ;

}  loaded_module_t ;

KIK_LIST_TYPEDEF( loaded_module_t) ;

/* --- static functions --- */

static KIK_LIST( loaded_module_t)  module_list = NULL ;

/* --- global functions --- */

kik_dl_handle_t
kik_dl_open(
	const char *  dirpath ,
	const char *  name
	)
{
	NSObjectFileImage  file_image;
	NSObjectFileImageReturnCode  ret;
	loaded_module_t *  module = NULL ;
	kik_dl_handle_t  handle = NULL ;
	KIK_ITERATOR( loaded_module_t)  iterator = NULL ;
	char *  path ;

	if( ! module_list)
	{
		kik_list_new( loaded_module_t , module_list) ;
	}

	iterator = kik_list_first( module_list) ;
	while( iterator)
	{
		if( kik_iterator_indirect( iterator) == NULL)
		{
			kik_error_printf(
				"iterator found , but it has no logs."
				"don't you cross over memory boundaries anywhere?\n") ;
		}
		else if ( strcmp( kik_iterator_indirect( iterator)->dirpath , dirpath) == 0 &&
			  strcmp( kik_iterator_indirect( iterator)->name , name) == 0)
		{
			kik_iterator_indirect( iterator)->ref_count ++ ;
			return  kik_iterator_indirect( iterator)->handle ;
		}

		iterator = kik_iterator_next( iterator) ;
	}

	if( ! ( module = malloc( sizeof(loaded_module_t))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif
		return  NULL ;
	}

	module->dirpath = strdup( dirpath) ;
	module->name = strdup( name) ;
	module->ref_count = 0 ;

	if( ( path = alloca( strlen( dirpath) + strlen( name) + 7)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif
		return  NULL ;
	}

	/*
	 * libfoo.so --> foo.so
	 */

	sprintf( path , "%slib%s.so" , dirpath , name) ;

	if( ( ret = NSCreateObjectFileImageFromFile( path , &file_image)) != NSObjectFileImageSuccess)
	{
		sprintf( path , "%s%s.so" , dirpath , name) ;
		if( ( ret = NSCreateObjectFileImageFromFile( path , &file_image)) != NSObjectFileImageSuccess)
		{
			goto  error ;
		}
	}

	handle = (kik_dl_handle_t)NSLinkModule( file_image ,
						path ,
						NSLINKMODULE_OPTION_BINDNOW);

	if( ! handle)
	{
		goto  error ;
	}

	kik_list_insert_head( loaded_module_t , module_list , module) ;
	module->handle = handle ;
	module->ref_count ++ ;

	return  handle ;

error:
	if( module)
	{
		free( module->dirpath) ;
		free( module->name) ;
		free( module) ;
	}

	if( module_list && kik_list_is_empty( module_list))
	{
		kik_list_delete( loaded_module_t , module_list) ;
		module_list = NULL ;
	}

	return  NULL ;
}

int
kik_dl_close(
	kik_dl_handle_t  handle
	)
{
	KIK_ITERATOR( loaded_module_t)  iterator = NULL ;
	loaded_module_t *  module ;

	if( ! module_list)
	{
		return  1 ;
	}

	iterator = kik_list_first( module_list) ;
	while( iterator)
	{
		if( kik_iterator_indirect( iterator) == NULL)
		{
			kik_error_printf(
				"iterator found , but it has no logs."
				"don't you cross over memory boundaries anywhere?\n") ;
		}
		else if ( kik_iterator_indirect( iterator)->handle == handle)
		{
			module = kik_iterator_indirect( iterator) ;

			module->ref_count -- ;

			if( module->ref_count)
			{
				return  0 ;
			}

			break ;
		}

		iterator = kik_iterator_next( iterator) ;
	}

	if( NSUnLinkModule( (NSModule)module->handle , NSUNLINKMODULE_OPTION_NONE) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " NSUnLinkModule() failed.\n" ) ;
	#endif
		return  1 ;
	}

	free( module->dirpath) ;
	free( module->name) ;
	free( module) ;
	kik_list_remove( loaded_module_t , module_list , iterator) ;

	if( kik_list_is_empty( module_list))
	{
		kik_list_delete( loaded_module_t , module_list) ;
		module_list = NULL ;
	}

	return  0 ;
}

void *
kik_dl_func_symbol(
	kik_dl_handle_t  unused ,
	const char *  symbol
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
	const char *  name
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

