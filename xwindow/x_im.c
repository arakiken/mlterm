/*
 *	$Id$
 */

#include  <kiklib/kik_dlfcn.h>
#include  <kiklib/kik_str.h>	/* kik_str_sep, kik_snprintf */

#include  "x_im.h"
#include  "x_term_manager.h"

#ifndef  LIBDIR
#define  IM_DIR  "/usr/local/lib/mlterm/"
#else
#define  IM_DIR  LIBDIR "/mlterm/"
#endif


typedef  x_im_t * (*x_im_new_func_t)( u_int32_t  magic ,
				      ml_char_encoding_t  term_encoding ,
				      x_im_event_listener_t *  im_listener ,
				      x_im_export_syms_t *  syms ,
				      char *  engine) ;


/* --- static variables --- */

static  x_im_export_syms_t  im_export_syms =
{
	ml_str_init ,
	ml_str_delete ,
	ml_char_combine ,
	ml_char_set ,
	ml_get_char_encoding ,
	ml_is_msb_set ,
	ml_parser_new ,
	ml_conv_new ,
	x_im_candidate_screen_new ,
	x_term_manager_add_fd ,
	x_term_manager_remove_fd

} ;

/* --- static functions --- */

static x_im_new_func_t
dlsym_im_new_func(
	char *  im_name
	)
{
	kik_dl_handle_t  handle ;
	char * libname ;

	if( ! im_name)
	{
		return  NULL ;
	}

	if( ! ( libname = alloca( sizeof(char) * (strlen( im_name) + 4))))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  NULL ;
	}

	sprintf( libname , "im-%s" , im_name) ;

	if( ! ( handle = kik_dl_open( IM_DIR , libname)))
	{
		return  NULL ;
	}

	return  (x_im_new_func_t) kik_dl_func_symbol( handle , "im_new") ;
}


/* --- global functions --- */

x_im_t *
x_im_new(
	ml_char_encoding_t  term_encoding ,
	x_im_event_listener_t *  im_listener ,
	char *  input_method
	)
{
	x_im_t *  im ;
	x_im_new_func_t  func ;
	char *  im_name ;
	char *  im_attr ;

	if( strchr( input_method , ':'))
	{
		im_attr = strdup( input_method) ;

		if( ( im_name = kik_str_sep( &im_attr , ":")) == NULL)
		{
		#ifdef  DEBUG
			kik_error_printf( "%s is illegal input method.\n" , input_method) ;
		#endif

			return  NULL ;
		}
	}
	else
	{
		im_name = strdup( input_method) ;
		im_attr = NULL ;
	}

	if ( ! ( func = dlsym_im_new_func( im_name)))
	{
		free( im_name) ;

		return  NULL ;
	}

	if( ! ( im = (*func)( IM_API_COMPAT_CHECK_MAGIC ,
			      term_encoding ,
			      im_listener ,
			      &im_export_syms ,
			      im_attr)))
	{
		kik_error_printf( "Cound not open specified input method(%s)\n" , im_name) ;
	}

	free( im_name) ;

	return  im ;
}

