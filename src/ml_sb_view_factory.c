/*
 *	$Id$
 */

#include  "ml_sb_view_factory.h"

#include  <stdio.h>		/* sprintf */
#include  <kiklib/kik_dlfcn.h>
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_debug.h>

#include  "ml_simple_sb_view.h"


#ifndef  LIBDIR
#define  SBLIB_DIR  "/usr/local/lib/mlterm/"
#else
#define  SBLIB_DIR  LIBDIR "/mlterm/"
#endif


typedef struct  lib_ref
{
	char *  name ;
	kik_dl_handle_t  handle ;
	u_int  count ;

} lib_ref_t ;


/* --- static variables --- */

static lib_ref_t *  lib_ref_table ;
static u_int  lib_ref_table_size ;


/* --- static functions --- */

static lib_ref_t *
search_lib_ref(
	char *  name
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < lib_ref_table_size ; counter ++)
	{
		if( strcmp( lib_ref_table[counter].name , name) == 0)
		{
			return  &lib_ref_table[counter] ;
		}
	}

	return  NULL ;
}

static ml_sb_view_new_func_t
dlsym_sb_view_new_func(
	char *  name ,
	int  is_transparent
	)
{
	char *  dir ;
	ml_sb_view_new_func_t  func ;
	kik_dl_handle_t  handle ;
	char *  symbol ;
	u_int  len ;
	lib_ref_t *  lib_ref ;

	if( ( lib_ref = search_lib_ref( name)))
	{
		handle = lib_ref->handle ;
		lib_ref->count ++ ;
	}
	else
	{
		void *  p ;
		char *  libpath ;

		dir = SBLIB_DIR ;

		len = strlen( dir) + 3 + strlen( name) + 1 ;
		if( ( libpath = alloca( len)) == NULL)
		{
			return  NULL ;
		}

		sprintf( libpath , "%slib%s" , dir , name) ;

		if( ( handle = kik_dl_open( libpath)) == NULL)
		{
			return  NULL ;
		}
		
		if( ( p = realloc( lib_ref_table , sizeof( lib_ref_t) * (lib_ref_table_size + 1)))
			== NULL)
		{
			kik_dl_close( handle) ;
			
			return  NULL ;
		}

		lib_ref_table = p ;
		lib_ref = &lib_ref_table[lib_ref_table_size ++] ;

		lib_ref->name = strdup( name) ;
		lib_ref->handle = handle ;
		lib_ref->count = 1 ;
	}

	len = 27 + strlen( name) + 1 ;
	if( ( symbol = alloca( len)) == NULL)
	{
		return  NULL ;
	}

	if( is_transparent)
	{
		sprintf( symbol , "ml_%s_transparent_sb_view_new" , name) ;
	}
	else
	{
		sprintf( symbol , "ml_%s_sb_view_new" , name) ;
	}
	
	if( ( func = (ml_sb_view_new_func_t) kik_dl_func_symbol( handle , symbol)) == NULL)
	{
		return  NULL ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s %d\n" , lib_ref->name , lib_ref->count) ;
#endif

	return  func ;
}


/* --- global functions --- */

ml_sb_view_t *
ml_sb_view_new(
	char *  name
	)
{
	ml_sb_view_new_func_t  func ;

	if( strcmp( name , "simple") == 0)
	{
		return  ml_simple_sb_view_new() ;
	}
	else if( ( func = dlsym_sb_view_new_func( name , 0)) == NULL)
	{
		return  NULL ;
	}
	
	return  (*func)() ;
}

ml_sb_view_t *
ml_transparent_scrollbar_view_new(
	char *  name
	)
{
	ml_sb_view_new_func_t  func ;

	if( strcmp( name , "simple") == 0)
	{
		return  ml_simple_transparent_sb_view_new() ;
	}
	else if( ( func = dlsym_sb_view_new_func( name , 0)) == NULL)
	{
		return  NULL ;
	}

	if( ( func = dlsym_sb_view_new_func( name , 1)) == NULL)
	{
		return  NULL ;
	}
	
	return  (*func)() ;
}

int
ml_unload_scrollbar_view_lib(
	char *  name
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < lib_ref_table_size ; counter ++)
	{
		if( strcmp( lib_ref_table[counter].name , name) == 0)
		{
			if( -- lib_ref_table[counter].count == 0)
			{
			#ifdef  __DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " %s unloaded.\n" , name) ;
			#endif
			
				kik_dl_close( lib_ref_table[counter].handle) ;
				free( lib_ref_table[counter].name) ;

				if( -- lib_ref_table_size == 0)
				{
					free( lib_ref_table) ;
					lib_ref_table = NULL ;
				}
				else
				{
					lib_ref_table[counter] = lib_ref_table[lib_ref_table_size] ;
				}
				
				return  1 ;
			}
		}
	}

	return  NULL ;
}
