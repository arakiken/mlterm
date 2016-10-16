/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __BL_DEBUG_H__
#define __BL_DEBUG_H__

#include "bl_def.h"
#include "bl_util.h" /* BL_INT_TO_STR */

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

int bl_debug_printf(const char* format, ...);

int bl_warn_printf(const char* format, ...);

int bl_error_printf(const char* format, ...);

int bl_msg_printf(const char* format, ...);

int bl_set_msg_log_file_name(const char* name);

#endif
