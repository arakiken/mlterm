/*
 *	$Id$
 */

#ifndef  __KIK_DEBUG_H__
#define  __KIK_DEBUG_H__


#include  "kik_def.h"
#include  "kik_util.h"	/* KIK_INT_TO_STR */

#include  <android/log.h>


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


#ifdef  KIK_DEBUG

#define  KIK_TESTIT(func, args)  TEST_ ## func args
#define  KIK_TESTIT_ONCE(func, args) \
	{ \
		static int  func ## _tested ; \
		if( ! func ## _tested) \
		{ \
			func ## _tested = 1 ; \
			TEST_ ## func args ; \
		} \
	}
#else

#define  KIK_TESTIT(func, args)
#define  KIK_TESTIT_ONCE(func, args)

#endif


#define  kik_debug_printf(...) \
	(__android_log_print( ANDROID_LOG_INFO , "" , __VA_ARGS__) >= 0)

#define  kik_warn_printf(...) \
	(__android_log_print( ANDROID_LOG_WARN , "" , __VA_ARGS__) >= 0)

#define  kik_error_printf(...) \
	(__android_log_print( ANDROID_LOG_ERROR , "" , __VA_ARGS__) >= 0)

#define  kik_msg_printf(...) \
	(__android_log_print( ANDROID_LOG_INFO , "" , __VA_ARGS__) >= 0)

#define kik_set_msg_log_file_name(name)  (0)


#endif
