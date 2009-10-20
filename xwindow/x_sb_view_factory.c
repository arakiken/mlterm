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

#if  1
#define  COMPAT_VER0
#endif


typedef  x_sb_view_t * (*x_sb_view_new_func_t)(void) ;
typedef  x_sb_view_t * (*x_sb_engine_new_func_t)( x_sb_view_conf_t *  conf, int is_transparent) ;

KIK_LIST_TYPEDEF( x_sb_view_conf_t) ;


#ifdef  COMPAT_VER0

/* For backward binary compatibility */
typedef struct x_sb_view_wrapper
{
	x_sb_view_t   view ;
	
	union
	{
		x_sb_view_ver0_t *  ver0 ;
	} u ;

} x_sb_view_wrapper_t ;

#endif


/* --- static variables --- */

static KIK_LIST( x_sb_view_conf_t)  view_conf_list = NULL ;


/* --- static functions --- */

#ifdef  COMPAT_VER0

static void
ver0_get_geometry_hints(
	x_sb_view_t *  view ,
	u_int *  width ,
	u_int *  top_margin ,
	u_int *  bottom_margin ,
	int *  up_button_y ,
	u_int *  up_button_height ,
	int *  down_button_y ,
	u_int *  down_button_height
	)
{
	x_sb_view_wrapper_t *  wrapper ;

	wrapper = (x_sb_view_wrapper_t*) view ;

	if( wrapper->u.ver0->get_geometry_hints)
	{
		(*wrapper->u.ver0->get_geometry_hints)( wrapper->u.ver0 ,
			width , top_margin , bottom_margin , up_button_y , up_button_height ,
			down_button_y , down_button_height) ;
	}
}

static void
ver0_get_default_color(
	x_sb_view_t *  view ,
	char **  fg_color ,
	char **  bg_color
	)
{
	x_sb_view_wrapper_t *  wrapper ;

	wrapper = (x_sb_view_wrapper_t*) view ;

	if( wrapper->u.ver0->get_default_color)
	{
		(*wrapper->u.ver0->get_default_color)( wrapper->u.ver0 ,
			fg_color , bg_color) ;
	}
}

static void
ver0_realized(
	x_sb_view_t *  view ,
	Display *  display ,
	int  screen ,
	Window  window ,
	GC  gc ,
	u_int  height
	)
{
	x_sb_view_wrapper_t *  wrapper ;

	wrapper = (x_sb_view_wrapper_t*) view ;

	if( wrapper->u.ver0->realized)
	{
		(*wrapper->u.ver0->realized)( wrapper->u.ver0 ,
			display , screen , window , gc , height) ;
	}
}

static void
ver0_resized(
	x_sb_view_t *  view ,
	Window  window ,
	u_int  height
	)
{
	x_sb_view_wrapper_t *  wrapper ;

	wrapper = (x_sb_view_wrapper_t*) view ;

	if( wrapper->u.ver0->resized)
	{
		(*wrapper->u.ver0->resized)( wrapper->u.ver0 , window , height) ;
	}
}

static void
ver0_delete(
	x_sb_view_t *  view
	)
{
	x_sb_view_wrapper_t *  wrapper ;

	wrapper = (x_sb_view_wrapper_t*) view ;

	if( wrapper->u.ver0->delete)
	{
		(*wrapper->u.ver0->delete)( wrapper->u.ver0) ;
	}

	free( wrapper) ;
}

static void
ver0_draw_scrollbar(
	x_sb_view_t *  view ,
	int  bar_top_y ,
	u_int  bar_height
	)
{
	x_sb_view_wrapper_t *  wrapper ;

	wrapper = (x_sb_view_wrapper_t*) view ;

	if( wrapper->u.ver0->draw_scrollbar)
	{
		(*wrapper->u.ver0->draw_scrollbar)( wrapper->u.ver0 ,
			bar_top_y , bar_height) ;
	}
}

static void
ver0_draw_up_button(
	x_sb_view_t *  view ,
	int  pressed
	)
{
	x_sb_view_wrapper_t *  wrapper ;

	wrapper = (x_sb_view_wrapper_t*) view ;

	if( pressed)
	{
		if( wrapper->u.ver0->up_button_pressed)
		{
			(*wrapper->u.ver0->up_button_pressed)( wrapper->u.ver0) ;
		}
	}
	else
	{
		if( wrapper->u.ver0->up_button_released)
		{
			(*wrapper->u.ver0->up_button_released)( wrapper->u.ver0) ;
		}
	}
}

static void
ver0_draw_down_button(
	x_sb_view_t *  view ,
	int  pressed
	)
{
	x_sb_view_wrapper_t *  wrapper ;

	wrapper = (x_sb_view_wrapper_t*) view ;

	if( pressed)
	{
		if( wrapper->u.ver0->down_button_pressed)
		{
			(*wrapper->u.ver0->down_button_pressed)( wrapper->u.ver0) ;
		}
	}
	else
	{
		if( wrapper->u.ver0->down_button_released)
		{
			(*wrapper->u.ver0->down_button_released)( wrapper->u.ver0) ;
		}
	}
}

#endif	/* COMPAT_VER0 */

static x_sb_view_t *
check_version(
	x_sb_view_t *  view
	)
{
	x_sb_view_wrapper_t *  wrapper ;
	
	if( view->version == 1)
	{
		return  view ;
	}

#ifdef  COMPAT_VER0

	/* version 0 */
	
	if( ( wrapper = malloc( sizeof( x_sb_view_wrapper_t))) == NULL)
	{
		(*view->delete)( view) ;

		/* Regardless of transparent or not. */
		return  x_simple_sb_view_new() ;
	}

	wrapper->u.ver0 = (x_sb_view_ver0_t*) view ;
	
	view = &wrapper->view ;
	view->version = 0 ;
	view->get_geometry_hints = ver0_get_geometry_hints ;
	view->get_default_color = ver0_get_default_color ;
	view->realized = ver0_realized ;
	view->resized = ver0_resized ;
	view->color_changed = NULL ;
	view->delete = ver0_delete ;
	view->draw_scrollbar = ver0_draw_scrollbar ;
	view->draw_background = NULL ;
	view->draw_up_button = ver0_draw_up_button ;
	view->draw_down_button = ver0_draw_down_button ;

	return  view ;

#else	/* COMPAT_VER0 */

	return  NULL ;

#endif
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

	if( ( handle = kik_dl_open( SBLIB_DIR , name)) == NULL &&
		( handle = kik_dl_open( "" , name)) == NULL)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " kik_dl_open(%s) failed.\n" , name) ;
	#endif
	
		return  NULL ;
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
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Loading %s failed.\n" , symbol) ;
	#endif
		
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
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Loading %s failed.\n" , symbol) ;
		#endif
		
			return  NULL ;
		}
	}

	return  func ;
}

static x_sb_engine_new_func_t
dlsym_sb_engine_new_func(
	char *  name
	)
{
	x_sb_engine_new_func_t  func ;
	kik_dl_handle_t  handle ;
	char *  symbol ;
	u_int  len ;

	if( ( handle = kik_dl_open( SBLIB_DIR , name)) == NULL ||
		( handle = kik_dl_open( "" , name)) == NULL)
	{
		return  NULL ;
	}

	len = 16 + strlen( name) + 1 ;
	if( ( symbol = alloca( len)) == NULL)
	{
		return  NULL ;
	}

	sprintf( symbol , "x_%s_sb_engine_new" , name) ;
	
	if( ( func = (x_sb_engine_new_func_t) kik_dl_func_symbol( handle , symbol)) == NULL)
	{
		return  NULL ;
	}

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

	if( conf->engine_name == NULL)
	{
		free_conf( conf) ;
		
		return  NULL ;
	}

#ifdef __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "%s has been registered as new view. [dir: %s]\n" ,
		conf->sb_name , conf->dir);
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

static int
unregister_view_conf(
	x_sb_view_conf_t *  conf
	)
{
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

	return  1 ;
}

static x_sb_view_conf_t *
find_view_rcfile(
	char *  name
	)
{
	x_sb_view_conf_t *  conf ;
	kik_file_t *  rcfile ;
	char *  user_dir ;
	char *  path ;

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
		void *  p ;
		
		if( ! ( p = realloc( path , strlen( SB_DIR) + strlen( name) + 5)))
		{
			free( path) ;
			return  NULL ;
		}

		path = p ;

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
		x_sb_engine_new_func_t  func_engine ;
		
		if( ( func_engine = dlsym_sb_engine_new_func( conf->engine_name)) == NULL)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " %s scrollbar failed.\n" , name) ;
		#endif
		
			unregister_view_conf( conf) ;
			
			return  NULL ;
		}

		return  check_version( (*func_engine)( conf , 0)) ;
	}

	if( strcmp( name , "simple") == 0)
	{
		return  check_version( x_simple_sb_view_new()) ;
	}
	else if( ( func = dlsym_sb_view_new_func( name , 0)) == NULL)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " %s scrollbar failed.\n" , name) ;
	#endif
	
		return  NULL ;
	}

	return  check_version( (*func)()) ;
}

x_sb_view_t *
x_transparent_scrollbar_view_new(
	char *  name
	)
{
	x_sb_view_new_func_t  func ;
	x_sb_view_conf_t *  conf ;

	/* new style plugin? (requires an rcfile and an engine library) */
	if( ( conf = find_view_rcfile( name)))
	{
		x_sb_engine_new_func_t  func_engine ;
		
		if( ( func_engine = dlsym_sb_engine_new_func( conf->engine_name)) == NULL)
		{
			unregister_view_conf( conf) ;
			
			return  NULL ;
		}

		return  check_version( (*func_engine)( conf , 1)) ;
	}

	if( strcmp( name , "simple") == 0)
	{
		return  check_version( x_simple_transparent_sb_view_new()) ;
	}
	else if( ( func = dlsym_sb_view_new_func( name , 1)) == NULL)
	{
		return  NULL ;
	}

	return  check_version( (*func)()) ;
}

int
x_unload_scrollbar_view_lib(
	char *  name
	)
{
	x_sb_view_conf_t *  conf ;

	/* new style plugin? (requires an rcfile and an engine library) */
	if( ( conf = search_view_conf( name)))
	{
		/* lib@name@.so -> lib@engine_name@.so */
		name = conf->engine_name ;
	}

	/* remove unused conf */
	if( conf)
	{
		if( conf->use_count == 0)
		{
		#ifdef __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				" %s is no longer used. removing from view_conf_list\n", name);
		#endif

			unregister_view_conf( conf) ;
		}
	#ifdef __DEBUG
		else
		{
			kik_debug_printf( KIK_DEBUG_TAG " %s is still being used. [use_count: %d]\n" ,
				conf->use_count) ;
		}
	#endif
	}
		
	return  1 ;
}
