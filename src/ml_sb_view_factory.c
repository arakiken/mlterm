/*
 *	$Id$
 */

#include  "ml_sb_view_factory.h"

#include  <stdio.h>		/* sprintf */
#include  <kiklib/kik_dlfcn.h>
#include  <kiklib/kik_mem.h>	/* alloca */

#include  "ml_simple_sb_view.h"


#ifndef  LIBDIR
#define  SBLIB_DIR  "/usr/local/lib/mlterm/"
#else
#define  SBLIB_DIR  LIBDIR "/mlterm/"
#endif


/* --- static variables --- */

static kik_dl_handle_t  prev_shlib_handle = NULL ;
static kik_dl_handle_t  cur_shlib_handle = NULL ;


/* --- static functions --- */

static ml_sb_view_new_func_t
dlsym_sb_view_new_func(
	char *  name ,
	int  is_transparent
	)
{
	char *  dir ;
	ml_sb_view_new_func_t  func ;
	kik_dl_handle_t  handle ;
	char *  libpath ;
	char *  symbol ;
	u_int  len ;

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
		kik_dl_close( handle) ;

		return  NULL ;
	}

	prev_shlib_handle = cur_shlib_handle ;
	cur_shlib_handle = handle ;

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
ml_unload_prev_scrollbar_view(void)
{
	if( prev_shlib_handle)
	{
		kik_dl_close( prev_shlib_handle) ;

		prev_shlib_handle = NULL ;
	}

	return  1 ;
}
