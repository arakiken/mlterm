/*
 *	$Id$
 */

#include  "x_sb_view_factory.h"

#include  <stdio.h>		/* sprintf */
#include  <kiklib/kik_dlfcn.h>
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_list.h>

#include  "x_simple_sb_view.h"

#ifndef  LIBDIR
#define  SBLIB_DIR  "/usr/local/lib/mlterm/"
#else
#define  SBLIB_DIR  LIBDIR "/mlterm/"
#endif

#ifndef  DATADIR
#define  SB_DIR  "/usr/local/share/mlterm/scrollbars"
#else
#define  SB_DIR  DATADIR "/mlterm/scrollbars"
#endif

typedef struct  lib_ref
{
	char *  name ;
	kik_dl_handle_t  handle ;
	u_int  count ;

} lib_ref_t ;

KIK_LIST_TYPEDEF( x_sb_view_conf_t) ;

/* --- static variables --- */

static lib_ref_t *  lib_ref_table ;
static u_int  lib_ref_table_size ;
static KIK_LIST( x_sb_view_conf_t)  view_conf_list = NULL ;

/* --- static functions --- */

static lib_ref_t *
search_lib_ref(
	char *  name
	)
{
	int  count ;
	
	for( count = 0 ; count < lib_ref_table_size ; count ++)
	{
		if( strcmp( lib_ref_table[count].name , name) == 0)
		{
			return  &lib_ref_table[count] ;
		}
	}

	return  NULL ;
}

static x_sb_view_new_func_t
dlsym_sb_view_new_func(
	char *  name ,
	int  is_transparent
	)
{
	x_sb_view_new_func_t  func ;
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

		if( ( handle = kik_dl_open( SBLIB_DIR , name)) == NULL)
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
		sprintf( symbol , "x_%s_transparent_sb_view_new" , name) ;
	}
	else
	{
		sprintf( symbol , "x_%s_sb_view_new" , name) ;
	}
	
	if( ( func = (x_sb_view_new_func_t) kik_dl_func_symbol( handle , symbol)) == NULL)
	{
		/* backward compatible with 2.4.0 or before */
		
		if( is_transparent)
		{
			sprintf( symbol , "ml_%s_transparent_sb_view_new" , name) ;
		}
		else
		{
			sprintf( symbol , "ml_%s_sb_view_new" , name) ;
		}

		if( ( func = (x_sb_view_new_func_t) kik_dl_func_symbol( handle , symbol)) == NULL)
		{
			return  NULL ;
		}
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s %d\n" , lib_ref->name , lib_ref->count) ;
#endif

	return  func ;
}

static x_sb_view_conf_t *
search_view_conf(
	char *  sb_name
	)
{
	KIK_ITERATOR( x_sb_view_conf_t)  iterator = NULL ;

	if( view_conf_list == NULL)
	{
		return  NULL ;
	}

	iterator = kik_list_first( view_conf_list) ;
	while( iterator)
	{
		if( kik_iterator_indirect( iterator) == NULL)
		{
			kik_error_printf(
				"iterator found , but it has no logs."
				"don't you cross over memory boundaries anywhere?\n") ;
		}
		else if( strcmp(kik_iterator_indirect( iterator)->sb_name , sb_name) == 0)
		{
			return  kik_iterator_indirect( iterator) ;
		}

		iterator = kik_iterator_next( iterator) ;
	}

	return  NULL ;
}

static void
free_conf(
	x_sb_view_conf_t *  conf
	)
{
	x_sb_view_rc_t *  rc ;
	int  i ;

	free( conf->sb_name) ;
	free( conf->engine_name) ;
	free( conf->dir) ;

	for( rc = conf->rc , i = 0 ; i < conf->rc_num ; rc ++ , i ++)
	{
		free( rc->key) ;
		free( rc->value) ;
	}

	free( conf->rc) ;

	free( conf) ;
}

static x_sb_view_conf_t *
register_new_view_conf(
	kik_file_t *  rcfile ,
	char *  sb_name ,
	char *  rcfile_path
	)
{
	x_sb_view_conf_t *  conf ;
	char *  key ;
	char *  value ;
	int  len ;

	if( ( conf = malloc( sizeof( x_sb_view_conf_t))) == NULL)
	{
		return  NULL ;
	}

	conf->sb_name = strdup( sb_name) ;

	conf->engine_name = NULL ;
	conf->rc = NULL ;
	conf->rc_num = 0 ;
	conf->dir = NULL ;
	conf->use_count = 0 ;

	/* remove "/rc" /foo/bar/name/rc -> /foo/bar/name */
	len = strlen( rcfile_path) - 3 ;
	if( ( conf->dir = malloc(sizeof( char) * ( len + 1))) == NULL)
	{
		free_conf( conf) ;
		return  NULL ;
	}
	strncpy( conf->dir , rcfile_path , len) ;
	conf->dir[len] = '\0' ;

	while( kik_conf_io_read( rcfile , &key , &value))
	{
		if( strcmp( key , "engine") == 0)
		{
			conf->engine_name = strdup( value) ;
		}
		else
		{
			x_sb_view_rc_t *  p ;

			if( ( p = realloc( conf->rc , sizeof( x_sb_view_rc_t) * (conf->rc_num + 1))) == NULL)
			{
			#ifdef __DEBUG
				kik_debug_printf( "realloc() failed.") ;
			#endif
				
				free_conf( conf) ;
				
				return  NULL ;
			}
			conf->rc = p ;
			p = &conf->rc[conf->rc_num] ;
			p->key = strdup( key) ;
			p->value = strdup( value) ;
			conf->rc_num ++ ;
		}
	}

#ifdef __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "%s has been registered as new view. [dir: %s]\n" , conf->sb_name , conf->dir);
#endif

	if( view_conf_list == NULL)
	{
	#ifdef __DEBUG
		kik_debug_printf( "view_conf_list is NULL. creating\n") ;
	#endif
		kik_list_new( x_sb_view_conf_t , view_conf_list) ;
	}

	kik_list_insert_head( x_sb_view_conf_t , view_conf_list , conf) ;

	return  conf ;
}

static x_sb_view_conf_t *
find_view_rcfile(
	char *  name
	)
{
	x_sb_view_conf_t *  conf ;
	kik_file_t *  rcfile ;
	char *  user_dir ;
	char *  sb_dir ;
	char *  path ;
	int  count ;

	/* search known conf from view_conf_list */
	if( ( conf = search_view_conf( name)))
	{
	#ifdef __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG "%s was found in view_conf_list\n" , sb_name) ;
	#endif

		return  conf ;
	}

	if( ! ( user_dir = kik_get_user_rc_path( "mlterm/scrollbars")))
	{
		return  NULL ;
	}

	if( ! ( path = malloc( strlen( user_dir) + strlen(name) + 5)))
	{
		free( user_dir) ;
		return  NULL ;
	}

	sprintf( path , "%s/%s/rc" , user_dir , name);
	free( user_dir) ;

	if( ! ( rcfile = kik_file_open( path , "r")))
	{
		if( ! ( path = realloc( path , strlen( SB_DIR) + strlen( name) + 5)))
		{
			return  NULL ;
		}

		sprintf( path, "%s/%s/rc" , SB_DIR , name);

		if( ! ( rcfile = kik_file_open( path , "r")))
		{
		#ifdef __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG "rcfile for %s could not be found\n" , name);
		#endif
			free( path) ;
			return  NULL ;
		}
	}

#ifdef __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "rcfile for %s: %s\n" , name, path);
#endif

	conf = register_new_view_conf( rcfile , name , path) ;

	free( path) ;

	kik_file_close( rcfile) ;

	return  conf ;
}


/* --- global functions --- */

x_sb_view_t *
x_sb_view_new(
	char *  name
	)
{
	x_sb_view_new_func_t  func ;
	x_sb_view_conf_t *  conf ;

	/* new style plugin ? (requires rcfile and engine library) */
	if( ( conf = find_view_rcfile( name)))
	{
		/* lib@name@.so -> lib@engine_name@.so */
		name = conf->engine_name ;
	}

	if( strcmp( name , "simple") == 0)
	{
		return  x_simple_sb_view_new() ;
	}
	else if( ( func = dlsym_sb_view_new_func( name , 0)) == NULL)
	{
		return  NULL ;
	}

	if( conf)
	{
		/*
		 * if loaded library is new style,
		 * you need argument (x_sb_view_conf_t *)
		 */
		x_sb_view_new_func_with_arg_t  func_with_arg ;
		
		func_with_arg = (x_sb_view_new_func_with_arg_t)func ;
		
		return  (*func_with_arg)( conf) ;
	}
	
	return  (*func)() ;
}

x_sb_view_t *
x_transparent_scrollbar_view_new(
	char *  name
	)
{
	x_sb_view_new_func_t  func ;
	x_sb_view_conf_t *  conf ;

	/* new style plugin? (requires a rcfile and an engine library) */
	if( ( conf = find_view_rcfile( name)))
	{
		/* lib@name@.so -> lib@engine_name@.so */
		name = conf->engine_name ;
	}

	if( strcmp( name , "simple") == 0)
	{
		return  x_simple_transparent_sb_view_new() ;
	}
	else if( ( func = dlsym_sb_view_new_func( name , 0)) == NULL)
	{
		return  NULL ;
	}

	if( ( func = dlsym_sb_view_new_func( name , 1)) == NULL)
	{
		return  NULL ;
	}

	if( conf)
	{
		/*
		 * if loaded library is new style,
		 * you need argument (x_sb_view_conf_t *)
		 */
		x_sb_view_new_func_with_arg_t  func_with_arg ;
		
		func_with_arg = (x_sb_view_new_func_with_arg_t)func ;
		
		return  (*func_with_arg)( conf) ;
	}

	return  (*func)() ;
}

int
x_unload_scrollbar_view_lib(
	char *  name
	)
{
	x_sb_view_conf_t *  conf ;
	int  count ;
	int  ret = 0 ;

	/* new style plugin? (requires a rcfile and an engine library) */
	if( ( conf = search_view_conf( name)))
	{
		/* lib@name@.so -> lib@engine_name@.so */
		name = conf->engine_name ;
	}

	for( count = 0 ; count < lib_ref_table_size ; count ++)
	{
		if( strcmp( lib_ref_table[count].name , name) == 0)
		{
			if( -- lib_ref_table[count].count == 0)
			{
			#ifdef  __DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " %s unloaded.\n" , name) ;
			#endif
			
				kik_dl_close( lib_ref_table[count].handle) ;
				free( lib_ref_table[count].name) ;
				
				if( -- lib_ref_table_size == 0)
				{
					free( lib_ref_table) ;
					lib_ref_table = NULL ;
				}
				else
				{
					lib_ref_table[count] = lib_ref_table[lib_ref_table_size] ;
				}
				
				ret = 1 ;

				break ;
			}
		}
	}

	if( conf)
	{
		/* remove unused conf */
		if( conf->use_count == 0)
		{
		#ifdef __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " %s is no longer used. removing from view_conf_list\n", name);
		#endif
			kik_list_search_and_remove( x_sb_view_conf_t , view_conf_list , conf) ;

			free_conf( conf) ;

			if( kik_list_is_empty( view_conf_list))
			{
			#ifdef __DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " view_conf_list is empty. freeing\n");
			#endif
				kik_list_delete( x_sb_view_conf_t , view_conf_list) ;
				view_conf_list = NULL ;
			}
		}
		#ifdef __DEBUG
		else
		{
			kik_debug_printf( KIK_DEBUG_TAG " %s is still being used. [use_count: %d]\n", conf->use_count) ;
		}
		#endif
	}

	return  ret ;
}
