/*
 *	$Id$
 */

#ifndef  __KIK_DLFCN_H__
#define  __KIK_DLFCN_H__


#include  "kik_config.h"

typedef void *  kik_dl_handle_t ;

kik_dl_handle_t  kik_dl_open( char *  dirpath , char * name) ;

int  kik_dl_close( kik_dl_handle_t  handle) ;

void *  kik_dl_func_symbol( kik_dl_handle_t  handle , char *  symbol) ;

#endif
