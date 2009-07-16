/*
 *	$Id$
 */

#ifndef  __MKF_MAP_LOADER_H__
#define  __MKF_MAP_LOADER_H__


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

#define  mkf_map_func(libname,funcname,bits) \
	static int (* _ ## funcname)( mkf_char_t *, u_int ## bits ## _t) ; \
	int funcname( mkf_char_t *  ch, u_int ## bits ## _t  ucscode) \
	{ \
		if( ! _ ## funcname) \
		{ \
			kik_dl_handle_t  handle ; \
			if( ( ! ( handle = kik_dl_open( MKFLIB_DIR, libname)) && \
			      ! ( handle = kik_dl_open( "", libname)) ) || \
			    ! ( _ ## funcname = kik_dl_func_symbol( handle, #funcname))) \
			{ \
				return  0 ; \
			} \
		} \
		return  (*_ ## funcname)( ch, ucscode) ; \
	}

#define  mkf_map_func2(libname,funcname) \
	static int (* _ ## funcname)( mkf_char_t *, mkf_char_t *) ; \
	int funcname( mkf_char_t *  dst_ch, mkf_char_t *  src_ch) \
	{ \
		if( ! _ ## funcname) \
		{ \
			kik_dl_handle_t  handle ; \
			if( ! ( handle = kik_dl_open( MKFLIB_DIR, libname)) || \
			    ! ( _ ## funcname = kik_dl_func_symbol( handle, #funcname))) \
			{ \
				return  0 ; \
			} \
		} \
		return  (*_ ## funcname)( dst_ch, src_ch) ; \
	}


#endif
