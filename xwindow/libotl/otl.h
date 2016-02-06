/*
 *	$Id$
 */

#ifndef  __OTL_H__
#define  __OTL_H__


#include  <kiklib/kik_types.h>


#ifdef  NO_DYNAMIC_LOAD_OTL

#ifdef  USE_HARFBUZZ
#include  "hb.c"
#else
#include  "otf.c"
#endif

#else	/* NO_DYNAMIC_LOAD_OTL */

#include  <kiklib/kik_dlfcn.h>
#include  <kiklib/kik_debug.h>
#include  <stdio.h>

#ifndef  LIBDIR
#define  OTL_DIR  "/usr/local/lib/mlterm/"
#else
#define  OTL_DIR  LIBDIR "/mlterm/"
#endif


/* --- static variables --- */

static void *  (*open_sym)( void * , u_int) ;
static void  (*close_sym)( void *) ;
static u_int  (*convert_sym)( void * , u_int32_t * , u_int , int8_t * , u_int8_t * ,
		u_int32_t * , u_int32_t * , u_int , const char * , const char * , u_int) ;


/* --- static functions --- */

static void *
otl_open(
	void *  obj ,
	u_int  size
	)
{
	static int  is_tried ;

	if( ! is_tried)
	{
		kik_dl_handle_t  handle ;

		is_tried = 1 ;

		if( ( ! ( handle = kik_dl_open( OTL_DIR , "otl")) &&
		      ! ( handle = kik_dl_open( "" , "otl"))) ||
		    ! ( open_sym = kik_dl_func_symbol( handle , "otl_open")) ||
		    ! ( close_sym = kik_dl_func_symbol( handle , "otl_close")) ||
		    ! ( convert_sym = kik_dl_func_symbol( handle ,
					"otl_convert_text_to_glyphs")))
		{
			kik_error_printf( "libotl: Could not load.\n") ;

			if( handle)
			{
				kik_dl_close( handle) ;
			}

			return  NULL ;
		}
	}
	else if( ! open_sym)
	{
		return  NULL ;
	}

	return  (*open_sym)( obj , size) ;
}

static void
otl_close(
	void *  otf
	)
{
	return  (*close_sym)( otf) ;
}

static u_int
otl_convert_text_to_glyphs(
	void *  otf ,
	u_int32_t *  shaped ,
	u_int  shaped_len ,
	int8_t *  offsets ,
	u_int8_t *  widths ,
	u_int32_t *  cmapped ,
	u_int32_t *  src ,
	u_int  src_len ,
	const char *  script ,
	const char *  features ,
	u_int  fontsize
	)
{
	return  (*convert_sym)( otf , shaped , shaped_len , offsets , widths ,
			cmapped , src , src_len , script , features , fontsize) ;
}


#endif


#endif	/* __OTL_H__ */

