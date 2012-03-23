/*
 *	$Id$
 */

#ifndef  __KIK_DEBUG_H__
#define  __KIK_DEBUG_H__


#include  "kik_def.h"
#include  "kik_util.h"	/* KIK_INT_TO_STR */


/* for kik_{debug|warn}_printf */
#if  0

#ifdef  CONCATABLE_FUNCTION
#define  KIK_DEBUG_TAG  "[" __FUNCTION__ "()]"
#else
#define  KIK_DEBUG_TAG  "[" __FILE__ "]"
#endif

#else

#define  KIK_DEBUG_TAG  "[" __FILE__ ":" KIK_INT_TO_STR(__LINE__) "]"

#endif


int  kik_debug_printf( const char *  format , ...) ;

int  kik_warn_printf( const char *  format , ...) ;

int  kik_error_printf( const char *  format , ...) ;

int  kik_msg_printf( const char *  format , ...) ;

int  kik_set_msg_log_file_name( const char *  name) ;


#endif
