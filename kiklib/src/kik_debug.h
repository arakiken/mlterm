/*
 *	$Id$
 */

#ifndef  __KIK_DEBUG_H__
#define  __KIK_DEBUG_H__


#include  "kik_config.h"


/* for kik_{debug|warn}_printf */
#ifdef  CONCATABLE_FUNCTION
#define  KIK_DEBUG_TAG  "[" __FUNCTION__ "()]"
#else
#define  KIK_DEBUG_TAG  ""
#endif


int  kik_debug_printf( const char *  format , ...) ;

int  kik_warn_printf( const char *  format , ...) ;

int  kik_error_printf( const char *  format , ...) ;

int  kik_msg_printf( const char *  format , ...) ;


#endif
