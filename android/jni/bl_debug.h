/*
 *	$Id$
 */

#ifndef __BL_DEBUG_H__
#define __BL_DEBUG_H__

#include "bl_def.h"
#include "bl_util.h" /* BL_INT_TO_STR */

#include <android/log.h>

/* for bl_{debug|warn}_printf */
#if 0

#ifdef CONCATABLE_FUNCTION
#define BL_DEBUG_TAG "[" __FUNCTION__ "()]"
#else
#define BL_DEBUG_TAG "[" __FILE__ "]"
#endif

#else

#define BL_DEBUG_TAG "[" __FILE__ ":" BL_INT_TO_STR(__LINE__) "]"

#endif

#ifdef BL_DEBUG

#define BL_TESTIT(func, args) TEST_##func args
#define BL_TESTIT_ONCE(func, args) \
  {                                \
    static int func##_tested;      \
    if (!func##_tested) {          \
      func##_tested = 1;           \
      TEST_##func args;            \
    }                              \
  }
#else

#define BL_TESTIT(func, args)
#define BL_TESTIT_ONCE(func, args)

#endif

#define bl_debug_printf(...) (__android_log_print(ANDROID_LOG_INFO, "", __VA_ARGS__) >= 0)

#define bl_warn_printf(...) (__android_log_print(ANDROID_LOG_WARN, "", __VA_ARGS__) >= 0)

#define bl_error_printf(...) (__android_log_print(ANDROID_LOG_ERROR, "", __VA_ARGS__) >= 0)

#define bl_msg_printf(...) (__android_log_print(ANDROID_LOG_INFO, "", __VA_ARGS__) >= 0)

#define bl_set_msg_log_file_name(name) (0)

#endif
