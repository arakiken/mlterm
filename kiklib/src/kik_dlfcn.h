/*
 *	$Id$
 */

#ifndef  __KIK_DLFCN_H__
#define  __KIK_DLFCN_H__


#include  "kik_config.h"


#if  defined(HAVE_DL_H)

#include  <dl.h>

typedef shl_t  kik_dl_handle_t ;

#elif  defined(HAVE_DLFCN_H)

#include  <dlfcn.h>

typedef void *  kik_dl_handle_t ;

#else

typedef void *  kik_dl_handle_t ;

#endif


kik_dl_handle_t  kik_dl_open( char *  path) ;

int  kik_dl_close( kik_dl_handle_t  handle) ;

void *  kik_dl_func_symbol( kik_dl_handle_t  handle , char *  symbol) ;


#endif
