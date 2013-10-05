/*
 *	$Id$
 */

#ifndef  __MKF_TBLFUNC_LOADER_H__
#define  __MKF_TBLFUNC_LOADER_H__


#include  <kiklib/kik_types.h>
#include  <kiklib/kik_dlfcn.h>

#include  "mkf_char.h"


#ifndef  LIBDIR
#define  MKFLIB_DIR  "/usr/local/lib/mkf/"
#else
#define  MKFLIB_DIR  LIBDIR "/mkf/"
#endif

#ifdef  DLFCN_NONE
#ifndef  NO_DYNAMIC_LOAD_TABLE
#define  NO_DYNAMIC_LOAD_TABLE
#endif
#endif


#ifndef  NO_DYNAMIC_LOAD_TABLE

#define  mkf_map_func(libname,funcname,bits) \
	int funcname( mkf_char_t *  ch, u_int ## bits ## _t  ucscode) \
	{ \
		static int (* _ ## funcname)( mkf_char_t *, u_int ## bits ## _t) ; \
		if( ! _ ## funcname && \
		    ! ( _ ## funcname = mkf_load_ ## libname ## _func( #funcname))) \
		{ \
			return  0 ; \
		} \
		return  (*_ ## funcname)( ch, ucscode) ; \
	}

#define  mkf_map_func2(libname,funcname) \
	int funcname( mkf_char_t *  dst_ch, mkf_char_t *  src_ch) \
	{ \
		static int (* _ ## funcname)( mkf_char_t *, mkf_char_t *) ; \
		if( ! _ ## funcname && \
		    ! ( _ ## funcname = mkf_load_ ## libname ## _func( #funcname))) \
		{ \
			return  0 ; \
		} \
		return  (*_ ## funcname)( dst_ch, src_ch) ; \
	}


void *  mkf_load_8bits_func( const char *  symname) ;

void *  mkf_load_jajp_func( const char *  symname) ;

void *  mkf_load_kokr_func( const char *  symname) ;

void *  mkf_load_zh_func( const char *  symname) ;


#endif	/* NO_DYNAMIC_LOAD_TABLE */


#endif
