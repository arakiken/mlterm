/*
 *	update: <2001/10/2(20:05:16)>
 *	$Id$
 */

#ifndef  __KIK_DEBUG_H__
#define  __KIK_DEBUG_H__


#include  "kik_def.h"


/* for kik_{debug|warn}_printf */
#define  KIK_DEBUG_TAG  "[" __FUNCTION__ "()]"


int  kik_debug_printf( const char *  format , ...) ;

int  kik_warn_printf( const char *  format , ...) ;

int  kik_error_printf( const char *  format , ...) ;

int  kik_msg_printf( const char *  format , ...) ;


#endif
